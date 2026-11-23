"""Verify VRAM-backed text larger than caches/banks survives display changes."""
from pathlib import Path
import argparse,os,re,json,subprocess,hashlib
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--runtime',type=Path,required=True)
ap.add_argument('--boot-disk',type=Path,required=True)
ap.add_argument('--profile-root',type=Path,required=True)
ap.add_argument('--machine',default='Panasonic_FS-A1ST')
ap.add_argument('--variants',nargs='+',required=True)
ap.add_argument('--output',type=Path,required=True)
a=ap.parse_args();root=Path(__file__).resolve().parents[1];runtime=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
s=(root/'tests/regression.py').read_text(encoding='utf8');ns={'Path':Path};exec(s[s.index('def files('):s.index('results=[]')],ns)
data=b''.join((f'{i:05d} '+('0123456789'*8)[:70]+'漢字').encode('cp932')+b'\r\n' for i in range(3390))
assert len(data)>262144
results=[]
for mode in a.variants:
 for columns in [40,80]:
  w=out/f'{mode}-{columns}';w.mkdir(exist_ok=False);profile=a.profile_root.resolve()/(a.machine+'-'+mode)/'user'
  disk=w/'test.dsk';disk.write_bytes(a.boot_disk.read_bytes());fixture=w/'LARGE.TXT';fixture.write_bytes(data);com=root/'bin'/f'VI{mode}.COM'
  listing=(root/'build'/f'{mode}.lst').read_text();sym={n:int(re.search(r'([0-9A-F]{8})\s+\d+ _'+n+':',listing)[1],16)+0x103 for n in ['main','key']}
  keys=b':mode80\r:kanji\r:mode40\r:mode80\r:kanji\r:ascii\r:wq\r'
  sig=com.read_bytes()[sym['key']-256:sym['key']-256+12].hex()
  script=['set renderer SDLGL-PP','set throttle off','set maxframeskip 10','set power on','catch {diskmanipulator delete diska AUTOEXEC.BAT}', 'set active 0;set idx 0', 'set keys [binary format H* '+keys.hex()+']']
  for f in [com,fixture]:script+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
  script+=['debug set_bp '+str(sym['main'])+' {[debug read memory 0x100]==195 && [debug read memory 0x101]=='+str(sym['main']&255)+' && [debug read memory 0x102]=='+str(sym['main']>>8)+'} {set active 1}',
   'debug set_bp '+str(sym['key'])+' {$active && [binary encode hex [debug read_block memory '+str(sym['key'])+' 12]] eq "'+sig+'"} {if {$idx<[string length $keys]} {set k [string index $keys $idx];incr idx;after time 0 [list type_via_keybuf $k]}}',
   'debug set_bp 0 {$active} {set active 0;set f [open {'+(w/'done.txt').as_posix()+'} w];puts $f $idx;close $f;after time 1 {exit}}',
   'after time 25 {type_via_keybuf [binary format H* '+('MODE '+str(columns)+'\r').encode().hex()+']}',
   'after time 35 {type_via_keybuf [binary format H* '+('VI'+mode+' LARGE.TXT\r').encode().hex()+']}',
   'after time 1800 {set f [open {'+(w/'timeout.txt').as_posix()+'} w];puts $f [list $active $idx [reg PC]];close $f;exit}']
  sc=w/'run.tcl';sc.write_text('\n'.join(script))
  extensions=['vimSX_DOS2'] if (profile/'extensions/vimSX_DOS2.xml').exists() else []
  if mode in ('EXT','9EX'):extensions.append('V9968_VRAM4M')
  z=subprocess.run([runtime/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',*[x for e in extensions for x in ['-ext',e]],'-diska',disk,'-script',sc],input=b'<openmsx-control>\n',capture_output=True,timeout=240,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(runtime/'share'),OPENMSX_USER_DATA=str(profile),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
  (w/'log.txt').write_bytes(z.stdout+z.stderr);assert z.returncode==0,z.returncode
  assert int((w/'done.txt').read_text())==len(keys)
  actual=ns['files'](disk)['LARGE.TXT'];assert actual==data,(mode,columns,len(actual),len(data))
  results.append(dict(variant=mode,machine=a.machine,initial_columns=columns,document_bytes=len(data),saved_bytes_match=True,sha256=hashlib.sha256(com.read_bytes()).hexdigest()))
  (out/'results.json').write_text(json.dumps(results,indent=2));print(mode,columns,len(data),'bytes, all mode changes and exact save PASS',flush=True)
