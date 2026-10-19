# SPDX-License-Identifier: GPL-3.0-only
"""Reproduce Honda's side flicker with the real game and verify the correction.
Run with the bundled Python (Pillow and NumPy), after build.ps1:
    python tests/test_honda_wide.py
Local ROMs are required. All fixtures and screenshots stay in _build/honda-regression.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'_build/honda-regression'
EXE=ROOT/'_build/cps2recomp/build/ssf2xj/ssf2xj.exe'
ENV=os.environ.copy();ENV['PATH']='C:/msys64/ucrt64/bin;'+ENV['PATH']
BASE='arcade_timing,training_stage=2,training_character=1,-training_input_display,-training_combo_readout,-training_hitboxes'

def run(name,extra):
    args=[EXE,'--rom',ROOT/'roms/ssf2xj.zip','--headless']+extra
    with (OUT/f'{name}.stderr').open('w') as log:
        p=subprocess.Popen([str(x) for x in args],cwd=ROOT,env=ENV,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
        try:code=p.wait(90)
        except subprocess.TimeoutExpired:p.kill();p.wait();raise
    assert code==0,(name,(OUT/f'{name}.stderr').read_text())

OUT.mkdir(parents=True,exist_ok=True)
eeprom=OUT/'eeprom.bin'
if (ROOT/'userdata/eeprom.bin').exists():shutil.copyfile(ROOT/'userdata/eeprom.bin',eeprom)
else:eeprom.write_bytes(b'\xff'*128)
boot=OUT/'boot.txt';boot.write_text('0 0000 0000\n2300 0010 0000\n2304 0000 0000\n')
run('boot',['--eeprom',eeprom,'--frames',3000,'--features',BASE+',display_shape=3','--keys','0:ctraining_start',
    '--movie',boot,'--state-save',f'3000:{OUT}/start.state','--dump-ram',f'3000:{OUT}/boot.ram'])
ram=(OUT/'boot.ram').read_bytes();assert ram[0x8c4f]==1 and ram[0x8009]==10,'Fixture did not reach Honda fight'
walk=OUT/'walk.txt';walk.write_text('0 0002 0002\n200 0001 0001\n550 0002 0002\n')
for name,shape in [('wide',3),('native',4),('original',0)]:
    run(name,['--frames',650,'--features',BASE+f',training_mode=1,display_shape={shape}',
        '--state-load',f'0:{OUT}/start.state','--movie',walk,'--shot-every',f'1:{OUT}/{name}-'])
last=None;states=set();gap_pixels=0
for frame in range(2,651):
    wide=np.array(Image.open(OUT/f'wide-{frame:06}.png').convert('RGB'))
    native=np.array(Image.open(OUT/f'native-{frame:06}.png').convert('RGB'))
    original=np.array(Image.open(OUT/f'original-{frame:06}.png').convert('RGB'))
    assert wide.shape==(224,512,3) and original.shape==(224,384,3)
    assert np.array_equal(wide[:,64:448],original),f'Original 4:3 area changed at frame {frame}'
    # Safe wide may cover only outside columns, and must never synthesize art.
    changed=np.any(wide!=native,axis=2)
    assert np.all(wide[changed]==0),f'Non-black synthesized pixels at frame {frame}'
    columns=np.all(wide==0,axis=(0,2))
    left=int(columns[:64].sum());right=int(columns[448:].sum())
    assert np.all(columns[:left]) and np.all(columns[512-right:])
    assert not np.any(columns[64:448]),'Black mask entered original viewport'
    if last:assert max(abs(left-last[0]),abs(right-last[1]))<=8,f'Edge jumped at frame {frame}'
    states.add((left,right));last=(left,right)
    # Unpainted upper side strips in the raw raster must be hidden consistently
    # across the full column, rather than intermittently mirrored into patches.
    gaps=np.all(native[:180]==0,axis=2);gaps[:,64:448]=False
    assert np.all(wide[:180][gaps]==0),f'Uncovered upper side gap at frame {frame}'
    gap_pixels+=int(gaps.sum())
assert (64,0) in states and (0,64) in states and (0,0) in states,'Camera did not traverse both edges and the center'
assert gap_pixels>0,'Fixture did not exercise missing side art'
# Keep the real Super Combo finish working while rejecting the bathhouse's
# bright mural during ordinary movement. Patch only this locally generated,
# trusted save-state fixture; never the ROM or the user's saves.
state=bytearray((OUT/'start.state').read_bytes())
at=state.find(ram)
assert at>=0 and state.find(ram,at+1)<0,'Could not locate the saved RAM fixture'
for address in (0x8878,0x887a,0x8a0a):state[at+address:at+address+2]=b'\x00\x01'
state[at+0x8702]=0x30
(OUT/'super.state').write_bytes(state)
(OUT/'super.txt').write_text('0 0001 0002\n50 0000 0000\n100 0004 0000\n103 0005 0000\n106 0001 0000\n109 0004 0000\n112 0005 0000\n115 0011 0000\n120 0000 0000\n')
for name,shape in [('superwide',3),('supernative',4)]:
    run(name,['--frames',210,'--features',BASE+f',training_mode=1,-training_health,display_shape={shape}',
        '--state-load',f'0:{OUT}/super.state','--movie',OUT/'super.txt','--shot-every',f'1:{OUT}/{name}-',
        '--trace',f'ff8878-ff887b:{OUT}/{name}.life'])
life=(OUT/'superwide.life').read_bytes();burst_frames=[]
for frame in range(2,211):
    wide=np.array(Image.open(OUT/f'superwide-{frame:06}.png').convert('RGB'))
    native=np.array(Image.open(OUT/f'supernative-{frame:06}.png').convert('RGB'))
    current=int.from_bytes(life[(frame-1)*4:(frame-1)*4+2],'big',signed=True)
    previous=int.from_bytes(life[(frame-1)*4+2:frame*4],'big',signed=True)
    changed=np.any(wide!=native,axis=2);changed[:,64:448]=False
    expanded=np.any(wide[changed]!=0)
    if current>0:assert not expanded,f'False Super finish while alive at frame {frame}'
    if expanded:
        assert current<=0
        burst_frames.append(frame)
        # In this real KO, the delayed value still says 1 while current life is
        # -1. Checking the delayed word would incorrectly suppress the effect.
        assert previous==1
assert burst_frames,'Real Super Combo KO did not extend into the wide columns'
report={'frames_checked':649,'original_viewport_preserved':True,'no_synthesized_side_art':True,
    'camera_edges_and_center_exercised':True,'upper_gap_pixels_covered':gap_pixels,'maximum_edge_step':8,
    'real_super_finish_extended_frames':burst_frames}
(OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
