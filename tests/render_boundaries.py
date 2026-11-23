from pathlib import Path
import subprocess,os,json,hashlib
from PIL import Image,ImageChops
import argparse
ap=argparse.ArgumentParser(description='Requires a compatible custom openMSX and your own bootable Kanji/DOS2 disk.')
ap.add_argument('--runtime',type=Path,required=True)
ap.add_argument('--boot-disk',type=Path,required=True)
ap.add_argument('--output',type=Path,default=Path('test-output'))
ap.add_argument('--variants',nargs='+',default=['INT','EXT'])
ap.add_argument('--profile-root',type=Path)
args=ap.parse_args()
r=args.runtime.resolve()
boot=args.boot_disk.resolve()
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]

src=(Path(__file__).parent/'regression.py').read_text(encoding='utf8');ns={'Path':Path};exec(src[src.index('def files('):src.index('for mode in')],ns)
# Lead byte at offset 255, plus a wide glyph crossing a visual-row boundary.
data=b'a'*255+'日本語'.encode('cp932')+b'\r\n'+b'b'*39+'漢字'.encode('cp932')+b'\r\n'
results=[]
for mode in args.variants:
 w=out/('boundaries-'+mode);w.mkdir(exist_ok=False);u=w/'user';(u/'machines').mkdir(parents=True,exist_ok=True)
 machine='Panasonic_FS-A1ST'
 if args.profile_root:
  u=args.profile_root/('Panasonic_FS-A1ST-'+mode)/'user';machine='vimSX_CPU_TEST'
 elif mode=='INT':
  machine+='V4TEST';(u/'machines'/(machine+'.xml')).write_text((r/'share/machines/Panasonic_FS-A1ST_V9968.xml').read_text().replace('<vram>128</vram>','<vram>128</vram><vram4m>true</vram4m>'))
 disk=w/'test.dsk';disk.write_bytes(boot.read_bytes());(w/'EDGE.TXT').write_bytes(data)
 s=['set renderer SDLGL-PP','set maxframeskip 0','set throttle off','set power on','catch {diskmanipulator delete diska AUTOEXEC.BAT}']
 if mode=='9EX':s+=['after time 1 {set videosource V9968}']
 for f in [root/'bin'/('VI'+mode+'.COM'),w/'EDGE.TXT']:
  s+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
 ops=[('VI'+mode+' EDGE.TXT\r','initial'),(':z80\r','z80'),('iX\b\x1b','gap0'),('$iX\b\x1b0','gap255'),('j$xp0k','wide-cut'),(':kanji\r','kanji'),('iX\b\x1b','kanji-gap'),(':ascii\r','ascii'),(':set number\r','number'),(':set nonumber\r','nonumber'),(':wq\r','exit')]
 for n,(keys,label) in enumerate(ops):
  t=40+n*60;s+=['after time '+str(t)+' {type_via_keybuf [binary format H* '+keys.encode().hex()+']}', 'after time '+str(t+55)+' {screenshot -raw {'+(w/(label+'.png')).as_posix()+'}}']
 s+=['after time '+str(40+len(ops)*60)+' {exit}'];sc=w/'run.tcl';sc.write_text('\n'.join(s))
 z=subprocess.run([r/'openmsx.exe','-control','stdio','-machine',machine,*(['-ext','V9968_VRAM4M'] if mode in ('EXT','9EX') else []),'-diska',disk,'-script',sc],input=b'<openmsx-control>\n',capture_output=True,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(r/'share'),OPENMSX_USER_DATA=str(u),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'),timeout=150)
 (w/'log.txt').write_bytes(z.stdout+z.stderr)
 assert ns['files'](disk)['EDGE.TXT']==data
 # Ignore status (dirty indicator) and underline scanline; compare every text glyph.
 imgs={label:Image.open(w/(label+'.png')).convert('RGB') for _,label in ops[:-1]}
 for a,b in [('initial','z80'),('z80','gap0'),('gap0','gap255'),('gap255','wide-cut'),('kanji','kanji-gap'),('gap255','ascii'),('ascii','nonumber')]:
  height=16 if mode in ('9IN','9EX') and a=='kanji' else 8
  for y in range(11 if height==16 else 23):
   box=(32,14+y*height,512,14+y*height+height-1)
   assert ImageChops.difference(imgs[a].crop(box),imgs[b].crop(box)).getbbox() is None,(mode,a,b,y)
 results.append(dict(variant=mode,saved_bytes_match=True,pixel_comparisons_match=True,sha256=hashlib.sha256((root/'bin'/('VI'+mode+'.COM')).read_bytes()).hexdigest()))
 (out/'results.json').write_text(json.dumps(results,indent=2))
 print(mode,'page/wrap/gap boundaries, mode changes, numbers, byte-exact save PASS',flush=True)
