"""Verify Kanji geometry at both ASCII widths, edits and DOS restoration.

Requires the same separately supplied runtime and profiles as the CPU tests.
Screenshots and memory observations are retained for visual review.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--runtime', type=Path, required=True)
ap.add_argument('--boot-disk', type=Path, required=True)
ap.add_argument('--profile-root', type=Path, required=True)
ap.add_argument('--output', type=Path, required=True)
ap.add_argument('--variants', nargs='+', required=True)
ap.add_argument('--columns', nargs='+', type=int, default=[40, 80])
args = ap.parse_args()
root = Path(__file__).resolve().parents[1]
r = args.runtime.resolve()
source = (root/'tests/regression.py').read_text(encoding='utf8')
ns = {'Path': Path}
exec(source[source.index('def files('):source.index('results=[]')], ns)
results = []
for mode in args.variants:
 for columns in args.columns:
  w = args.output.resolve()/f'{mode}-{columns}'
  w.mkdir(parents=True, exist_ok=False)
  profile = args.profile_root.resolve()/f'Panasonic_FS-A1ST-{mode}'/'user'
  disk = w/'test.dsk'; disk.write_bytes(args.boot_disk.read_bytes())
  data = b'H H H 0123456789\r\n'+('日本語 ABC xyz 0123\r\n').encode('cp932')+b'last'
  (w/'EDGE.TXT').write_bytes(data)
  com = root/'bin'/f'VI{mode}.COM'
  listing = (root/'build'/f'{mode}.lst').read_text()
  names = ['width','bodyRows','ascii80','kanji','videoStride','screenHeight']
  addr = {n: int(re.search(r'([0-9A-F]{8})\s+\d+ _'+n+':', listing)[1],16)+0x6000 for n in names}
  script = ['set renderer SDLGL-PP','set throttle off','set maxframeskip 0','set power on',
            'catch {diskmanipulator delete diska AUTOEXEC.BAT}']
  if mode == '9EX': script += ['after time 1 {set videosource V9968}']
  for f in [com,w/'EDGE.TXT']:
   script += ['catch {diskmanipulator delete diska '+f.name+'}', 'diskmanipulator import diska {'+f.as_posix()+'}']
  ops = [(f'MODE {columns}\r','dos-before'),(f'VI{mode} EDGE.TXT\r','initial'),
         (':kanji\r','kanji-initial'),(':ascii\r','ascii-initial'),
         (':mode40\r','narrow'),(':kanji\r','kanji40'),
         (':mode80\r','wide'),(':kanji\r','kanji80'),
         (':set number\r','number'),(':color 3\r','color'),(':color 1\r','color1'),
         ('iX\bQ\x1b','edit'),(':set nonumber\r','nonumber'),(':ascii\r','ascii'),
         (':wq\r','exit'),('echo RETURNED>RETURN.TXT\r','dos-after'),
         (f'VI{mode} EDGE.TXT\r','reopen'),
         (':mode'+str(80 if columns==40 else 40)+'\r','opposite'),
         (':kanji\r','kanji-exit'),(':q!\r','discard'),
         ('echo QUIT>QUIT.TXT\r','dos-kanji-exit')]
  for i,(keys,label) in enumerate(ops):
   t=30+i*35
   script += [f'after time {t} {{type_via_keybuf [binary format H* {keys.encode().hex()}]}}',
              'after time '+str(t+32)+' {screenshot -raw {'+(w/(label+'.png')).as_posix()+'}}']
   readings = ' '.join('[debug read memory '+str(addr[n])+']' for n in names)
   script += ['after time '+str(t+32)+' {set f [open {'+(w/(label+'.txt')).as_posix()+'} w];puts $f "'+readings+' [debug read memory 0xf3b0] [debug read memory 0xfcaf] [debug read memory 0xf3df] [debug read memory 0xffe8]";close $f}']
  script += [f'after time {30+len(ops)*35} {{exit}}']
  sc=w/'run.tcl';sc.write_text('\n'.join(script),encoding='utf8')
  p=subprocess.run([r/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',
      *(['-ext','V9968_VRAM4M'] if mode in ('EXT','9EX') else []),'-diska',disk,'-script',sc],
      input=b'<openmsx-control>\n',capture_output=True,timeout=180,
      env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(r/'share'),OPENMSX_USER_DATA=str(profile),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
  (w/'emulator.log').write_bytes(p.stdout+p.stderr)
  assert p.returncode==0,(mode,columns,p.returncode)
  states={label:list(map(lambda v:int(v,0),(w/(label+'.txt')).read_text().split())) for _,label in ops}
  # Some installed Kanji drivers retain 32/64 in LINLEN after DOS MODE.
  # Restore the observed entry state, rather than inventing a new DOS width.
  assert (states['dos-before'][-4]>40)==(columns==80),(mode,columns,'DOS MODE',states)
  for label,expected in [('initial',columns),('ascii-initial',columns),('wide',80),('ascii',80),('narrow',40)]:
   assert states[label][0]==expected,(mode,columns,label,states[label])
   assert states[label][-3]==(7 if expected==80 else 5),(mode,label,states[label])
  for label,wide in [('kanji-initial',columns==80),('kanji40',False),('kanji80',True)]:
   state=states[label]
   if mode in ('9IN','9EX'):
    assert state[:2]==[60,23],(mode,label,state)
   else:
    assert state[:2]==([63,21] if wide else [31,10]),(mode,label,state)
    assert bool(state[-1]&8)==wide,(mode,label,'interlace',state)
    assert state[-2]&14==(10 if wide else 6),(mode,label,'R0',state)
    from PIL import Image
    im=Image.open(w/(label+'.png')).convert('RGB');iw,ih=im.size
    long_rows=[y for y in range(ih) if sum(1 for x in range(iw) if min(im.getpixel((x,y)))>200)>iw*.65]
    assert long_rows and min(long_rows)>ih*.65,(mode,label,'separator position/width',long_rows)
  assert states['dos-after'][-4:-2]==states['dos-before'][-4:-2],(mode,columns,'restore')
  assert states['dos-kanji-exit'][-4:-2]==states['dos-before'][-4:-2],(mode,columns,'Kanji exit restore',states['dos-kanji-exit'],states['dos-before'])
  files=ns['files'](disk)
  assert files['EDGE.TXT']==b'Q'+data,(mode,columns,files['EDGE.TXT'])
  assert b'RETURNED' in files.get('RETURN.TXT',b''),(mode,columns,'DOS return')
  assert b'QUIT' in files.get('QUIT.TXT',b''),(mode,columns,'Kanji DOS return')
  result=dict(variant=mode,initial_columns=columns,kanji_40_80_geometry_edit_restore=True,
              sha256=hashlib.sha256(com.read_bytes()).hexdigest())
  results.append(result)
  (args.output/'results.json').write_text(json.dumps(results,indent=2))
  print(mode,columns,'KANJI 40/80 geometry / edit / save / DOS return PASS',flush=True)
