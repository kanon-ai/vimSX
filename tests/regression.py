from pathlib import Path
import subprocess,os,json
import argparse
ap=argparse.ArgumentParser(description='Requires a compatible custom openMSX and your own bootable Kanji/DOS2 disk.')
ap.add_argument('--runtime',type=Path,required=True)
ap.add_argument('--boot-disk',type=Path,required=True)
ap.add_argument('--output',type=Path,default=Path('test-output'))
args=ap.parse_args()
r=args.runtime.resolve()
boot=args.boot_disk.resolve()
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]

def files(disk):
 d=disk.read_bytes();out={}
 for i in range(3584,7168,32):
  e=d[i:i+32]
  if not e[0] or e[0]==229 or e[11]&24:continue
  name=e[:8].decode('ascii').rstrip()+'.'+e[8:11].decode('ascii').rstrip()
  c=int.from_bytes(e[26:28],'little');data=b''
  while 2<=c<0xff8:
   data+=d[7168+(c-2)*1024:7168+(c-1)*1024]
   o=512+c*3//2;v=int.from_bytes(d[o:o+2],'little');c=(v>>4 if c&1 else v&4095)
  out[name]=data[:int.from_bytes(e[28:32],'little')]
 return out
for mode in ['INT','EXT']:
 for case in ['exchange','new','bulk']:
  w=out/('twofiles-'+mode+'-'+case);w.mkdir(exist_ok=False);u=w/'user';(u/'machines').mkdir(parents=True,exist_ok=True)
  machine='Panasonic_FS-A1ST'
  if mode=='INT':
   machine+='V4TEST';(u/'machines'/(machine+'.xml')).write_text((r/'share/machines/Panasonic_FS-A1ST_V9968.xml').read_text().replace('<vram>128</vram>','<vram>128</vram><vram4m>true</vram4m>'))
  disk=w/'test.dsk';disk.write_bytes(boot.read_bytes())
  (w/'A.TXT').write_bytes('alpha\r\n日本語かな\r\nomega'.encode('cp932'))
  (w/'B.TXT').write_bytes(b'target')
  bulk=''.join('%03d 日本語 abcdefghijklmnop\r\n'%i for i in range(400)).encode('cp932')
  if case=='bulk':(w/'A.TXT').write_bytes(bulk)
  script=['set renderer SDLGL-PP','set maxframeskip 0','set throttle off','set power on','catch {diskmanipulator delete diska AUTOEXEC.BAT}']
  for f in [root/'bin'/('VI'+mode+'.COM'),w/'A.TXT',w/'B.TXT']:
   script+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
  ops=[('VI'+mode+' A.TXT\r','boot')]
  if case=='exchange':
   ops += [(':set number\r','number'),(':e MISSING\\BAD.TXT\r','open-error'),('jyy','yank-jp'),(':e B.TXT\r','second'),('p','paste-jp'),(':bn\r','first'),('dd','cut-jp'),(':bp\r','second2'),('P','paste-before'),(':b1\r','first2'),('2yy','yank-last'),(':b2\r','second3'),('p','paste-last'),(':b1\r','first3'),('x','cut-char'),(':b2\r','second4'),('P','paste-char'),(':wq\r','unsaved-guard'),(':b1\r','first4'),('p','paste-back'),(':kanji\r','kanji'),(':ascii\r','ascii'),(':wa\r','saved'),(':e C.TXT\r','third-refused'),(':q\r','exit')]
   expected={'A.TXT':'alpha\r\nmoega'.encode('cp932'),'B.TXT':'target\r\n日本語かな\r\noomega\r\n日本語かな\r\n'.encode('cp932')}
  elif case=='new':
   ops += [('2dd','cut-two'),(':e NEW.TXT\r','new'),('p','paste-empty'),(':bn\r','first'),('yy','copy-last'),(':bp\r','second'),('2p','repeat-paste'),(':set number\r','number'),(':color 3\r','color'),(':z80\r','z80'),(':r800\r','r800'),(':wqa\r','exit')]
   expected={'A.TXT':b'omega','NEW.TXT':'alpha\r\nomega\r\nomega\r\n日本語かな\r\n'.encode('cp932')}
  else:
   first=b''.join(bulk.splitlines(keepends=True)[:200]);last=b''.join(bulk.splitlines(keepends=True)[200:])
   ops += [('999yy','copy-all'),(':e B.TXT\r','second'),('p','paste-all'),(':bn\r','first'),('200dd','cut-200'),(':bp\r','second2'),('P','paste-200'),(':wqa\r','exit')]
   expected={'A.TXT':last,'B.TXT':b'target\r\n'+first+bulk}
  for n,(keys,label) in enumerate(ops):
   spacing=40 if case=='bulk' else 6
   t=18+n*spacing
   script += [f'after time {t} {{type_via_keybuf [binary format H* {keys.encode("cp932").hex()}]}}', 'after time '+str(t+5)+' {catch {screenshot -raw {'+(w/(label+'.png')).as_posix()+'}}}']
  script.append('after time '+str(18+len(ops)*spacing+5)+' {exit}')
  sc=w/'run.tcl';sc.write_text('\n'.join(script))
  result=subprocess.run([r/'openmsx.exe','-control','stdio','-machine',machine,*(['-ext','V9968_VRAM4M'] if mode=='EXT' else []),'-diska',disk,'-script',sc],input=b'<openmsx-control>\n',capture_output=True,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(r/'share'),OPENMSX_USER_DATA=str(u),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'),timeout=120)
  (w/'emulator.log').write_bytes(result.stdout+result.stderr)
  actual=files(disk)
  for name,data in expected.items():assert actual.get(name)==data,(mode,case,name,repr(actual.get(name)),repr(data))
  assert 'C.TXT' not in actual
  print(mode,case,'PASS',flush=True)
