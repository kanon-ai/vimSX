"""Exercise journal wraparound, eviction and oversized transaction recovery."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
from binary_layout import data_base
p=argparse.ArgumentParser(description=__doc__)
for n in ('runtime','boot-disk','profile-root','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--variants',nargs='+',default=['MAP','9IN','9EX']);a=p.parse_args()
root=Path(__file__).resolve().parents[1];rt=a.runtime.resolve();ns={'Path':Path}
source=(root/'tests/regression.py').read_text(encoding='utf8');exec(source[source.index('def files('):source.index('results=[]')],ns)
results=[]
for mode in a.variants:
 w=a.output.resolve()/mode;w.mkdir(parents=True,exist_ok=False);disk=w/'test.dsk';disk.write_bytes(a.boot_disk.read_bytes())
 com=root/'bin'/('VI'+mode+'.COM');listing=(root/'build'/(mode+'.lst')).read_text()
 def sym(n,base):return int(re.search(r'([0-9A-F]{8})\s+\d+ _'+n+':',listing)[1],16)+base
 key=sym('key',0x103);state=sym('histories',data_base(root,mode));sig=com.read_bytes()[key-256:key-244].hex()
 mask=2047 if mode=='MAP' else 65535
 s=['set renderer none','set throttle off','set power on','catch {diskmanipulator delete diska AUTOEXEC.BAT}','set seedUndo 0']
 # A legitimate empty journal near the ring end tests 16-bit wrap without
 # generating 65,000 edits. Both documents and all disk data are untouched.
 seed=bytes(sum(([((mask-2)&255),(mask-2)>>8] for _ in range(4)),[])+[1]).hex()
 s+=['debug set_bp '+str(key)+' {$seedUndo && [binary encode hex [debug read_block memory '+str(key)+' 12]] eq "'+sig+'"} {debug write_block memory '+str(state+9)+' [binary format H* '+seed+'];set seedUndo 0}']
 expected={'A.TXT':b'alpha','B.TXT':b'EDCBAalpha'}
 if mode=='MAP':expected.update({'EVICT.TXT':b'X'*(720-mask//3)+b'alpha','REDO.TXT':b'X'*720+b'alpha','BIG.TXT':b'Q\r\n'*1040+b'alpha'})
 for name in expected:(w/name).write_bytes(b'alpha')
 for f in [com,*[w/n for n in expected]]:s+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
 t=25
 def keys(text,delay=15):
  global t
  s.append('after time '+str(t)+' {type_via_keybuf [binary format H* '+text.encode().hex()+']}');t+=delay
 keys('VI'+mode+' A.TXT\r');keys(':e B.TXT\r')
 s.append('after time '+str(t)+' {set seedUndo 1;type_via_keybuf [binary format H* 0c]}');t+=5
 keys('iA\x1b0iB\x1b0iC\x1b0iD\x1b0iE\x1b5u5\x12');keys(':b1\r:wqa\r')
 if mode=='MAP':
  for name,redo in [('EVICT.TXT',False),('REDO.TXT',True)]:
   keys('VIMAP '+name+'\r');keys('iX\x1b'*720,600);keys('999u'+('999\x12' if redo else ''),60);keys(':wq\r')
  keys('VIMAP BIG.TXT\r');keys('i'+'Q\r'*1040+'\x1b',600);keys('uiZ\x1bu',30);keys(':wq\r')
 keys('echo RETURNED>RETURN.TXT\r');s+=['after time '+str(t)+' {exit}'];script=w/'run.tcl';script.write_text('\n'.join(s))
 z=subprocess.run([rt/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',*(['-ext','V9968_VRAM4M'] if mode in ('EXT','9EX') else []),'-diska',disk,'-script',script],input=b'<openmsx-control>\n',capture_output=True,timeout=500,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(rt/'share'),OPENMSX_USER_DATA=str(a.profile_root.resolve()/('Panasonic_FS-A1ST-'+mode)/'user'),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
 (w/'emulator.log').write_bytes(z.stdout+z.stderr);assert z.returncode==0,z.returncode
 files=ns['files'](disk)
 for name,want in expected.items():assert files.get(name)==want,(mode,name,len(files.get(name,b'')),len(want))
 assert b'RETURNED' in files.get('RETURN.TXT',b'')
 results.append(dict(variant=mode,ring_wrap=True,eviction_and_overflow=mode=='MAP',saved_bytes_match=True,sha256=hashlib.sha256(com.read_bytes()).hexdigest()))
 a.output.joinpath('results.json').write_text(json.dumps(results,indent=2));print(mode,'journal limits PASS',flush=True)
