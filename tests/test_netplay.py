# SPDX-License-Identifier: GPL-3.0-only
"""Native synchronization regression tests. Run from the repo root:
python tests/test_netplay.py [--frames 6000] [--protocol-only]
Requires a built game, local roms/ssf2xj.zip + ssf2t.zip, MSYS2 UCRT64.
All processes are hidden, all listeners are loopback, and all outputs stay in _build.
No server, database, production settings, or ROM contents are modified.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'_build/netplay-tests'
ENV=os.environ.copy()
ENV['PATH']='C:/msys64/ucrt64/bin;'+ENV['PATH']
EXE=ROOT/'_build/cps2recomp/build/ssf2xj/ssf2xj.exe'
FLAGS=subprocess.CREATE_NO_WINDOW
RESULTS=[]

def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]

def spawn(args,name):
    f=(OUT/(name+'.stderr')).open('w')
    try:
        return subprocess.Popen([str(a) for a in args],cwd=ROOT,env=ENV,stdout=f,stderr=f,creationflags=FLAGS)
    finally:f.close()

def wait(p,timeout=30):
    try:return p.wait(timeout)
    except subprocess.TimeoutExpired:
        p.kill();p.wait();raise AssertionError('Process hung beyond deadline')

def packet(body):return struct.pack('!I',len(body))+body

def exact(s,n):
    result=b''
    while len(result)<n:
        b=s.recv(n-len(result))
        if not b:raise AssertionError('Unexpected EOF')
        result+=b
    return result

def receive(s):
    size=struct.unpack('!I',exact(s,4))[0]
    assert 0<size<=4096
    return exact(s,size)

def dial(number):
    deadline=time.monotonic()+3
    while True:
        try:
            s=socket.create_connection(('127.0.0.1',number),timeout=2);s.settimeout(3);return s
        except ConnectionRefusedError:
            if time.monotonic()>deadline:raise
            time.sleep(.01)

def protocol_tests():
    harness=OUT/'netplay-protocol.exe'
    subprocess.run(['C:/msys64/ucrt64/bin/g++.exe','-std=c++20','-O2','-Inetplay',
        'tests/netplay-protocol.cpp','netplay/native_netplay.cpp','-lws2_32','-lbcrypt','-o',str(harness)],
        cwd=ROOT,env=ENV,check=True,creationflags=FLAGS)
    def case(name,action,expected=5,timeout=500,limit=3):
        number=port();p=spawn([harness,f'127.0.0.1:{number}',timeout,limit],name)
        try:
            with dial(number) as s:
                hello=receive(s);assert hello==b'\x01'+struct.pack('!I',1)+b'\x00test-runtime'
                action(s)
            code=wait(p,5)
            assert code==expected,(name,code,(OUT/(name+'.stderr')).read_text())
        finally:
            if p.poll() is None:p.kill();p.wait()
        RESULTS.append({'test':name,'passed':True})
    hello=b'\x01'+struct.pack('!I',1)+b'\x01test-runtime'
    def good(s):
        # Deliberately split every header, hello and frame payload across writes.
        for byte in packet(hello):s.sendall(bytes([byte]))
        sent=[]
        for frame in range(4):
            body=receive(s);kind,got,local,checksum=struct.unpack('!BQII',body)
            assert kind==(3 if frame==3 else 2) and got==frame and checksum==frame^0x12345678
            sent.append((frame,local,0x70 if frame%2 else 0x380))
            if frame==1:time.sleep(.08)
            response=packet(struct.pack('!BQII',kind,frame,0x70 if frame%2 else 0x380,checksum))
            for offset in range(0,len(response),3):s.sendall(response[offset:offset+3])
        time.sleep(.05)
        good.sent=sent
    case('fragmented-delayed-final-confirmation',good,0,1500)
    confirmed=[tuple(map(int,line.split())) for line in (OUT/'fragmented-delayed-final-confirmation.stderr').read_text().splitlines()]
    assert confirmed==good.sent,'A local input changed after it was sent'
    def reject_frame(s,change):
        s.sendall(packet(hello));kind,frame,mask,checksum=struct.unpack('!BQII',receive(s))
        s.sendall(packet(change(kind,frame,mask,checksum)))
        time.sleep(.1)
    case('bad-input-bits',lambda s:reject_frame(s,lambda k,f,m,h:struct.pack('!BQII',k,f,0x1000,h)))
    case('future-frame',lambda s:reject_frame(s,lambda k,f,m,h:struct.pack('!BQII',k,f+1,m,h)))
    case('state-divergence',lambda s:reject_frame(s,lambda k,f,m,h:struct.pack('!BQII',k,f,m,h^1)))
    case('unexpected-message',lambda s:reject_frame(s,lambda k,f,m,h:struct.pack('!BQII',3,f,m,h)))
    case('oversized-packet',lambda s:(s.sendall(struct.pack('!I',4097)),time.sleep(.1)))
    case('zero-length-packet',lambda s:(s.sendall(struct.pack('!I',0)),time.sleep(.1)))
    case('wrong-protocol',lambda s:(s.sendall(packet(b'\x01'+struct.pack('!I',2)+b'\x01test-runtime')),time.sleep(.1)))
    case('wrong-role',lambda s:(s.sendall(packet(b'\x01'+struct.pack('!I',1)+b'\x00test-runtime')),time.sleep(.1)))
    case('wrong-runtime',lambda s:(s.sendall(packet(hello+b'x')),time.sleep(.1)))
    case('truncated-packet-disconnect',lambda s:s.sendall(packet(hello)[:8]))
    case('peer-disconnect',lambda s:None)
    case('silent-peer-timeout',lambda s:time.sleep(.7))
    def duplicate(s):
        s.sendall(packet(hello));k,f,m,h=struct.unpack('!BQII',receive(s))
        reply=packet(struct.pack('!BQII',k,f,m,h));s.sendall(reply+reply)
        time.sleep(.1)
    case('duplicate-frame',duplicate)
    # Refusing a malformed local endpoint should not open any listener.
    p=spawn([harness,'not-an-endpoint',500,1],'invalid-endpoint');assert wait(p)==3
    RESULTS.append({'test':'invalid-endpoint','passed':True})

def game_tests(frames):
    # Freeze a read-only copy of the user's EEPROM. Even the offline reference
    # must not write to userdata/. A blank EEPROM works if the user has none.
    eeprom=OUT/'initial-eeprom.bin'
    if (ROOT/'userdata/eeprom.bin').exists():shutil.copyfile(ROOT/'userdata/eeprom.bin',eeprom)
    else:eeprom.write_bytes(b'\xff'*128)
    original=eeprom.read_bytes()
    source=ROOT/'ssf2xj/movies/versus-win.txt'
    for role in (0,1):
        lines=[]
        for line in source.read_text().splitlines():
            if not line or line.startswith('#'):continue
            bits=line.split();bits[2 if role==0 else 1]='0fff';lines.append(' '.join(bits))
        (OUT/f'player{role+1}.txt').write_text('\n'.join(lines)+'\n')
    common=[EXE,'--rom',ROOT/'roms/ssf2xj.zip','--headless','--frames',frames,'--features','arcade_timing','--eeprom',eeprom]
    number=port();processes=[]
    try:
        for role in (0,1):
            processes.append(spawn(common+['--movie',OUT/f'player{role+1}.txt','--hash-log',OUT/f'player{role+1}.hash',
                '--record',OUT/f'player{role+1}.record','--net-log',OUT/f'player{role+1}.net',
                '--net-host' if role==0 else '--net-join',f'127.0.0.1:{number}'],'game'+str(role+1)))
            time.sleep(.2)
        assert [wait(p,120) for p in processes]==[0,0],[(OUT/f'game{i}.stderr').read_text() for i in (1,2)]
    finally:
        for p in processes:
            if p.poll() is None:p.kill();p.wait()
    hashes=(OUT/'player1.hash').read_bytes()
    assert hashes==(OUT/'player2.hash').read_bytes() and len(hashes.splitlines())==frames
    assert (OUT/'player1.record').read_bytes()==(OUT/'player2.record').read_bytes()
    for role in (1,2):assert f'complete {frames} ' in (OUT/f'player{role}.net').read_text()
    assert eeprom.read_bytes()==original,'Online session changed EEPROM'
    # Compare the network run against the ordinary offline execution, frame by frame.
    p=spawn(common+['--movie',source,'--hash-log',OUT/'offline.hash'],'offline')
    assert wait(p,120)==0
    assert hashes==(OUT/'offline.hash').read_bytes(),'Network input composition differs from offline'
    RESULTS.append({'test':'two-native-processes-and-offline-reference','passed':True,'frames':frames,
        'hashLogSha256':hashlib.sha256(hashes).hexdigest(),'lastFrame':hashes.splitlines()[-1].decode()})
    # Different settings must fail before the first frame, on both processes.
    number=port()
    a=spawn(common+['--net-host',f'127.0.0.1:{number}','--net-timeout',1500,'--hash-log',OUT/'mismatch1.hash'],'mismatch1')
    time.sleep(.2)
    b=spawn(common+['--net-join',f'127.0.0.1:{number}','--features','-arcade_timing','--net-timeout',1500,'--hash-log',OUT/'mismatch2.hash'],'mismatch2')
    assert wait(a,10)==5 and wait(b,10)==5
    assert not (OUT/'mismatch1.hash').read_bytes() and not (OUT/'mismatch2.hash').read_bytes()
    RESULTS.append({'test':'native-settings-mismatch-before-first-frame','passed':True})
    # Mutation commands cannot be used on one peer behind the session's back.
    for option,value in [('--keys','1:p'),('--poke','ff8000=01'),('--state-load','0:missing.bin'),('--auto','1')]:
        p=spawn(common+['--net-host',f'127.0.0.1:{port()}',option,value],'reject-'+option[2:])
        assert wait(p,10)==1
    RESULTS.append({'test':'native-local-mutations-blocked','passed':True})

def window_test():
    """Exercise RunWindowed and its pending-frame path, using SDL's dummy devices."""
    number=port();processes=[]
    old={key:ENV.get(key) for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','CPS2RECOMP_HIDDEN_WINDOW')}
    ENV.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',CPS2RECOMP_HIDDEN_WINDOW='1')
    try:
        common=[EXE,'--rom',ROOT/'roms/ssf2xj.zip','--frames',120,'--features','arcade_timing',
            '--renderer','sdl','--unthrottled','--mute']
        for role in (0,1):
            processes.append(spawn(common+['--net-host' if role==0 else '--net-join',f'127.0.0.1:{number}',
                '--net-log',OUT/f'window{role+1}.net','--hash-log',OUT/f'window{role+1}.hash',
                '--shot-every',f'120:{OUT}/window{role+1}-'],'window'+str(role+1)))
            time.sleep(.2)
        assert [wait(p,15) for p in processes]==[0,0]
        assert (OUT/'window1.hash').read_bytes()==(OUT/'window2.hash').read_bytes()
        for role in (1,2):
            assert 'complete 120 ' in (OUT/f'window{role}.net').read_text()
            assert (OUT/f'window{role}-000120.png').exists()
        RESULTS.append({'test':'sdl-window-loop-software-renderer','passed':True,'frames':120})
    finally:
        for p in processes:
            if p.poll() is None:p.kill();p.wait()
        for key,value in old.items():
            if value is None:ENV.pop(key,None)
            else:ENV[key]=value

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--frames',type=int,default=6000);parser.add_argument('--protocol-only',action='store_true');parser.add_argument('--window-only',action='store_true')
    args=parser.parse_args();assert 1<=args.frames<=100000
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.window_only:protocol_tests()
    if not args.protocol_only:
        if not args.window_only:game_tests(args.frames)
        window_test()
    report='window-results.json' if args.window_only else 'protocol-results.json' if args.protocol_only else 'results.json'
    (OUT/report).write_text(json.dumps(RESULTS,indent=2)+'\n')
    print(json.dumps(RESULTS,indent=2))
