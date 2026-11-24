from pathlib import Path
import argparse,shutil
p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--boot',type=Path,required=True);a=p.parse_args()
r=a.work.resolve();r.mkdir(parents=True,exist_ok=False);f=r/'files';f.mkdir();(r/'export').mkdir();src=Path(__file__).resolve().parents[1]
shutil.copy2(a.boot,r/'boot.dsk');shutil.copy2(src/'MSXSH.COM',f/'MSXSH.COM')
(f/'RAMON.COM').write_bytes(bytes.fromhex('06030e68cd050006000e62cd0500'))
(f/'RAMOFF.COM').write_bytes(bytes.fromhex('06000e68cd050006000e62cd0500'))
(f/'FREE.COM').write_bytes(bytes.fromhex('af110204210000cd caff 79 32 0002 11000221010006010e49cd050006000e62cd0500'))
(f/'FINAL.SH').write_bytes(b'echo FINAL')
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
set ef [open "$case/errors.bin" wb]
fconfigure $ef -translation binary
debug set_bp 5 {[reg C] == 0x49 && [reg B] == 2 && [debug read_block memory 0x300 64] eq $signature} {puts -nonewline $ef [debug read_block memory [reg DE] [reg HL]]}
debug set_bp 5 {[reg C] == 0x62 && [debug read_block memory 0x300 64] eq $signature} {puts $lf "EXIT [reg B]";flush $lf}
set power on
after realtime 5 {type "mode 80\r"}
after realtime 7 {type "ramon\r"}
after realtime 9 {type "free >PRE.BIN\r"}
after realtime 11 {type "msxsh -c \"echo BAD\"\r"}
after realtime 14 {type "free >POST.BIN\r"}
after realtime 16 {type "ramoff\r"}
after realtime 18 {type "msxsh -c \"echo RECOVERED\" >GOOD.TXT\r"}
after realtime 21 {type "msxsh FINAL.SH >FINAL.TXT\r"}
after realtime 25 {screenshot "$case/result.png";puts $lf [cpuregs];flush $lf}
after realtime 27 {set power off;diskmanipulator export diska "$case/export" PRE.BIN POST.BIN GOOD.TXT FINAL.TXT;close $ef;close $lf;quit}
'''
(r/'run.tcl').write_text(t.replace('@CASE@',r.as_posix()).replace('@SIG@',signature));print(r)
