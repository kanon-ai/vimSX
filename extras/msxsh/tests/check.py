from pathlib import Path
import sys,json,hashlib
r=Path(sys.argv[1]);src=Path(__file__).resolve().parents[1]
tests=json.loads((r/'expected.json').read_text());data=(r/'export/result.txt').read_bytes();errors=[];cases={}
for i,t in enumerate(tests):
 marker=('CASE='+t['name']+'\r\n').encode();end=('CASE='+tests[i+1]['name']+'\r\n').encode() if i+1<len(tests) else None
 if marker not in data:errors.append(t['name']+': missing');continue
 actual=data.split(marker,1)[1]
 if end:actual=actual.split(end,1)[0]
 expected=(t['out']+'STATUS='+str(t['code'])+'\r\n').encode()
 cases[t['name']]=actual.hex()
 if actual!=expected:errors.append(f"{t['name']}: {actual!r} != {expected!r}")
for name in ('EXACT.OUT','EXACT3.OUT'):
 q=r/'export'/name
 if not q.exists() or q.read_bytes()!=b'x'*4094+b'\r\n':errors.append(name+': 4096-byte mismatch')
if (r/'export/MUSTNOT.OUT').exists():errors.append('overflow executed downstream command')
for name,expected in [('FIRST.TXT',b'first\r\nsecond\r\n'),('MATCH.TXT',b'alpha\r\nalpha beta\r\n'),('OVERRIDE.TXT',b'saved\r\n')]:
 q=r/'export'/name
 if not q.exists() or q.read_bytes()!=expected:errors.append(name+': redirection mismatch')
if (r/'export/MSXSH.COM').read_bytes()!=(src/'MSXSH.COM').read_bytes():errors.append('binary identity mismatch')
for before,after in [('BEFORE.BIN','AFTER.BIN'),('PRE.BIN','POST.BIN')]:
 x=(r/'export'/before).read_bytes();y=(r/'export'/after).read_bytes()
 if len(x)!=1 or x!=y:errors.append(f'mapper leak: {before}={x.hex()} {after}={y.hex()}')
stderr=(r/'errors.bin').read_bytes()
for msg in [b'RAM pipe exceeds 4096 bytes',b'unsupported DOS call on RAM pipe',b'nested MSXSH unsupported',b'open input',b'open output']:
 if msg not in stderr:errors.append('missing diagnostic '+repr(msg))
report=dict(passed=not errors,cases=len(tests),binary_sha256=hashlib.sha256((src/'MSXSH.COM').read_bytes()).hexdigest(),errors=errors,stdout_hex=data.hex(),stderr_hex=stderr.hex())
(r/'results.json').write_text(json.dumps(report,indent=2))
print('\n'.join(errors) if errors else f'PASS: {len(tests)} cases, 100 child returns, exact 4KB pipes and overflow, binary identity')
sys.exit(bool(errors))
