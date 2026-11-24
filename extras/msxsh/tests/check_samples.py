from pathlib import Path
import json,sys
r=Path(sys.argv[1]);src=Path(__file__).resolve().parents[1]
for file,out in json.loads((r/'expected.json').read_text()).items():assert (r/'export'/file).read_bytes()==out.encode(),file
assert (r/'run.log').read_text()=='EXIT 0\nEXIT 0\nEXIT 0\n'
assert (r/'export/MSXSH.COM').read_bytes()==(src/'MSXSH.COM').read_bytes()
for name in ['DEMO.SH','CONTROL.SH']:assert (r/'export'/name).read_bytes()==(src/'examples'/name).read_bytes()
print('PASS: both supplied scripts, exact output/exit and binary/script identity')
