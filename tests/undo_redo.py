"""Exercise grouped undo/redo through actual editor keys and byte-exact saves."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess

p=argparse.ArgumentParser(description=__doc__)
for n in ('runtime','boot-disk','profile-root','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--variants',nargs='+',default=['MAP','9IN','9EX'])
p.add_argument('--cpu',choices=['z80','r800'],default='r800')
a=p.parse_args();root=Path(__file__).resolve().parents[1];rt=a.runtime.resolve()
ns={'Path':Path};source=(root/'tests/regression.py').read_text(encoding='utf-8')
exec(source[source.index('def files('):source.index('results=[]')],ns)
original='alpha\n日本語\nomega'
cases=[
 ('empty','u\x12',original),
 ('insert-undo','iABC\x1bu',original),
 ('insert-redo','iABC\x1bu\x12','ABCalpha\n日本語\nomega'),
 ('ex-undo','iABC\x1b:undo\r',original),
 ('ex-redo','iABC\x1b:undo\r:redo\r','ABCalpha\n日本語\nomega'),
 ('ex-uppercase','iABC\x1b:UNDO\r:REDO\r','ABCalpha\n日本語\nomega'),
 ('multilevel-undo','iONE\x1b0iTWO\x1b2u',original),
 ('multilevel-redo','iONE\x1b0iTWO\x1b2u2\x12','TWOONEalpha\n日本語\nomega'),
 ('newline-backspace','iABC\b\rDEF\x1bu\x12','AB\nDEFalpha\n日本語\nomega'),
 ('sjis-undo','j0xu',original),
 ('sjis-redo','j0xu\x12','alpha\n本語\nomega'),
 ('line-undo','jddu',original),
 ('line-redo','jddu\x12','alpha\nomega'),
 ('paste-undo','jyykpu',original),
 ('paste-redo','jyykpu\x12','alpha\n日本語\n日本語\nomega'),
 ('replace-undo',':%s/a/AA/g\ru',original),
 ('replace-redo',':%s/a/AA/g\ru\x12','AAlphAA\n日本語\nomegAA'),
 ('redo-branch','iOLD\x1buiNEW\x1b\x12','NEWalpha\n日本語\nomega'),
 ('mode-roundtrip','iABC\x1b:mode80\r:kanji\ru:mode40\r\x12','ABCalpha\n日本語\nomega'),
 ('savepoint-undo','iABC\x1b:w\ru',original),
 ('savepoint-redo','iABC\x1b:w\ru\x12','ABCalpha\n日本語\nomega'),
 ('cross-file','jdd:e B.TXT\rpu\x12:b1\ru:b2\ru:b1\r',original),
]
results=[]
for mode in a.variants:
 w=a.output.resolve()/mode;w.mkdir(parents=True,exist_ok=False)
 disk=w/'test.dsk';disk.write_bytes(a.boot_disk.read_bytes());com=root/'bin'/('VI'+mode+'.COM')
 listing=(root/'build'/(mode+'.lst')).read_text();mapping=(root/'build'/(mode+'.map')).read_text()
 data=int(re.search(r'^\s*([0-9A-F]+)\s+s__DATA\s',mapping,re.M)[1],16)
 def symbol(name,base):return int(re.search(r'([0-9A-F]{8})\s+\d+ _'+name+':',listing)[1],16)+base
 key=symbol('key',0x103);dirty=symbol('dirty',data);sig=com.read_bytes()[key-256:key-244].hex()
 s=['set renderer none','set throttle off','set power on','catch {diskmanipulator delete diska AUTOEXEC.BAT}', 'set testActive 0;set testCase 0;set testIndex 0']
 for i,(_,_,_) in enumerate(cases):(w/f'U{i:02}.TXT').write_bytes(original.replace('\n','\r\n').encode('cp932'))
 (w/'B.TXT').write_bytes(b'target')
 for f in [com,w/'B.TXT',*sorted(w.glob('U*.TXT'))]:s+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
 for i,(name,seq,expected) in enumerate(cases):
  prefix=(':'+a.cpu+'\r'+seq).encode('cp932')
  s+=['set testKeys('+str(i)+') [binary format H* '+(prefix+b':wqa\r').hex()+']',
      'set testCheck('+str(i)+') '+str(len(prefix)),
      'set testFile('+str(i)+') {'+(w/(name+'.dirty')).as_posix()+'}',
      'set testLaunch('+str(i)+') [binary format H* '+('VI'+mode+f' U{i:02}.TXT\r').encode().hex()+']']
 main=symbol('main',0x103)
 s+=['debug set_bp '+str(main)+' {!$testActive && [debug read memory 0x100]==195 && [debug read memory 0x101]=='+str(main&255)+' && [debug read memory 0x102]=='+str(main>>8)+'} {set testActive 1;set testIndex 0}',
     'debug set_bp '+str(key)+' {$testActive && [binary encode hex [debug read_block memory '+str(key)+' 12]] eq "'+sig+'"} {'
     'if {$testIndex==$testCheck($testCase)} {set f [open $testFile($testCase) w];puts $f [debug read memory '+str(dirty)+'];close $f};'
     'if {$testIndex<[string length $testKeys($testCase)]} {set next [string index $testKeys($testCase) $testIndex];incr testIndex;after time 0 [list type_via_keybuf $next]}}',
     'debug set_bp 0 {$testActive} {set testActive 0;incr testCase;if {$testCase<'+str(len(cases))+'} {after time 2 [list type_via_keybuf $testLaunch($testCase)]} else {after time 2 {type_via_keybuf [binary format H* '+b'echo RETURNED>RETURN.TXT\r'.hex()+']};after time 8 {exit}}}',
     'after time 25 {type_via_keybuf $testLaunch(0)}','after time 1800 {exit}']
 script=w/'run.tcl';script.write_text('\n'.join(s))
 profile=a.profile_root.resolve()/('Panasonic_FS-A1ST-'+mode)/'user'
 extensions=['vimSX_DOS2'] if (profile/'extensions/vimSX_DOS2.xml').exists() else []
 if mode in ('EXT','9EX'):extensions.append('V9968_VRAM4M')
 z=subprocess.run([rt/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',*[v for e in extensions for v in ('-ext',e)],'-diska',disk,'-script',script],input=b'<openmsx-control>\n',capture_output=True,timeout=360,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(rt/'share'),OPENMSX_USER_DATA=str(profile),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
 (w/'emulator.log').write_bytes(z.stdout+z.stderr);assert z.returncode==0,z.returncode
 files=ns['files'](disk)
 for i,(name,seq,expected) in enumerate(cases):
  actual=files.get(f'U{i:02}.TXT');want=expected.replace('\n','\r\n').encode('cp932');assert actual==want,(mode,name,actual,want)
  flag=int((w/(name+'.dirty')).read_text())
  expected_flag=0 if name in ('empty','insert-undo','ex-undo','multilevel-undo','sjis-undo','line-undo','paste-undo','replace-undo','savepoint-redo','cross-file') else 1
  assert flag==expected_flag,(mode,name,'dirty',flag,expected_flag)
 assert files['B.TXT']==b'target'
 assert b'RETURNED' in files.get('RETURN.TXT',b'')
 results.append(dict(variant=mode,cpu=a.cpu,cases=len(cases),saved_bytes_match=True,savepoint_flags_match=True,sha256=hashlib.sha256(com.read_bytes()).hexdigest()))
 a.output.joinpath('results.json').write_text(json.dumps(results,indent=2));print(mode,'undo/redo, grouping, savepoints and two buffers PASS',flush=True)
