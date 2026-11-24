from pathlib import Path
import sys,json
interactive,lowmem=map(Path,sys.argv[1:3]);src=Path(__file__).resolve().parents[1]
for r in (interactive,lowmem):assert (r/'export/MSXSH.COM').read_bytes()==(src/'MSXSH.COM').read_bytes()
out=(interactive/'output.bin').read_bytes();log=(interactive/'run.log').read_text()
assert log.startswith('EXIT 1\nEXIT 0\n'),log
for expected in [b'CLI\r\n',b'editd\r\n',b'1 echo editd\r\n2 history\r\n',b'interactive\r\n']:
 assert expected in out,repr(expected)
assert out.count(b'editd\r\n')>=3
assert (lowmem/'export/PRE.BIN').read_bytes()==(lowmem/'export/POST.BIN').read_bytes()==bytes([3])
assert (lowmem/'export/GOOD.TXT').read_bytes()==b'RECOVERED\r\n'
assert (lowmem/'export/FINAL.TXT').read_bytes()==b'FINAL\r\n'
assert (lowmem/'run.log').read_text().startswith('EXIT 2\nEXIT 0\nEXIT 0\n')
assert b'need four free primary mapper segments' in (lowmem/'errors.bin').read_bytes()
print('PASS: interactive editing/history/state/exit, CLI, low-memory cleanup/recovery, final script line without newline')
