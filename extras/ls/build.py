"""Build with SDCC 4.x: python build.py --sdcc PATH/TO/sdcc.exe"""
import argparse, pathlib, subprocess, os
p=argparse.ArgumentParser();p.add_argument('--sdcc',default='sdcc');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parent
build=root/'build';build.mkdir(exist_ok=True)
sdcc=pathlib.Path(a.sdcc)
if sdcc.parent!=pathlib.Path('.'):
 os.environ['PATH']=str(sdcc.parent)+os.pathsep+os.environ.get('PATH','')
asm=str(sdcc.with_name('sdasz80.exe' if os.name=='nt' else 'sdasz80')) if sdcc.parent!=pathlib.Path('.') else 'sdasz80'
subprocess.run([asm,'-plosgff',str(build/'crt0.rel'),str(root/'crt0.s')],check=True)
subprocess.run([str(sdcc),'-mz80','--std-c99','--opt-code-size','--no-std-crt0',
 '--code-loc','0x300','--data-loc','0x4000','-Wl-b_HOME=0x180',str(build/'crt0.rel'),str(root/'ls.c'),'-o',str(build/'ls.ihx')],check=True)
data={};upper=0
for line in (build/'ls.ihx').read_text().splitlines():
 b=bytes.fromhex(line[1:]);assert sum(b)%256==0
 n,hi,lo,typ=b[:4];addr=upper+(hi<<8)+lo
 if typ==0:
  for i,v in enumerate(b[4:4+n]):data[addr+i]=v
 elif typ==4:upper=int.from_bytes(b[4:6],'big')<<16
assert min(data)==0x100 and max(data)<0x4000, 'Code/data overlap'
binary=bytes(data.get(i,0) for i in range(0x100,max(data)+1))
(root/'LS.COM').write_bytes(binary)
print(f'LS.COM: {len(binary)} bytes; highest code address {max(data):04X}')
