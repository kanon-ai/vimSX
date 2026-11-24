from pathlib import Path
import json,re,sys
r=Path(sys.argv[1]);src=Path(__file__).resolve().parents[1]
tests=json.loads((r/'expected.json').read_text());codes=list(map(int,re.findall(r'^EXIT (\d+)$',(r/'run.log').read_text(),re.M)))
assert codes==[t[2] for t in tests]+[0],codes
for i,(name,script,code,out) in enumerate(tests):
 actual=(r/f'export/O{i}.TXT').read_bytes();assert actual==out.encode(),(name,actual)
out=(r/'output.bin').read_bytes()
for text in [b'> ',b'interactive:a\r\n',b'interactive:b\r\n',b'branch\r\n',b'cancelled:130\r\n']:assert text in out,text
assert (r/'export/MSXSH.COM').read_bytes()==(src/'MSXSH.COM').read_bytes()
print(f'PASS: {len(tests)} independent scripts, syntax/EOF/bounds/exit, interactive if/for and Ctrl-C')
