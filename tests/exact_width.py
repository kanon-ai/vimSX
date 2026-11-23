"""Check exact-width wrapping, Japanese boundaries, numbers and end editing.

Uses an existing CPU-test profile and separately supplied emulator/boot disk.
The rendered-row cache is inspected, screenshots are retained, and saved
document bytes must remain identical after reversible edits.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--runtime',type=Path,required=True)
ap.add_argument('--boot-disk',type=Path,required=True)
ap.add_argument('--profile-root',type=Path,required=True)
ap.add_argument('--output',type=Path,required=True)
ap.add_argument('--variants',nargs='+',required=True)
ap.add_argument('--headless',action='store_true',help='Inspect row cache and saved bytes without the host renderer or screenshots')
ap.add_argument('--verify-only',action='store_true',help='Recheck retained captures and disk from the same COM binary without rerunning the emulator')
args=ap.parse_args();root=Path(__file__).resolve().parents[1];r=args.runtime.resolve()
source=(root/'tests/regression.py').read_text(encoding='utf8');ns={'Path':Path}
exec(source[source.index('def files('):source.index('results=[]')],ns)
results=[]
for mode in args.variants:
 w=args.output.resolve()/mode;w.mkdir(parents=True,exist_ok=args.verify_only)
 profile=args.profile_root.resolve()/f'Panasonic_FS-A1ST-{mode}'/'user'
 listing=(root/'build'/f'{mode}.lst').read_text()
 addr={n:int(re.search(r'([0-9A-F]{8})\s+\d+ _'+n+':',listing)[1],16)+0x6000 for n in ['painted','width','bodyRows','gutter','gx','gy']}
 disk=w/'test.dsk'
 if not args.verify_only:disk.write_bytes(args.boot_disk.read_bytes())
 com=root/'bin'/f'VI{mode}.COM'
 script=['set renderer '+('none' if args.headless else 'SDLGL-PP'),'set throttle off','set maxframeskip 10','set power on',
         'catch {diskmanipulator delete diska AUTOEXEC.BAT}']
 if mode=='9EX':script+=['after time 1 {set videosource V9968}']
 key_address=int(re.search(r'([0-9A-F]{8})\s+\d+ _key:',listing)[1],16)+0x103
 key_signature=com.read_bytes()[key_address-0x100:key_address-0x100+12].hex()
 script+=['set wrapShot {};set wrapImage 0',
  'debug set_bp '+str(key_address)+' {$wrapShot ne "" && [binary encode hex [debug read_block memory '+str(key_address)+' 12]] eq "'+key_signature+'"} {set f [open $wrapShot w];puts $f [list [debug read memory '+str(addr['width'])+'] [debug read memory '+str(addr['bodyRows'])+'] [debug read memory '+str(addr['gutter'])+'] [debug read memory '+str(addr['gx'])+'] [debug read memory '+str(addr['gy'])+'] [binary encode hex [debug read_block memory '+str(addr['painted'])+' 1863]]];close $f;if {'+('0' if args.headless else '$wrapImage')+'} {screenshot -raw [file rootname $wrapShot].png};set wrapShot {}}']
 fixture={};cases=[];scroll_cases=[]
 for layout,cols in [('40',40),('80',80),('kanji',60 if mode in ('9IN','9EX') else 31)]:
  for numbered in [False,True]:
   width=cols-(4 if numbered else 0);name=f'W{len(cases)}.TXT'
   data=b'A'*width+b'\r\nB\r\n'+b'A'*(width*2)+b'\r\nC\r\n'+b'A'*(width-2)+'漢'.encode('cp932')+b'\r\nD\r\n'+b'Z'*width
   fixture[name]=data;(w/name).write_bytes(data)
   cases.append((name,layout,numbered,cols))
 for layout,cols in [('40',40),('80',80),('kanji',60 if mode in ('9IN','9EX') else 31)]:
  name=f'S{len(scroll_cases)}.TXT';data=b'\r\n'.join(f'{i:02d}'.encode()+b'A'*(cols-2) for i in range(30))
  fixture[name]=data;(w/name).write_bytes(data);scroll_cases.append((name,layout,cols))
 for f in [com,*[w/n for n in fixture]]:
  script+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
 t=25;observations=[]
 def operation(keys,label=None,check=False):
  global t
  # Legacy software rendering can take over 20 emulated seconds for 29 moves.
  delay=50 if mode in ('INT','EXT') and label and label.startswith('scroll-') else 20
  script.append(f'after time {t} {{type_via_keybuf [binary format H* {keys.encode().hex()}]}}')
  if label:
   # DOS may temporarily map over editor RAM while polling. Inspect at the
   # next editor key entry, after an ignored Ctrl-L, with its own RAM mapped.
   script.append('after time '+str(t+delay-2)+' {set wrapShot {'+(w/(label+'.json')).as_posix()+'};set wrapImage '+str(int(check))+';type_via_keybuf [binary format H* 0c]}')
  t+=delay
 for i,(name,layout,numbered,cols) in enumerate(cases):
  operation('MODE 40\r')
  operation(f'VI{mode} {name}\r')
  operation(':kanji\r' if layout=='kanji' else ':mode'+layout+'\r')
  operation(':set number\r' if numbered else ':set nonumber\r',f'{i}-before',True)
  operation('A',f'{i}-insert')
  operation('X\b\r\b\x1b',f'{i}-after',True)
  operation(':wq\r')
  observations.append((i,cols,numbered))
 for i,(name,layout,cols) in enumerate(scroll_cases):
  operation('MODE 40\r');operation(f'VI{mode} {name}\r')
  operation(':kanji\r' if layout=='kanji' else ':mode'+layout+'\r')
  operation('j'*29,f'scroll-{i}-down',True)
  operation('k'*29,f'scroll-{i}-up',True)
  operation(':wq\r')
 operation('echo RETURNED>RETURN.TXT\r')
 script.append(f'after time {t} {{exit}}')
 sc=w/'run.tcl';sc.write_text('\n'.join(script),encoding='utf8')
 if not args.verify_only:
  p=subprocess.run([r/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',
     *(['-ext','V9968_VRAM4M'] if mode in ('EXT','9EX') else []),'-diska',disk,'-script',sc],
     input=b'<openmsx-control>\n',capture_output=True,timeout=300,
     env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(r/'share'),OPENMSX_USER_DATA=str(profile),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
  (w/'emulator.log').write_bytes(p.stdout+p.stderr);assert p.returncode==0,(mode,p.returncode)
 for i,cols,numbered in observations:
  gutter=4 if numbered else 0
  for stage in ['before','after']:
   parts=(w/f'{i}-{stage}.json').read_text().split();values=[int(x,0) for x in parts[:5]];rows=bytes.fromhex(parts[5])
   assert values[:3]==[cols,23 if mode in ('9IN','9EX') or cols in (40,80) else 10,gutter],(mode,i,stage,values)
   for y,ch in enumerate(b'ABAACADZ'):
    assert rows[y*81+gutter]==ch,(mode,i,stage,y,rows[y*81:(y+1)*81])
   if numbered:
    for y,number in enumerate([b'1',b'2',b'3',b'',b'4',b'5',b'6',b'7']):
     assert rows[y*81:y*81+gutter].strip()==number,(mode,i,stage,y,'line number')
   assert rows[8*81]==ord('~'),(mode,i,stage,'EOF blank row')
   # gx/gy belong to our graphics renderer; the DOS Kanji driver owns its own cursor.
   if mode in ('9IN','9EX') or cols in (40,80):
    assert values[3]<cols and values[4]<values[1],(mode,i,stage,'cursor outside text',values)
  # A full-width end-of-line insertion caret must stay inside the viewport.
  parts=(w/f'{i}-insert.json').read_text().split();values=[int(x,0) for x in parts[:5]]
  if mode in ('9IN','9EX') or cols in (40,80):
   assert values[3]<cols and values[4]<values[1],(mode,i,'insert cursor',values)
 for i,(_,_,cols) in enumerate(scroll_cases):
  for stage in ['down','up']:
   parts=(w/f'scroll-{i}-{stage}.json').read_text().split();body=int(parts[1],0);rows=bytes.fromhex(parts[5]);first=30-body if stage=='down' else 0
   for y in range(body):assert rows[y*81:y*81+2]==f'{first+y:02d}'.encode(),(mode,i,stage,y,rows[y*81:y*81+6])
 files=ns['files'](disk)
 assert files[com.name]==com.read_bytes(),(mode,'capture binary mismatch')
 for name,data in fixture.items():assert files.get(name)==data,(mode,name,'saved bytes changed')
 assert b'RETURNED' in files.get('RETURN.TXT',b''),(mode,'DOS return')
 results.append(dict(variant=mode,layouts=6,exact_width_newline_eof_and_double_width=True,
                     japanese_boundary=True,numbered=True,reversible_end_edits=True,
                     scroll_both_directions=True,saved_bytes_match=True,sha256=hashlib.sha256(com.read_bytes()).hexdigest()))
 (args.output/'results.json').write_text(json.dumps(results,indent=2))
 print(mode,'exact width / EOF / Japanese / numbers / Enter / Backspace / saved bytes PASS',flush=True)
