"""Create a fresh, isolated openMSX validation disk. Requires user's DOS2 disk."""
from pathlib import Path
import argparse,shutil,json
p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--boot',type=Path,required=True);p.add_argument('--search',type=Path,required=True);p.add_argument('--ls',type=Path,required=True);p.add_argument('--control',action='store_true');a=p.parse_args()
src=Path(__file__).resolve().parents[1];r=a.work.resolve();r.mkdir(parents=True,exist_ok=False)
f=r/'files';f.mkdir();(r/'export').mkdir();shutil.copy2(a.boot,r/'boot.dsk')
def file(name,data):
 q=f/name;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(data)
class Code:
 def __init__(self):self.b=bytearray();self.labels={};self.fix=[]
 def emit(self,s):self.b.extend(bytes.fromhex(s))
 def label(self,s):self.labels[s]=256+len(self.b)
 def jump(self,opcode,label):self.emit(opcode);self.fix.append((len(self.b),label));self.emit('0000')
 def finish(self):
  for pos,label in self.fix:self.b[pos:pos+2]=self.labels[label].to_bytes(2,'little')
  return bytes(self.b)
c=Code();c.label('read');c.emit('11 0002 21 0002 06 00 0e 48 cd 0500 b7');c.jump('ca','write');c.emit('fe c7');c.jump('ca','ok');c.jump('c3','bad');c.label('write');c.emit('11 0002 06 01 0e 49 cd 0500 b7');c.jump('c2','bad');c.jump('c3','read');c.label('bad');c.emit('06 02');c.jump('c3','exit');c.label('ok');c.emit('06 00');c.label('exit');c.emit('0e 62 cd 0500 c9');file('COPY.COM',c.finish())
file('RET.COM',bytes.fromhex('c9'));file('JUMP.COM',bytes.fromhex('c30000'));file('SEVEN.COM',bytes.fromhex('06070e62cd0500c9'))
file('WIPE.COM',bytes.fromhex('21000211010201efbd36a5edb0c30000'))
file('LEGACY.COM',bytes.fromhex('1e580e02cd050006000e62cd0500'))
file('DUP.COM',bytes.fromhex('06010e47cd050006000e62cd0500'))
file('CLOSE.COM',bytes.fromhex('06010e45cd050006000e62cd0500'))
file('FREE.COM',bytes.fromhex('af110204210000cd caff 79 32 0002 11000221010006010e49cd050006000e62cd0500'))
c=Code();c.emit('af110204210000cdcaff 31f07f af0600');c.jump('11','allocated');c.emit('d5e9');c.label('allocated');c.emit('31f0bf fb06000e62cd0500');file('ALLOC.COM',c.finish())
file('ZERO.COM',b'');file('BAD.BIN',b'\xc9')
for n in (4096,4097):
 code=bytes.fromhex('11000221')+n.to_bytes(2,'little')+bytes.fromhex('06010e49cd050006000e62cd0500')
 file(('OUT4K' if n==4096 else 'OVER')+'.COM',code.ljust(256,b'\0')+b'x'*(n-2)+b'\r\n')
