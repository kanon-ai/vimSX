from pathlib import Path
import shutil,json,argparse
parser=argparse.ArgumentParser()
parser.add_argument('--work-dir',type=Path,required=True)
parser.add_argument('--boot-disk',type=Path,required=True)
parser.add_argument('--program-dir',type=Path,default=Path(__file__).resolve().parents[1])
a=parser.parse_args()
r=a.work_dir.resolve();case=r/'run';case.mkdir(parents=True,exist_ok=False)
fixtures={
 'T/ONE.TXT':b'alpha\r\nBeta\r\nalpha beta\r\n\r\nlast',
 'T/TWO.TXT':b'alpha\r\nliteral a.b\r\nliteral axb\r\n[x]*\r\n',
 'T/EMPTY.TXT':b'', 'T/NODOT':b'alpha\n',
 'T/SUB/CHILD.TXT':b'TODO child\r\nalpha\r\n',
 'T/SUB/DEEP/LEAF.C':b'TODO leaf\nBeta\n',
 'T/SUB/AFTER.TXT':b'TODO after\r\n','T/SIB/OTHER.TXT':b'TODO sibling\r\n',
 'E/BOUND.TXT':b'a'*510+b'NEEDLE\n', 'E/LONG.TXT':b'x'*8193+b'\n',
 'E/LIMIT.TXT':b'x'*8192+b'\n', 'E/LARGE.TXT':b'0123456789 NEEDLE abcdefghijklmnopqrstuvwxyz\r\n'*2500,
 'E/BINARY.TXT':b'A\x00NEEDLE\n', 'E/JPN.TXT':'日本語です\r\n英語\r\n日本語\r\n'.encode('cp932'),
 'JPTEST.BAT':'@echo off\r\ngrep -n "日本語" E\\JPN.TXT\r\n'.encode('cp932'),
}
for name,data in fixtures.items():
 p=case/'files'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
shutil.copy2(a.boot_disk,case/'boot.dsk')
tests=[]
def add(name,cmd,code,out=None,contains=None,unordered=False):
 tests.append(dict(name=name,cmd=cmd,code=code,out=out.hex() if isinstance(out,bytes) else out,contains=contains,unordered=unordered,binary=isinstance(out,bytes)))
