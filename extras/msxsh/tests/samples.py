from pathlib import Path
import argparse,shutil,json
p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--boot',type=Path,required=True);p.add_argument('--search',type=Path,required=True);a=p.parse_args()
src=Path(__file__).resolve().parents[1];r=a.work.resolve();r.mkdir(parents=True,exist_ok=False);f=r/'files';f.mkdir();(r/'export').mkdir();shutil.copy2(a.boot,r/'boot.dsk')
for file in [src/'MSXSH.COM',a.search/'GREP.COM',src/'examples/DEMO.SH',src/'examples/CONTROL.SH']:shutil.copy2(file,f/file.name)
expected={'DEMO.OUT':'=== MSXSH demo ===\r\nCurrent directory:\r\nA:/\r\nHello MSX!\r\nSuccess branch\r\nFailure branch\r\n1:apple banana\r\nPipeline status: 0\r\nNo-match status: 1\r\n=== Done ===\r\n','CONTROL.OUT':'Hello MSX!\r\nFruit: apple\r\nSpecial: banana\r\nFruit: cherry\r\napple banana\r\nFound: apple\r\nNot found: orange\r\nMode: sample\r\n'}
expected['CLI.OUT']='a\r\nb\r\n'
(r/'expected.json').write_text(json.dumps(expected,indent=2))
signature=(src/'MSXSH.COM').read_bytes()[0x200:0x240].hex()
t=r'''set save_settings_on_exit false
set renderer SDLGL-PP
set throttle off
set power off
set case {@CASE@}
diskmanipulator import diska "$case/files"
diskmanipulator export diska "$case/export" MSXSH.COM DEMO.SH CONTROL.SH GREP.COM
set signature [binary format H* @SIG@]
set lf [open "$case/run.log" w]
debug set_bp 5 {[reg C] == 0x62 && [debug read_block memory 0x300 64] eq $signature} {puts $lf "EXIT [reg B]";flush $lf}
set power on
after realtime 5 {type "mode 80\r"}
after realtime 7 {type "msxsh DEMO.SH >DEMO.OUT\r"}
after realtime 11 {type "msxsh CONTROL.SH >CONTROL.OUT\r"}
after realtime 15 {type "msxsh -c \"for i in a b; do echo \$i; done\" >CLI.OUT\r"}
after realtime 20 {screenshot "$case/result.png"}
after realtime 22 {set power off;diskmanipulator export diska "$case/export" DEMO.OUT CONTROL.OUT CLI.OUT;close $lf;quit}
'''
(r/'run.tcl').write_text(t.replace('@CASE@',r.as_posix()).replace('@SIG@',signature));print(r)
