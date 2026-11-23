"""Measure emulator time between key-read entries, without host-time claims.
Requires a profile directory produced by cpu_compatibility.py for the turbo R.
"""
from pathlib import Path
import argparse, os, re, subprocess, json,hashlib

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--runtime', type=Path, required=True)
ap.add_argument('--profile-dir', type=Path, required=True)
ap.add_argument('--boot-disk', type=Path, required=True)
ap.add_argument('--output', type=Path, required=True)
ap.add_argument('--variants',nargs='+',default=['MAP','INT','9IN'])
ap.add_argument('--cpus',nargs='+',default=['z80','r800'])
ap.add_argument('--display',choices=['40','80','kanji'],default='40')
ap.add_argument('--bin-dir',type=Path,help='Optional archived binaries for before/after comparisons')
ap.add_argument('--build-dir',type=Path,help='Matching archived compiler listings')
args=ap.parse_args()
root=Path(__file__).resolve().parents[1]
bin_dir=args.bin_dir or root/'bin'; build_dir=args.build_dir or root/'build'
runtime=args.runtime.resolve(); profile=args.profile_dir.resolve()
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
results=[]
for mode in args.variants:
 for cpu in args.cpus:
  w=out/(mode+'-'+cpu);w.mkdir(exist_ok=False)
  disk=w/'test.dsk';disk.write_bytes(args.boot_disk.read_bytes())
  fixture=w/'BENCH.TXT';fixture.write_bytes(b'benchmark\r\n')
  listing=(build_dir/(mode+'.lst')).read_text()
  sym={k:int(re.search(r'([0-9A-F]{8})\s+\d+ _'+k+':',listing)[1],16)+0x103 for k in ('main','key')}
  in_ram=f'[debug read memory 0x100] == 195 && [debug read memory 0x101] == {sym["main"]&255} && [debug read memory 0x102] == {sym["main"]>>8}'
  # All operations execute; no simulated editing or host speed assumptions.
  groups=[('cpu',(':'+cpu+'\r').encode()),('insert',b'i'+b'a'*120),
          ('backspace',b'\b'*20),('lines',(b'\rline of code printf();'*18)),
          ('move',b'\x1b'+b'k'*18+b'j'*18),('numbers',b':set number\r'),('exit',b':q!\r')]
  if args.display!='40':groups.insert(1,('display',(':kanji\r' if args.display=='kanji' else ':mode80\r').encode()))
  keys=b''.join(v for _,v in groups)
  script=['set renderer SDLGL-PP','set throttle off','set maxframeskip 0','set power on',
          'catch {diskmanipulator delete diska AUTOEXEC.BAT}',
          'set benchActive 0','set benchIndex 0','set benchSamples {}',
          'set benchKeys [binary format H* '+keys.hex()+']']
  for f in (bin_dir/('VI'+mode+'.COM'),fixture):
   script+=['catch {diskmanipulator delete diska '+f.name+'}', 'diskmanipulator import diska {'+f.as_posix()+'}']
  script+=[f'debug set_bp {sym["main"]} {{!$benchActive && [debug read memory 0x100] == 195 && [debug read memory 0x101] == {sym["main"]&255} && [debug read memory 0x102] == {sym["main"]>>8}}} {{set benchActive 1;set benchLast [machine_info time]}}',
    f'debug set_bp {sym["key"]} {{$benchActive && {in_ram}}} {{set benchNow [machine_info time];lappend benchSamples [expr {{$benchNow-$benchLast}}];set benchLast $benchNow;if {{$benchIndex<[string length $benchKeys]}} {{set benchNext [string index $benchKeys $benchIndex];incr benchIndex;after time 0 [list type_via_keybuf $benchNext]}}}}',
    'debug set_bp 0 {$benchActive} {set benchActive 0;set f [open {'+(w/'samples.txt').as_posix()+'} w];puts $f $benchSamples;close $f;exit}',
    'after time 25 {type_via_keybuf [binary format H* '+('VI'+mode+' BENCH.TXT\r').encode().hex()+']}',
    'after time 600 {set f [open {'+(w/'timeout.txt').as_posix()+'} w];puts $f "$benchActive $benchIndex [llength $benchSamples] [reg PC]";catch {puts $f $errorInfo};close $f;screenshot -raw {'+(w/'timeout.png').as_posix()+'};exit}']
  sc=w/'run.tcl';sc.write_text('\n'.join(script))
  try:
   extensions=['vimSX_DOS2'] if (profile/'user/extensions/vimSX_DOS2.xml').exists() else []
   if mode=='9EX':extensions.append('V9968_VRAM4M')
   p=subprocess.run([runtime/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',*[s for e in extensions for s in ('-ext',e)],'-diska',disk,'-script',sc],
       input=b'<openmsx-control>\n',capture_output=True,timeout=60,
       env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(runtime/'share'),OPENMSX_USER_DATA=str(profile/'user'),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
  except subprocess.TimeoutExpired as e:
   (w/'emulator.log').write_bytes((e.stdout or b'')+(e.stderr or b''));raise
  (w/'emulator.log').write_bytes(p.stdout+p.stderr)
  assert p.returncode==0,(mode,cpu,p.returncode)
  samples=[float(v) for v in (w/'samples.txt').read_text().split()]
  assert len(samples)==len(keys),(len(samples),len(keys))
  result=dict(variant=mode,cpu=cpu,display=args.display,startup_ms=samples[0]*1000,sha256=hashlib.sha256((bin_dir/('VI'+mode+'.COM')).read_bytes()).hexdigest())
  offset=1
  for label,seq in groups:
   # Last exit character has no following key entry; exclude it.
   values=samples[offset:min(offset+len(seq),len(samples))]
   if values:result[label+'_mean_ms']=sum(values)*1000/len(values)
   offset+=len(seq)
  results.append(result);print(result,flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2))