file('T/ONE.TXT',b'alpha\r\nBeta\r\nalpha beta\r\n');file('T/SUB/CHILD.TXT',b'alpha child\r\n')
file('BIN/SEVEN.COM',bytes.fromhex('06070e62cd0500c9'))
tests=[]
def test(name,cmd,out='',code=0):tests.append(dict(name=name,cmd=cmd,out=out,code=code))
test('echo','echo hello','hello\r\n')
test('quotes',"echo 'a b' \"c d\" e\\ f",'a b c d e f\r\n')
test('env','export TEST=alive; echo $TEST ${TEST}; echo \'$TEST\'','alive alive\r\n$TEST\r\n')
test('unset','unset TEST; echo "${TEST}end"','end\r\n')
test('condition','false && echo BAD || echo OK; true || echo BAD; true && echo YES','OK\r\nYES\r\n')
test('ret','ret')
test('seven','seven',code=7)
test('jump','jump')
test('restore','export TEST=preserved; wipe; echo $TEST; pwd','preserved\r\nA:/\r\n')
test('pipe2','echo alpha | grep alpha','alpha\r\n')
test('pipe3','echo alpha | grep alpha | grep -n alpha','1:alpha\r\n')
test('laststatus','echo alpha | grep NONE',code=1)
test('empty','false | grep alpha',code=1)
test('boundary','out4k | copy >EXACT.OUT')
test('boundary3','out4k | copy | copy >EXACT3.OUT')
test('overflow','over | copy >MUSTNOT.OUT',code=2)
test('recovery','echo recovered | copy','recovered\r\n')
test('legacy','legacy | copy',code=2)
test('dup','dup | copy',code=2)
test('close','close | copy',code=2)
test('write','echo first >FIRST.TXT; echo second >>FIRST.TXT; copy <FIRST.TXT','first\r\nsecond\r\n')
test('externalfile','grep alpha T/ONE.TXT >MATCH.TXT; copy <MATCH.TXT','alpha\r\nalpha beta\r\n')
test('inputoverride','echo ignore | copy <T/ONE.TXT','alpha\r\nBeta\r\nalpha beta\r\n')
test('outputoverride','echo saved >OVERRIDE.TXT | grep alpha',code=1)
test('missinginput','echo BAD <ABSENT.TXT',code=2)
test('missingoutput','echo BAD >ABSENT/FAIL.TXT',code=2)
test('aftererrors','echo still-alive','still-alive\r\n')
test('cwd','cd T/SUB; pwd; cd ../..; pwd','A:/T/SUB\r\nA:/\r\n')
test('path','export PATH=A:/BIN; cd T; seven',code=7)
test('back','cd /; pwd','A:/\r\n')
test('recursive','grep -rh alpha T','alpha\r\nalpha beta\r\nalpha child\r\n')
test('find','find T -type f -name CHILD.TXT | grep CHILD','T\\SUB\\CHILD.TXT\r\n')
test('ls','ls T/*.TXT | grep ONE.TXT','ONE.TXT\r\n')
test('lslong','ls -l T/*.TXT | grep -c ONE.TXT','1\r\n')
test('lswide','ls -w T/*.TXT | grep -c ONE.TXT','1\r\n')
test('missing','NOSUCH',code=127)
test('badext','BAD.BIN',code=126)
test('emptycom','ZERO.COM',code=126)
test('nested','msxsh',code=126)
test('syntax','echo "unclosed',code=2)
test('background','echo BAD & echo BAD',code=2)
test('substitution','echo $(pwd)',code=2)
test('exitbad','exit 256',code=2)
test('repeat','\n'.join(['ret']*100)+'\necho repeats-ok','repeats-ok\r\n')
test('freebefore','free >BEFORE.BIN')
test('release','\n'.join(['alloc']*100)+'\nfree >AFTER.BIN')
test('statepipe','exit | copy',code=2)
if a.control:
 exec((Path(__file__).parent/'control_cases.py').read_text(),globals())
test('end','echo DONE','DONE\r\n')
script=''.join('echo CASE='+t['name']+'\n'+t['cmd']+'\necho STATUS=$?\n' for t in tests)+'exit 0\n'
file('TEST.SH',script.replace('\n','\r\n').encode());(r/'expected.json').write_text(json.dumps(tests,indent=2))
for source in [src/'MSXSH.COM',a.search/'GREP.COM',a.search/'FIND.COM',a.ls/'LS.COM']:shutil.copy2(source,f/source.name)
t=r'''set save_settings_on_exit false
set renderer SDLGL-PP
set throttle off
set power off
set case {@CASE@}
set lf [open "$case/run.log" w]
puts $lf [diskmanipulator import diska "$case/files"]
diskmanipulator export diska "$case/export" MSXSH.COM
set ef [open "$case/errors.bin" wb]
fconfigure $ef -translation binary
debug set_bp 0xc006 {[reg C] == 0x49 && [reg B] == 2} {puts -nonewline $ef [debug read_block memory [reg DE] [reg HL]]}
flush $lf
set power on
after realtime 5 {type "mode 80\r"}
after realtime 7 {type "free >PRE.BIN\r"}
after realtime 10 {type "msxsh TEST.SH >RESULT.TXT\r"}
after realtime 30 {type "free >POST.BIN\r"}
after realtime 36 {screenshot "$case/result.png";puts $lf [cpuregs];flush $lf}
after realtime 38 {set power off;foreach name {RESULT.TXT EXACT.OUT EXACT3.OUT FIRST.TXT MATCH.TXT OVERRIDE.TXT MUSTNOT.OUT BEFORE.BIN AFTER.BIN PRE.BIN POST.BIN} {catch {diskmanipulator export diska "$case/export" $name} e;puts $lf "$name: $e"};close $ef;close $lf;quit}
'''
(r/'run.tcl').write_text(t.replace('@CASE@',r.as_posix()));print(r)



