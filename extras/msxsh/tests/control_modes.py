"""Independent scripts check abort/EOF boundaries and interactive collection."""
from pathlib import Path
import argparse,json,shutil
p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--boot',type=Path,required=True);a=p.parse_args()
src=Path(__file__).resolve().parents[1];r=a.work.resolve();r.mkdir(parents=True,exist_ok=False);f=r/'files';f.mkdir();(r/'export').mkdir();shutil.copy2(a.boot,r/'boot.dsk');shutil.copy2(src/'MSXSH.COM',f/'MSXSH.COM')
cases=[
 ('missingfi','if true\nthen\necho BAD\n',2,''),
 ('missingdone','for i in a\ndo\necho BAD\n',2,''),
 ('badfor','for 1x in a; do echo BAD; done\necho BAD\n',2,''),
 ('depth5','if true; then if true; then if true; then if true; then if true; then echo BAD; fi; fi; fi; fi; fi\necho BAD\n',2,''),
 ('emptyelse','if true; then echo BAD; else; fi\necho BAD\n',2,''),
 ('exitloop','for i in a b; do echo $i; exit 7; done\necho BAD\n',7,'a\r\n'),
 ('nodes96','if true\nthen\n'+'true\n'*94+'fi\necho OK\n',0,'OK\r\n'),
 ('nodes97','if true\nthen\n'+'true\n'*95+'fi\necho BAD\n',2,''),
 ('blocklimit','if true\nthen\n'+('#'+'x'*199+'\n')*21+'echo BAD\nfi\necho BAD\n',2,''),
 ('finalfi','if true; then echo FINAL; fi',0,'FINAL\r\n'),
]
for i,(_,script,_,_) in enumerate(cases):(f/f'S{i}.SH').write_bytes(script.replace('\n','\r\n').encode())
(r/'expected.json').write_text(json.dumps(cases,indent=2))
signature=(src/'MSXSH.COM').read_bytes()[0x200:0x240].hex()
t=r'''set save_settings_on_exit false
set renderer SDLGL-PP
set throttle off
set power off
set case {@CASE@}
diskmanipulator import diska "$case/files"
diskmanipulator export diska "$case/export" MSXSH.COM
set signature [binary format H* @SIG@]
set lf [open "$case/run.log" w]
set of [open "$case/output.bin" wb]
fconfigure $of -translation binary
debug set_bp 0xc006 {[reg C] == 0x49 && [reg B] == 1} {puts -nonewline $of [debug read_block memory [reg DE] [reg HL]]}
debug set_bp 5 {[reg C] == 0x62 && [debug read_block memory 0x300 64] eq $signature} {puts $lf "EXIT [reg B]";flush $lf}
set power on
after realtime 5 {type "mode 80\r"}
'''
for i in range(len(cases)):t+=fr'after realtime {7+3*i} {{type "msxsh S{i}.SH >O{i}.TXT\r"}}'+'\n'
t+=r'''after realtime 39 {type "msxsh\r"}
after realtime 42 {type "for i in a b\r"}
after realtime 44 {type "do\r"}
after realtime 46 {type "echo \"interactive:$i\"\r"}
after realtime 48 {type "done\r"}
after realtime 50 {type "if true\r"}
after realtime 52 {type "then\r"}
after realtime 54 {type "echo branch\r"}
after realtime 56 {type "fi\r"}
after realtime 58 {type "if true\r"}
after realtime 60 {type "\003"}
after realtime 62 {type "echo cancelled:$?\r"}
after realtime 64 {type "exit\r"}
after realtime 66 {screenshot "$case/result.png"}
after realtime 68 {set power off;foreach name {O0.TXT O1.TXT O2.TXT O3.TXT O4.TXT O5.TXT O6.TXT O7.TXT O8.TXT O9.TXT} {diskmanipulator export diska "$case/export" $name};close $of;close $lf;quit}
'''
t=t.replace('$i',r'\$i').replace('$?',r'\$?')
(r/'run.tcl').write_text(t.replace('@CASE@',r.as_posix()).replace('@SIG@',signature));print(r)
