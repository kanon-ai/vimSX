import argparse,pathlib,subprocess,os
p=argparse.ArgumentParser();p.add_argument('--sdcc',default='sdcc');a=p.parse_args()
r=pathlib.Path(__file__).resolve().parent;b=r/'build';b.mkdir(exist_ok=True)
(r/'MSXSH.COM').unlink(missing_ok=True)
cc=pathlib.Path(a.sdcc);os.environ['PATH']=str(cc.parent)+os.pathsep+os.environ.get('PATH','')
def run(args):subprocess.run([str(x) for x in args],check=True)
def ihx(p):
 d={}
 for line in p.read_text().splitlines():
  q=bytes.fromhex(line[1:]);assert sum(q)%256==0
  if q[3]==0:
   addr=int.from_bytes(q[1:3],'big');d.update({addr+i:v for i,v in enumerate(q[4:4+q[0]])})
  else:assert q[3]==1
 return d
asm=cc.with_name('sdasz80.exe');link=cc.with_name('sdldz80.exe')
run([asm,'-plosgff',b/'resident.rel',r/'resident.s'])
run([link,'-n','-i',b/'resident.ihx',b/'resident.rel'])
d=ihx(b/'resident.ihx');assert min(d)==0xc000 and max(d)<0xc600
blob=bytes(d.get(i,0) for i in range(0xc000,max(d)+1))
(r/'resident.inc').write_text('static const unsigned char resident_image[]={'+','.join(str(x) for x in blob)+'};\n')
run([asm,'-plosgff',b/'crt0.rel',r/'crt0.s'])
run([cc,'-mz80','--std-c99','--opt-code-size','--no-std-crt0','--code-loc','0x300','--data-loc','0x6000','-Wl-b_HOME=0x180',b/'crt0.rel',r/'shell.c','-o',b/'shell.ihx'])
d=ihx(b/'shell.ihx');assert min(d)==256 and max(d)<0x6000
import re
symbols=dict((name,int(value,16)) for value,name in re.findall(r'^\s*([0-9A-Fa-f]{8})\s+([sl]__\w+)',(b/'shell.map').read_text(),re.M))
assert symbols['s__DATA']+symbols['l__DATA']<=0xb000
assert symbols['s__HOME']+symbols['l__HOME']<=0x300
(r/'MSXSH.COM').write_bytes(bytes(d.get(i,0) for i in range(256,max(d)+1)))
print('MSXSH.COM',len((r/'MSXSH.COM').read_bytes()),'bytes; resident',len(blob))
