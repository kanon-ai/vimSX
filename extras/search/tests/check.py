from pathlib import Path
import json,re,sys,hashlib
r=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'validation'
tests=json.loads((r/'expected.json').read_text(encoding='utf-8'))
log=(r/'results.log').read_text();results=dict((n,(int(a),int(b))) for n,a,b in re.findall(r'EXIT (\S+) (\d+) EXPECTED (\d+)',log))
errors=[]
record=json.loads((r/'results.json').read_text(encoding='utf-8')) if (r/'results.json').exists() else None
if record:
 for name,info in record['binaries'].items():
  data=(Path(__file__).resolve().parents[1]/(name+'.COM')).read_bytes()
  if len(data)!=info['bytes'] or hashlib.sha256(data).hexdigest()!=info['sha256']:errors.append(name+': binary identity mismatch')
for t in tests:
 name=t['name']
 if name not in results:errors.append(name+': not completed');continue
 actual=bytes.fromhex(record['cases'][name]['stdout_hex']) if record else (r/(name+'.out')).read_bytes();err=bytes.fromhex(record['cases'][name]['stderr_hex']) if record else (r/(name+'.err')).read_bytes()
 expected=bytes.fromhex(t['out']) if t['binary'] else t['out'].encode()
 if t['unordered']:actual=sorted(actual.splitlines());expected=sorted(expected.splitlines())
 if actual!=expected:errors.append(f'{name}: output {actual!r} != {expected!r}')
 if t['contains']:
  if t['contains'].encode() not in err:errors.append(f'{name}: expected error not found {err!r}')
 elif err:errors.append(f'{name}: unexpected stderr {err!r}')
 if results[name]!=(t['code'],t['code']):errors.append(name+': exit code mismatch')
if 'DONE' not in log:errors.append('run not complete')
if record or (r/'MATCH.OUT').exists():
 redirected=bytes.fromhex(record['redirected_hex']) if record else (r/'MATCH.OUT').read_bytes()
 if redirected!=b'alpha\r\nalpha beta\r\n':errors.append('DOS output redirection mismatch')
else:errors.append('redirected file missing')
if errors:
 print('\n'.join(errors));sys.exit(1)
print(f'PASS: {len(tests)} openMSX/MSX-DOS2 cases; exact output, errors and exit codes; redirected file verified.')
