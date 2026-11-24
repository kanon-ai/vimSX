from pathlib import Path
import argparse,shutil
p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--boot',type=Path,required=True);p.add_argument('--ls',type=Path,required=True);a=p.parse_args()
r=a.work.resolve();r.mkdir(parents=True,exist_ok=False);f=r/'files';f.mkdir();(r/'export').mkdir()
src=Path(__file__).resolve().parents[1];shutil.copy2(a.boot,r/'boot.dsk');shutil.copy2(src/'MSXSH.COM',f/'MSXSH.COM');shutil.copy2(a.ls/'LS.COM',f/'LS.COM')
signature=(src/'MSXSH.COM').read_bytes()[0x200:0x240].hex()
t=r'''set save_settings_on_exit false
set renderer SDLGL-PP
set throttle off
set power off
set case {@CASE@}
diskmanipulator import diska "$case/files"
diskmanipulator export diska "$case/export" MSXSH.COM
set lf [open "$case/run.log" w]
set of [open "$case/output.bin" wb]
fconfigure $of -translation binary
set signature [binary format H* @SIG@]
debug set_bp 0xc006 {[reg C] == 0x49 && [reg B] == 1} {puts -nonewline $of [debug read_block memory [reg DE] [reg HL]]}
debug set_bp 5 {[reg C] == 0x62 && [debug read_block memory 0x300 64] eq $signature} {puts $lf "EXIT [reg B]";flush $lf}
set power on
after realtime 5 {type "mode 80\r"}
after realtime 7 {type "msxsh -c \"echo CLI; false\"\r"}
after realtime 11 {type "msxsh\r"}
after realtime 14 {type "echo editX"}
after realtime 16 {type "\bd\r"}
after realtime 18 {keymatrixdown 8 0x20}
after realtime 18.2 {keymatrixup 8 0x20}
after realtime 20 {type "\r"}
after realtime 22 {type "history\r"}
after realtime 24 {type "export TEST=interactive\r"}
after realtime 26 {type "ls -w\r"}
after realtime 28 {type "echo $TEST\r"}
after realtime 30 {type "exit\r"}
after realtime 32 {type "echo BACKDOS\r"}
after realtime 36 {screenshot "$case/result.png";puts $lf [cpuregs];flush $lf}
after realtime 38 {close $of;close $lf;quit}
'''
# Tcl must send a literal dollar to MSXSH, not expand a Tcl variable.
t=t.replace('echo $TEST',r'echo \$TEST')
(r/'run.tcl').write_text(t.replace('@CASE@',r.as_posix()).replace('@SIG@',signature));print(r)

