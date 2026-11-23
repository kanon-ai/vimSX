"""Physical MSX KANA toggle and I-key hold; verify saved Japanese bytes."""
from pathlib import Path
import argparse,os,subprocess,json,hashlib
ap=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','profile-dir','boot-disk','output'):ap.add_argument('--'+name,type=Path,required=True)
ap.add_argument('--variant',choices=['9IN','9EX'],required=True)
a=ap.parse_args();root=Path(__file__).resolve().parents[1];r=a.runtime.resolve();profile=a.profile_dir.resolve()
source=(root/'tests/regression.py').read_text(encoding='utf8');ns={'Path':Path}
exec(source[source.index('def files('):source.index('for mode in')],ns)
results=[]
for mode in ('ascii','kanji'):
 w=a.output.resolve()/mode;w.mkdir(parents=True,exist_ok=False)
 disk=w/'test.dsk';disk.write_bytes(a.boot_disk.read_bytes());com=root/'bin'/('VI'+a.variant+'.COM')
 s=['set renderer SDLGL-PP','set throttle off','set maxframeskip 0','set power on',
    'catch {diskmanipulator delete diska AUTOEXEC.BAT}','catch {diskmanipulator delete diska KANA.TXT}',
    'catch {diskmanipulator delete diska '+com.name+'}','diskmanipulator import diska {'+com.as_posix()+'}']
 if a.variant=='9EX':s+=['after time 1 {set videosource V9968}']
 def keys(t,text):s.append('after time '+str(t)+' {type_via_keybuf [binary format H* '+text.encode().hex()+']}')
 keys(40,'VI'+a.variant+' KANA.TXT\r');keys(60,':'+mode+'\r');keys(80,'i')
 s+=['after time 90 {keymatrixdown 6 16}','after time 90.1 {keymatrixup 6 16}',
     'after time 92 {keymatrixdown 3 64}','after time 92.1 {keymatrixup 3 64}',
     'after time 94 {keymatrixdown 3 64}','after time 95.5 {keymatrixup 3 64}',
     'after time 110 {screenshot -raw {'+(w/'held.png').as_posix()+'}}']
 keys(115,'\x1b');keys(125,':wq\r');s+=['after time 150 {exit}']
 sc=w/'run.tcl';sc.write_text('\n'.join(s))
 extensions=['vimSX_DOS2'] if (profile/'user/extensions/vimSX_DOS2.xml').exists() else []
 if a.variant=='9EX':extensions.append('V9968_VRAM4M')
 p=subprocess.run([r/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',*[v for e in extensions for v in ('-ext',e)],'-diska',disk,'-script',sc],input=b'<openmsx-control>\n',capture_output=True,timeout=90,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(r/'share'),OPENMSX_USER_DATA=str(profile/'user'),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
 (w/'emulator.log').write_bytes(p.stdout+p.stderr);assert p.returncode==0,p.returncode
 data=ns['files'](disk).get('KANA.TXT',b'');glyph='に'.encode('cp932')
 result=dict(variant=a.variant,mode=mode,actual=data.hex(),characters=len(data)//2,match=len(data)>=4 and data==glyph*(len(data)//2),sha256=hashlib.sha256(com.read_bytes()).hexdigest())
 results.append(result);(a.output/'results.json').write_text(json.dumps(results,indent=2))
 assert result['match'],result
 print(a.variant,mode,'physical KANA + I repeat PASS',result['characters'],flush=True)
