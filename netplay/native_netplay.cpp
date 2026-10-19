// SPDX-License-Identifier: GPL-3.0-only
// Experimental Windows transport. The frame protocol can later run over the
// authenticated room's WebRTC channel without changing machine stepping.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "native_netplay.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace port {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t kMaxPacket = 4096, kMaxQueue = 16384;
constexpr uint32_t kProtocol = 1;
bool Pending() { const int e=WSAGetLastError(); return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS; }
void CloseSocket(SOCKET& s) { if(s!=INVALID_SOCKET) { closesocket(s); s=INVALID_SOCKET; } }
bool Nonblocking(SOCKET s) { u_long on=1; return ioctlsocket(s,FIONBIO,&on)==0; }
void U32(std::vector<uint8_t>& v,uint32_t n) { for(int i=3;i>=0;--i) v.push_back(static_cast<uint8_t>(n>>(i*8))); }
uint32_t Read32(const uint8_t* p) { return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]; }
void U64(std::vector<uint8_t>& v,uint64_t n) { U32(v,uint32_t(n>>32)); U32(v,uint32_t(n)); }
uint64_t Read64(const uint8_t* p) { return (uint64_t(Read32(p))<<32)|Read32(p+4); }
}
struct NetSession::Impl {
    SOCKET socket=INVALID_SOCKET, listener=INVALID_SOCKET;
    bool winsock=false, host=false, connected=false, helloSent=false, ready=false, sent=false, failed=false;
    uint64_t expected=0;
    uint16_t held=0;
    unsigned timeout=10000;
    std::string identity,error,status="Connecting";
    std::vector<uint8_t> tx,rx;
    Clock::time_point progress=Clock::now();
    FILE* log=nullptr;
    ~Impl() { CloseSocket(socket); CloseSocket(listener); if(winsock) WSACleanup(); if(log) fclose(log); }
    void Note(const char* event,uint64_t frame=0,uint32_t hash=0) {
        if(log) { fprintf(log,"%s %llu %08x\n",event,(unsigned long long)frame,hash); fflush(log); }
    }
    NetSession::Result Fail(const std::string& what) {
        if(!failed) { error=what; status=what; failed=true; Note(what.c_str(),expected); CloseSocket(socket); CloseSocket(listener); }
        return NetSession::Result::Failed;
    }
    bool Queue(const std::vector<uint8_t>& body) {
        if(body.empty() || body.size()>kMaxPacket || tx.size()+body.size()+4>kMaxQueue) { Fail("Netplay packet queue exceeded"); return false; }
        U32(tx,static_cast<uint32_t>(body.size())); tx.insert(tx.end(),body.begin(),body.end()); return true;
    }
    bool Pump() {
        while(!tx.empty()) {
            const int n=send(socket,reinterpret_cast<const char*>(tx.data()),static_cast<int>(tx.size()),0);
            if(n>0) tx.erase(tx.begin(),tx.begin()+n);
            else if(n==SOCKET_ERROR && Pending()) break;
            else { Fail("Netplay send failed / peer disconnected"); return false; }
        }
        // Bound work per poll as well as memory, keeping the SDL event loop responsive.
        for(int attempt=0;attempt<4;attempt++) {
            std::array<uint8_t,4096> bytes{};
            const int n=recv(socket,reinterpret_cast<char*>(bytes.data()),static_cast<int>(bytes.size()),0);
            if(n>0) {
                if(rx.size()+n>kMaxQueue) { Fail("Netplay receive queue exceeded"); return false; }
                rx.insert(rx.end(),bytes.begin(),bytes.begin()+n);
            } else if(n==SOCKET_ERROR && Pending()) break;
            else if(n==0 && !rx.empty()) break; // consume the final complete packet before noticing EOF
            else { Fail("Netplay peer disconnected"); return false; }
        }
        return true;
    }
    bool Packet(std::vector<uint8_t>& body) {
        if(rx.size()<4) return false;
        const uint32_t size=Read32(rx.data());
        if(!size || size>kMaxPacket) { Fail("Invalid netplay packet length"); return false; }
        if(rx.size()<size+4) return false;
        body.assign(rx.begin()+4,rx.begin()+4+size); rx.erase(rx.begin(),rx.begin()+4+size); return true;
    }
};
NetSession::NetSession():impl(std::make_unique<Impl>()) {}
NetSession::~NetSession()=default;
const std::string& NetSession::Error() const { return impl->error; }
const std::string& NetSession::Status() const { return impl->status; }
int NetSession::Player() const { return impl->host?0:1; }
bool NetSession::Open(const std::string& endpoint,bool host,const std::string& identity,unsigned timeoutMs,const std::string& logPath,std::string& error) {
    auto& s=*impl;
    auto fail=[&](const std::string& why) { s.Fail(why); error=why; return false; };
    if(endpoint.empty() || identity.empty() || identity.size()>kMaxPacket-16 || timeoutMs<100 || timeoutMs>60000)
        return fail("Invalid netplay endpoint, identity or timeout (100..60000 ms)");
    const size_t colon=endpoint.rfind(':');
    if(colon==std::string::npos) return fail("Netplay endpoint must be a numeric IPv4 address:port");
    const std::string address=endpoint.substr(0,colon), portText=endpoint.substr(colon+1);
    if(portText.empty() || portText.find_first_not_of("0123456789")!=std::string::npos || portText.size()>5) return fail("Invalid netplay port");
    const unsigned port=static_cast<unsigned>(std::stoul(portText));
    if(!port || port>65535) return fail("Invalid netplay port");
    WSADATA data{}; if(WSAStartup(MAKEWORD(2,2),&data)!=0) return fail("Cannot initialize Winsock");
    s.winsock=true; s.host=host; s.identity=identity; s.timeout=timeoutMs;
    if(!logPath.empty()) { s.log=fopen(logPath.c_str(),"w"); if(!s.log) return fail("Cannot create netplay log"); }
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(static_cast<u_short>(port));
    if(inet_pton(AF_INET,address.c_str(),&addr.sin_addr)!=1) return fail("Use a numeric IPv4 address for netplay");
    SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(sock==INVALID_SOCKET) return fail("Cannot create netplay socket");
    s.socket=sock;
    if(!Nonblocking(sock)) return fail("Cannot configure netplay socket");
    if(host) {
        const BOOL exclusive=TRUE;
        if(setsockopt(sock,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive)) ||
           bind(sock,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) || listen(sock,1)) return fail("Cannot listen on netplay endpoint");
        s.listener=s.socket; s.socket=INVALID_SOCKET; s.status="Waiting for player 2";
    } else {
        const int result=connect(sock,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));
        if(result==SOCKET_ERROR && !Pending()) return fail("Cannot connect to netplay host");
        s.connected=result==0;
    }
    s.progress=Clock::now(); s.Note(host?"listen":"connect"); return true;
}
NetSession::Result NetSession::Poll(uint64_t frame,uint16_t local,uint32_t hash,uint16_t pads[2],bool finish) {
    auto& s=*impl;
    if(s.failed) return Result::Failed;
    if(frame!=s.expected) return s.Fail("Invalid local netplay frame");
    if(Clock::now()-s.progress>std::chrono::milliseconds(s.timeout)) return s.Fail("Netplay timed out waiting for peer");
    if(!s.connected) {
        if(s.host) {
            s.socket=accept(s.listener,nullptr,nullptr);
            if(s.socket==INVALID_SOCKET) return Pending()?Result::Waiting:s.Fail("Netplay accept failed");
            CloseSocket(s.listener);
            if(!Nonblocking(s.socket)) return s.Fail("Cannot configure accepted netplay socket");
            s.connected=true;
        } else {
            fd_set ready,except; FD_ZERO(&ready); FD_ZERO(&except); FD_SET(s.socket,&ready); FD_SET(s.socket,&except);
            timeval zero{};
            if(select(0,nullptr,&ready,&except,&zero)==SOCKET_ERROR) return s.Fail("Netplay connect polling failed");
            if(!FD_ISSET(s.socket,&ready) && !FD_ISSET(s.socket,&except)) return Result::Waiting;
            int error=0,length=sizeof(error);
            if(getsockopt(s.socket,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&length) || error) return s.Fail("Netplay connection refused");
            s.connected=true;
        }
        s.progress=Clock::now();
    }
    if(!s.helloSent && s.tx.empty()) {
        // Hello is queued once; its type differs from frame packets.
        const BOOL nodelay=TRUE; setsockopt(s.socket,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&nodelay),sizeof(nodelay));
        std::vector<uint8_t> body{1}; U32(body,kProtocol); body.push_back(static_cast<uint8_t>(Player()));
        body.insert(body.end(),s.identity.begin(),s.identity.end()); s.Queue(body); s.helloSent=true;
    }
    if(!s.Pump()) return Result::Failed;
    if(!s.ready) {
        std::vector<uint8_t> body;
        if(!s.Packet(body)) return s.failed?Result::Failed:Result::Waiting;
        if(body.size()<6 || body[0]!=1 || Read32(body.data()+1)!=kProtocol || body[5]!=1-Player()) return s.Fail("Incompatible netplay protocol or player roles");
        if(std::string(body.begin()+6,body.end())!=s.identity) return s.Fail("Netplay build, ROM, settings or initial state mismatch");
        s.ready=true; s.status="Running"; s.Note("ready",frame,hash); s.progress=Clock::now();
    }
    if(!s.sent) {
        if(local&~0x0FFF) return s.Fail("Invalid local netplay input bits");
        s.held=local;
        std::vector<uint8_t> body{static_cast<uint8_t>(finish?3:2)};
        U64(body,frame); U32(body,local); U32(body,hash);
        if(!s.Queue(body)) return Result::Failed;
        s.sent=true;
    }
    if(!s.Pump()) return Result::Failed;
    // Flush the entire local packet before releasing its frame to the machine.
    if(!s.tx.empty()) return Result::Waiting;
    std::vector<uint8_t> body;
    if(!s.Packet(body)) return s.failed?Result::Failed:Result::Waiting;
    if(body.size()!=17 || body[0]!=(finish?3:2) || Read64(body.data()+1)!=frame) return s.Fail("Unexpected netplay message or frame order");
    const uint32_t remote=Read32(body.data()+9);
    if(remote&~0x0FFF) return s.Fail("Invalid remote netplay input bits");
    if(Read32(body.data()+13)!=hash) return s.Fail("Netplay state divergence detected");
    pads[Player()]=s.held; pads[1-Player()]=static_cast<uint16_t>(remote);
    s.Note(finish?"complete":"frame",frame,hash);
    s.expected++; s.sent=false; s.progress=Clock::now(); return Result::Ready;
}
std::string NetExecutableHash(std::string& error) {
    wchar_t path[32768]; const DWORD length=GetModuleFileNameW(nullptr,path,32768);
    if(!length || length>=32768) { error="Cannot locate netplay executable"; return {}; }
    FILE* file=_wfopen(path,L"rb");
    if(!file) { error="Cannot read netplay executable"; return {}; }
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    std::array<unsigned char,32> digest{};
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 &&
            BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    std::array<unsigned char,65536> bytes{};
    while(ok) {
        const size_t n=fread(bytes.data(),1,bytes.size(),file);
        if(n && BCryptHashData(hash,bytes.data(),static_cast<ULONG>(n),0)<0) { ok=false; break; }
        if(n<bytes.size()) { ok=!ferror(file); break; }
    }
    if(ok) ok=BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
    fclose(file); if(hash) BCryptDestroyHash(hash); if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok) { error="Cannot hash netplay executable"; return {}; }
    std::string result; constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:digest) { result+=hex[c>>4]; result+=hex[c&15]; }
    return result;
}
}
