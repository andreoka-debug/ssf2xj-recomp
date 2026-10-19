// SPDX-License-Identifier: GPL-3.0-only
#include "port.h"
#include "../runtime/game.h"
#include <SDL3/SDL.h>
#include <zlib.h>
#include <sstream>
using namespace cps2recomp;
namespace port {
uint32_t Port::NetHash() const {
    // Canonical game checksum plus EEPROM. No pointers or struct padding are
    // included; controls are carried separately in the next frame message.
    uLong hash=StateHash();
    hash=crc32(hash,reinterpret_cast<const Bytef*>(machine->eeprom.words),sizeof(machine->eeprom.words));
    return static_cast<uint32_t>(hash);
}
bool Port::InitNetplay(std::string& error) {
    if(opt.netHost.empty() && opt.netJoin.empty()) return true;
    if((!opt.netHost.empty() && !opt.netJoin.empty()) || opt.autoPlay || !opt.keys.empty() || !opt.poke.empty() ||
       !opt.stateSave.empty() || !opt.stateLoad.empty() || !opt.menuTest.empty() || opt.soundTest || opt.musicTest ||
       opt.testTo || !opt.dumpProgram.empty() || opt.cpuSpeed!=1 || opt.hashPicture || opt.hashGame || opt.checkRewrites ||
       !opt.recordEntries.empty()) {
        error="Netplay requires exactly one host/join endpoint and normal gameplay; debug mutations and local state commands are unavailable.";
        return false;
    }
    // Session-only overrides: never write the player's saved offline preferences.
    std::string choices=opt.features;
    for(const auto& feature:TheGame().features) {
        const std::string name=feature.name;
        if(name=="run_ahead" || name=="training_mode" || (feature.flags & kFeatureTraining)) choices+=",-"+name;
    }
    LoadFeatures(opt.settingsPath,choices);
    machine->SetBoardTiming(FeatureOn("arcade_timing"));
    std::ostringstream identity;
    const std::string build=NetExecutableHash(error);
    if(build.empty()) return false;
    identity << "ssf2xj-native-lockstep-v1\n" << build << '\n' << opt.cpu << ':' << opt.sound << ':' << opt.noRewrites << ':' << opt.frames << '\n';
    // ROMs are already validated by the loader. Include every loaded region as
    // well, so different program patches cannot join the same session.
    for(const auto* region:{&roms.program,&roms.opcodes,&roms.graphics,&roms.soundProgram,&roms.samples})
        identity << crc32(0,region->data(),static_cast<uInt>(region->size())) << ':' << region->size() << '\n';
    for(const auto& feature:TheGame().features) identity << feature.name << '=' << FeatureSetting(feature.name) << '\n';
    netHash=NetHash();
    identity << "initial=" << netHash;
    netplay=std::make_unique<NetSession>();
    if(!netplay->Open(opt.netHost.empty()?opt.netJoin:opt.netHost,!opt.netHost.empty(),identity.str(),opt.netTimeout,opt.netLog,error)) return false;
    fprintf(stderr,"Netplay P%d: %s\n",netplay->Player()+1,netplay->Status().c_str());
    return true;
}
bool Port::NetTick() {
    uint16_t local=pad[0]; // Every PC uses its local P1 keyboard/first gamepad layout.
    if(!opt.moviePlay.empty()) {
        auto it=playMovie.changes.upper_bound(frame);
        if(it==playMovie.changes.begin()) local=0;
        else { --it; local=netplay->Player()==0?it->second.first:it->second.second; }
    }
    const auto result=netplay->Poll(frame,local,netHash,pad);
    if(result==NetSession::Result::Failed) {
        netError=netplay->Error(); quit=true;
        fprintf(stderr,"%s (frame %llu)\n",netError.c_str(),(unsigned long long)frame);
        return false;
    }
    if(result==NetSession::Result::Waiting) return false;
    service=test=false; coinFrames=start2Frames=testFrames=0;
    RunFrame();
    netHash=NetHash();
    return true;
}
bool Port::EndNetplay() {
    if(!netplay || !netError.empty()) return netError.empty();
    if(!opt.frames || frame!=opt.frames) return true; // user exit closes the peer's socket
    // Confirm the last frame too, before either process can report success.
    uint16_t pads[2]{};
    for(;;) {
        const auto result=netplay->Poll(frame,0,netHash,pads,true);
        if(result==NetSession::Result::Ready) return true;
        if(result==NetSession::Result::Failed) { netError=netplay->Error(); fprintf(stderr,"%s\n",netError.c_str()); return false; }
        SDL_Delay(1);
    }
}
}
