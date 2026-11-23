"""Convert a Japanese sentence with the installed MSX-JE and save it.
Uses BIOS key-buffer input plus a real Ctrl+Space keyboard-matrix chord.
"""
from pathlib import Path
import argparse,os,subprocess,json,hashlib
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--runtime',type=Path,required=True)
ap.add_argument('--profile-dir',type=Path,required=True)
ap.add_argument('--boot-disk',type=Path,required=True)
ap.add_argument('--output',type=Path,required=True)
ap.add_argument('--variant',choices=['MAP','INT','EXT','9IN','9EX'],required=True)
ap.add_argument('--columns',choices=[40,80],type=int,default=40)
args=ap.parse_args();root=Path(__file__).resolve().parents[1]
w=args.output.resolve();w.mkdir(parents=True,exist_ok=False)
r=args.runtime.resolve();profile=args.profile_dir.resolve();mode=args.variant
disk=w/'test.dsk';disk.write_bytes(args.boot_disk.read_bytes())
com=root/'bin'/('VI'+mode+'.COM')
script=['set renderer SDLGL-PP','set throttle off','set maxframeskip 0','set power on',
        'catch {diskmanipulator delete diska AUTOEXEC.BAT}',
        'catch {diskmanipulator delete diska GREET.TXT}',
        'catch {diskmanipulator delete diska '+com.name+'}',
        'diskmanipulator import diska {'+com.as_posix()+'}']
if mode=='9EX':script+=['after time 1 {set videosource V9968}']
def type_at(t,text):script.append('after time '+str(t)+' {type_via_keybuf [binary format H* '+text.encode().hex()+']}')
def chord(t):
 script.append('after time '+str(t)+' {keymatrixdown 6 2;keymatrixdown 8 1}')
 script.append('after time '+str(t+.2)+' {keymatrixup 8 1;keymatrixup 6 2}')
type_at(25,'VI'+mode+' GREET.TXT\r')
if args.columns==80:type_at(32,':mode80\r')
type_at(45,':kanji\r');type_at(65,'i')
chord(75);type_at(80,'konnbann');type_at(90,' ')
script+=['after time 94 {screenshot -raw {'+(w/'candidate.png').as_posix()+'}}']
type_at(100,'\r');type_at(110,'ha,konnnichiha.');type_at(125,'\r');chord(135)
type_at(140,'\x1b')
script+=['after time 148 {screenshot -raw {'+(w/'sentence.png').as_posix()+'}}']
type_at(155,':wq\r');script+=['after time 180 {exit}']
sc=w/'run.tcl';sc.write_text('\n'.join(script))
extensions=['vimSX_DOS2'] if (profile/'user/extensions/vimSX_DOS2.xml').exists() else []
if mode in ('EXT','9EX'):extensions.append('V9968_VRAM4M')
p=subprocess.run([r/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',
    *[s for e in extensions for s in ('-ext',e)],'-diska',disk,'-script',sc],
    input=b'<openmsx-control>\n',capture_output=True,timeout=90,
    env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(r/'share'),OPENMSX_USER_DATA=str(profile/'user'),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
(w/'emulator.log').write_bytes(p.stdout+p.stderr);assert p.returncode==0,p.returncode
ns={'Path':Path};source=(root/'tests/regression.py').read_text(encoding='utf8')
exec(source[source.index('def files('):source.index('for mode in')],ns)
actual=ns['files'](disk).get('GREET.TXT');expected='今晩は、こんにちは。'.encode('cp932')
(w/'result.json').write_text(json.dumps(dict(variant=mode,columns=args.columns,actual=actual.hex() if actual else None,expected=expected.hex(),match=actual==expected,sha256=hashlib.sha256(com.read_bytes()).hexdigest()),indent=2))
assert actual==expected,actual
print(mode,'MSX-JE conversion and saved bytes PASS')