add('basic','grep alpha T\\ONE.TXT',0,'alpha\r\nalpha beta\r\n')
add('number','grep -n alpha T\\ONE.TXT',0,'1:alpha\r\n3:alpha beta\r\n')
add('ignore','grep -in beta T\\ONE.TXT',0,'2:Beta\r\n3:alpha beta\r\n')
add('invert','grep -v alpha T\\ONE.TXT',0,'Beta\r\n\r\nlast\r\n')
add('count','grep -c alpha T\\ONE.TXT',0,'2\r\n')
add('nomatch','grep NONE T\\ONE.TXT',1,'')
add('countzero','grep -c NONE T\\ONE.TXT',1,'0\r\n')
add('emptyfile','grep "" T\\EMPTY.TXT',1,'')
add('emptypattern','grep "" T\\ONE.TXT',0,'alpha\r\nBeta\r\nalpha beta\r\n\r\nlast\r\n')
add('literal','grep a.b T\\TWO.TXT',0,'literal a.b\r\n')
add('metachars','grep "[x]*" T\\TWO.TXT',0,'[x]*\r\n')
add('spaces','grep "alpha beta" T\\ONE.TXT',0,'alpha beta\r\n')
add('multiple','grep alpha T\\ONE.TXT T\\TWO.TXT',0,'T\\ONE.TXT:alpha\r\nT\\ONE.TXT:alpha beta\r\nT\\TWO.TXT:alpha\r\n')
add('wild','grep -l alpha T\\*.TXT',0,'T\\ONE.TXT\r\nT\\TWO.TXT\r\n',unordered=True)
todo=['T\\SUB\\CHILD.TXT:TODO child','T\\SUB\\DEEP\\LEAF.C:TODO leaf','T\\SUB\\AFTER.TXT:TODO after','T\\SIB\\OTHER.TXT:TODO sibling']
add('recursive','grep -r TODO T',0,'\r\n'.join(todo)+'\r\n',unordered=True)
add('recursivehide','grep -rh TODO T',0,'TODO child\r\nTODO leaf\r\nTODO after\r\nTODO sibling\r\n',unordered=True)
add('recursivecount','grep -rc TODO T',0,'T\\ONE.TXT:0\r\nT\\TWO.TXT:0\r\nT\\EMPTY.TXT:0\r\nT\\NODOT:0\r\nT\\SUB\\CHILD.TXT:1\r\nT\\SUB\\DEEP\\LEAF.C:1\r\nT\\SUB\\AFTER.TXT:1\r\nT\\SIB\\OTHER.TXT:1\r\n',unordered=True)
add('directory','grep TODO T',2,'',contains='directory requires -r')
add('stdin','grep -n beta <T\\ONE.TXT',0,'3:alpha beta\r\n')
add('redirect','grep alpha T\\ONE.TXT >MATCH.OUT',0,'alpha\r\nalpha beta\r\n')
add('boundary','grep NEEDLE E\\BOUND.TXT',0,b'a'*510+b'NEEDLE\r\n')
add('large','grep -c NEEDLE E\\LARGE.TXT',0,'2500\r\n')
add('limit','grep -c x E\\LIMIT.TXT',0,'1\r\n')
add('overlong','grep x E\\LONG.TXT',2,'',contains='line exceeds 8192 bytes')
add('binary','grep NEEDLE E\\BINARY.TXT',0,b'A\x00NEEDLE\r\n')
add('japanese','jptest',0,'1:日本語です\r\n3:日本語\r\n'.encode('cp932'))
add('epattern','grep -F -e alpha T\\ONE.TXT',0,'alpha\r\nalpha beta\r\n')
add('noargs','grep',2,'',contains='pattern required')
add('badoption','grep -z alpha T\\ONE.TXT',2,'',contains='unknown option')
add('missing','grep alpha T\\ABSENT.TXT',2,'',contains='DOS error')
tree=['T','T\\ONE.TXT','T\\TWO.TXT','T\\EMPTY.TXT','T\\NODOT','T\\SUB','T\\SUB\\CHILD.TXT','T\\SUB\\AFTER.TXT','T\\SUB\\DEEP','T\\SUB\\DEEP\\LEAF.C','T\\SIB','T\\SIB\\OTHER.TXT']
def findcase(name,cmd,paths):add(name,cmd,0,''.join(p+'\r\n' for p in paths),unordered=True)
findcase('findall','find T',tree)
findcase('findname','find T -name "*.txt"',[p for p in tree if p.endswith('.TXT')])
findcase('findtype','find T -type d',['T','T\\SUB','T\\SUB\\DEEP','T\\SIB'])
findcase('findfiles','find T -type f',[p for p in tree if p not in ['T','T\\SUB','T\\SUB\\DEEP','T\\SIB']])
findcase('finddepth0','find T -maxdepth 0',['T'])
findcase('finddepth1','find T -maxdepth 1',[p for p in tree if p.count('\\')<=1])
findcase('findand','find T -name "*.TXT" -type f -maxdepth 1',['T\\ONE.TXT','T\\TWO.TXT','T\\EMPTY.TXT'])
findcase('findempty','find T -name ""',[])
findcase('findquestion','find T -name "?NE.TXT"',['T\\ONE.TXT'])
findcase('findmany','find T\\SUB T\\SIB -type d',['T\\SUB','T\\SUB\\DEEP','T\\SIB'])
findcase('findfile','find T\\ONE.TXT',['T\\ONE.TXT'])
add('findbad','find T -maxdepth 33',2,'',contains='-maxdepth needs')
add('findmissing','find ABSENT',2,'',contains='DOS error')
add('findbadtype','find T -type z',2,'',contains='-type must')
findcase('finddefault','find -name LEAF.C',['.\\T\\SUB\\DEEP\\LEAF.C'])
findcase('finddot','find . -maxdepth 0',['.'])
findcase('findslash','find T/ -maxdepth 0',['T'])
add('findtrailmissing','find ABSENT/ -maxdepth 0',2,'',contains='DOS error')
add('continue','grep alpha T\\ABSENT.TXT T\\ONE.TXT',2,'T\\ONE.TXT:alpha\r\nT\\ONE.TXT:alpha beta\r\n',contains='DOS error')
(case/'expected.json').write_text(json.dumps(tests,ensure_ascii=False,indent=2),encoding='utf-8')
template=r'''set save_settings_on_exit false
set renderer SDLGL-PP
set throttle off
set power off
set case {@CASE@}
set dist {@DIST@}
set lf [open "$case/results.log" w]
diskmanipulator import diska "$dist/GREP.COM" "$dist/FIND.COM" "$case/files"
diskmanipulator export diska $case GREP.COM FIND.COM
set signatures {}
foreach name {GREP FIND} {
 set f [open "$case/$name.COM" rb];set loaded [read $f];close $f
 set f [open "$dist/$name.COM" rb];set wanted [read $f];close $f
 if {$loaded ne $wanted} {puts $lf "BINARY MISMATCH";close $lf;quit;return}
 puts $lf "$name.COM verified ([string length $loaded] bytes)"
 lappend signatures [string range $loaded 512 551]
}
set queue {@QUEUE@}
set active 0
proc nextcase {} {
 global queue active phase expected lf case stdoutfile stderrfile
 if {![llength $queue]} {set power off;diskmanipulator export diska $case MATCH.OUT;puts $lf DONE;close $lf;quit;return}
 lassign [lindex $queue 0] phase expected command;set queue [lrange $queue 1 end]
 puts $lf "BEGIN $phase: $command";flush $lf
 set stdoutfile [open "$case/$phase.out" wb];set stderrfile [open "$case/$phase.err" wb]
 set active 1;type "$command\r"
}
proc output {} {
 global active stdoutfile stderrfile
 if {!$active} {return}
 if {[reg B]==1} {set f $stdoutfile} else {set f $stderrfile}
 puts -nonewline $f [debug read_block memory [reg DE] [reg HL]]
}
proc exited {} {
 global active phase expected lf case stdoutfile stderrfile
 if {!$active} {return};set active 0
 set code [reg B];puts $lf "EXIT $phase $code EXPECTED $expected";flush $lf
 close $stdoutfile;close $stderrfile
 if {$code!=$expected} {after realtime 0.3 {screenshot "$case/FAIL-$phase.png";close $lf;quit}} else {
  if {$phase in {recursive findall findname japanese}} {after realtime 0.3 {screenshot "$case/$phase.png"}}
  after realtime 0.7 {nextcase}
 }
}
debug set_bp 5 {[reg C]==73 && [reg B] in {1 2} && [debug read_block memory 0x300 40] in $signatures} {output}
debug set_bp 5 {[reg C]==98 && [debug read_block memory 0x300 40] in $signatures} {exited}
set power on
after realtime 5 {type "mode 80\r"}
after realtime 9 {nextcase}
after realtime 240 {puts $lf TIMEOUT;flush $lf;screenshot "$case/TIMEOUT.png";close $lf;quit}
'''
queue='\n'.join('{'+t['name']+' '+str(t['code'])+' {'+t['cmd']+'}}' for t in tests)
(case/'run.tcl').write_text(template.replace('@DIST@',a.program_dir.resolve().as_posix()).replace('@CASE@',case.as_posix()).replace('@QUEUE@',queue))
print(len(tests),'cases prepared')
