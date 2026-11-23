"""Compare incremental graphics edits with a forced full repaint in VRAM."""
from pathlib import Path
from binary_layout import data_base
import argparse, hashlib, json, os, re, subprocess

p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','boot-disk','profile-root','output'):
    p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--variants',nargs='+',default=['MAP'])
p.add_argument('--cpu',choices=['z80','r800'],default='r800')
a=p.parse_args(); root=Path(__file__).resolve().parents[1]; rt=a.runtime.resolve()
results=[]
for mode in a.variants:
    w=a.output.resolve()/mode; w.mkdir(parents=True,exist_ok=False)
    disk=w/'test.dsk'; disk.write_bytes(a.boot_disk.read_bytes())
    com=root/'bin'/('VI'+mode+'.COM')
    fixture=w/'TAIL.TXT'; fixture.write_bytes(b'abcdefghij\r\n'+ '日本語かな'.encode('cp932')+b'\r\n'+b'Z'*80+b'\r\nEND')
    listing=(root/'build'/(mode+'.lst')).read_text()
    def address(name,base):
        return int(re.search(r'([0-9A-F]{8})\s+\d+ _'+name+':',listing)[1],16)+base
    key=address('key',0x103); repaint=address('repaint',data_base(root,mode))
    signature=com.read_bytes()[key-0x100:key-0x100+12].hex()
    s=['set renderer none','set throttle off','set power on',
       'catch {diskmanipulator delete diska AUTOEXEC.BAT}','set tailPhase 0','set tailPath {}']
    for f in (com,fixture):
        s+=['catch {diskmanipulator delete diska '+f.name+'}', 'diskmanipulator import diska {'+f.as_posix()+'}']
    # Capture at key entry with editor RAM mapped, then repaint the same state.
    s+=['debug set_bp '+str(key)+' {$tailPhase && [binary encode hex [debug read_block memory '+str(key)+' 12]] eq "'+signature+'"} {'
        'set f [open ${tailPath}-${tailPhase}.bin wb];puts -nonewline $f [debug read_block VRAM 0 54272];close $f;'
        'if {$tailPhase == 1} {set tailPhase 2;debug write memory '+str(repaint)+' 1;after time 0 {type_via_keybuf [binary format H* 0c]}} else {set tailPhase 0}}']
    t=25; checks=[]
    def op(keys):
        global t
        s.append('after time '+str(t)+' {type_via_keybuf [binary format H* '+keys.hex()+']}');t+=18
    op(('VI'+mode+' TAIL.TXT\r').encode())
    op((':'+a.cpu+'\r').encode())
    for cols in (40,80):
        op((':mode'+str(cols)+'\r').encode())
        for label,keys in [('append',b'ggA123456789\x1b'),
                           ('backspace',b'A'+b'\b'*7+b'\x1b'),
                           ('delete',b'03x'),('middle',b'0iXY\x1b'),
                           ('wide-delete',b'j0x'),('wide-insert',b'i'+ '漢'.encode('cp932')+b'\x1b'),
                           ('split-join',b'A\r\b\x1b'),('delete-line',b'dd'),
                           ('paste',b'p'),('numbers',b':set number\r'),
                           ('numbered-delete',b'0x'),('nonumber',b':set nonumber\r')]:
            op(keys); name=str(cols)+'-'+label; checks.append(name)
            s.append('after time '+str(t)+' {set tailPath {'+(w/name).as_posix()+'};set tailPhase 1;type_via_keybuf [binary format H* 0c]}');t+=18
    op(b':q!\r');s+=['after time '+str(t)+' {exit}']
    script=w/'run.tcl';script.write_text('\n'.join(s))
    z=subprocess.run([rt/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',
        *(['-ext','V9968_VRAM4M'] if mode=='EXT' else []),'-diska',disk,'-script',script],
        input=b'<openmsx-control>\n',capture_output=True,timeout=300,
        env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(rt/'share'),
                 OPENMSX_USER_DATA=str(a.profile_root.resolve()/('Panasonic_FS-A1ST-'+mode)/'user'),
                 OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
    (w/'emulator.log').write_bytes(z.stdout+z.stderr);assert z.returncode==0,z.returncode
    for name in checks:
        first=(w/(name+'-1.bin')).read_bytes();second=(w/(name+'-2.bin')).read_bytes()
        assert first==second,(mode,name,'incremental pixels differ from full repaint')
    results.append(dict(variant=mode,cpu=a.cpu,comparisons=len(checks),incremental_equals_full_repaint=True,
                        sha256=hashlib.sha256(com.read_bytes()).hexdigest()))
    a.output.joinpath('results.json').write_text(json.dumps(results,indent=2))
    print(mode,'24 incremental/full repaint VRAM comparisons PASS',flush=True)
