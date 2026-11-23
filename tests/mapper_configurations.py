"""Test minimum RAM rejection and allocation across two mapper slots."""
from pathlib import Path
import argparse, os, re, shutil, subprocess, json,hashlib
import xml.etree.ElementTree as ET
ap=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','profile-dir','boot-disk','output'):ap.add_argument('--'+name,type=Path,required=True)
a=ap.parse_args();root=Path(__file__).resolve().parents[1];r=a.runtime.resolve()
listing=(root/'build/MAP.lst').read_text()
symbols={k:int(re.search(r'([0-9A-F]{8})\s+\d+ _'+k+':',listing)[1],16)+0x6000 for k in ('storageLimit','mapperCount','mapperSlots')}
source=(root/'tests/regression.py').read_text(encoding='utf8');ns={'Path':Path}
exec(source[source.index('def files('):source.index('for mode in')],ns)
results=[]
for case in ('128-reject','128-plus-128'):
 w=a.output.resolve()/case;w.mkdir(parents=True,exist_ok=False)
 u=w/'user';shutil.copytree(a.profile_dir/'user',u)
 config=u/'machines/vimSX_CPU_TEST.xml';tree=ET.parse(config)
 tree.find('.//MemoryMapper/size').text='128'
 if case=='128-plus-128':
  primary=tree.find('.//primary[@slot="2"]');primary.attrib.pop('external',None)
  ET.SubElement(primary,'MemoryMapper',{'id':'Expansion RAM'})
  m=primary.find('MemoryMapper');ET.SubElement(m,'size').text='128';ET.SubElement(m,'mem',{'base':'0x0000','size':'0x10000'})
 config.write_text('<?xml version="1.0"?>\n<!DOCTYPE msxconfig SYSTEM "msxconfig2.dtd">\n'+ET.tostring(tree.getroot(),encoding='unicode'))
 disk=w/'test.dsk';disk.write_bytes(a.boot_disk.read_bytes())
 data=(b'0123456789abcdefghijklmnop\r\n'*700)
 (w/'A.TXT').write_bytes(data)
 script=['set renderer SDLGL-PP','set throttle off','set maxframeskip 0','set power on','catch {diskmanipulator delete diska AUTOEXEC.BAT}','catch {diskmanipulator delete diska B.TXT}']
 for f in (root/'bin/VIMAP.COM',w/'A.TXT'):
  script+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
 def keys(t,s):script.append('after time '+str(t)+' {type_via_keybuf [binary format H* '+s.encode().hex()+']}')
 keys(25,'VIMAP A.TXT\r')
 script+=['after time 80 {screenshot -raw {'+(w/'startup.png').as_posix()+'}}']
 if case=='128-plus-128':
  script+=['after time 85 {set f [open {'+(w/'allocation.txt').as_posix()+'} w];puts $f [binary encode hex [debug read_block memory '+str(symbols['storageLimit'])+' 4]];set n [debug read memory '+str(symbols['mapperCount'])+'];puts $f $n;puts $f [binary encode hex [debug read_block memory '+str(symbols['mapperSlots'])+' $n]];close $f}']
  keys(100,'999yy');keys(250,':e B.TXT\r');keys(280,'p');keys(450,':wqa\r')
  # A second launch confirms DOS has reclaimed the user segments.
  keys(540,'VIMAP B.TXT\r');keys(650,':wq\r')
 script+=['after time 700 {exit}'];sc=w/'run.tcl';sc.write_text('\n'.join(script))
 p=subprocess.run([r/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST','-ext','vimSX_DOS2','-diska',disk,'-script',sc],input=b'<openmsx-control>\n',capture_output=True,timeout=180,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(r/'share'),OPENMSX_USER_DATA=str(u),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
 (w/'emulator.log').write_bytes(p.stdout+p.stderr);assert p.returncode==0,p.returncode
 files=ns['files'](disk);assert files['A.TXT']==data
 result=dict(variant='MAP',case=case,source_unchanged=True,sha256=hashlib.sha256((root/'bin/VIMAP.COM').read_bytes()).hexdigest())
 if case=='128-plus-128':
  assert files.get('B.TXT')==data,('copy mismatch',len(files.get('B.TXT',b'')))
  lines=(w/'allocation.txt').read_text().splitlines();slots=set(bytes.fromhex(lines[2]))
  assert len(slots)==2,slots
  result.update(limit=int.from_bytes(bytes.fromhex(lines[0]),'little'),slots=sorted(slots),copied_bytes=len(data))
 else:assert 'B.TXT' not in files
 results.append(result);print(result,flush=True)
(a.output/'results.json').write_text(json.dumps(results,indent=2))
