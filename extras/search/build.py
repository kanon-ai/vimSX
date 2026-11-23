import argparse,pathlib,subprocess,os
p=argparse.ArgumentParser();p.add_argument('--sdcc',default='sdcc');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parent;build=root/'build';build.mkdir(exist_ok=True)
cc=pathlib.Path(a.sdcc)
if cc.parent!=pathlib.Path('.'):os.environ['PATH']=str(cc.parent)+os.pathsep+os.environ.get('PATH','')
asm=str(cc.with_name('sdasz80.exe' if os.name=='nt' else 'sdasz80')) if cc.parent!=pathlib.Path('.') else 'sdasz80'
subprocess.run([asm,'-plosgff',str(build/'crt0.rel'),str(root/'crt0.s')],check=True)
for name in ('grep','find'):
    flags=['-DGREP'] if name=='grep' else []
    subprocess.run([str(cc),'-mz80','--std-c99','--opt-code-size','--no-std-crt0','--code-loc','0x300','--data-loc','0x6000','-Wl-b_HOME=0x180',*flags,str(build/'crt0.rel'),str(root/'search.c'),'-o',str(build/(name+'.ihx'))],check=True)
    data={}
    for line in (build/(name+'.ihx')).read_text().splitlines():
        b=bytes.fromhex(line[1:]);assert sum(b)%256==0
        if b[3]==0:
            addr=int.from_bytes(b[1:3],'big')
            data.update({addr+i:v for i,v in enumerate(b[4:4+b[0]])})
        else:assert b[3]==1
    assert min(data)==256 and max(data)<0x6000
    binary=bytes(data.get(i,0) for i in range(256,max(data)+1))
    (root/(name.upper()+'.COM')).write_bytes(binary)
    print(name.upper()+'.COM:',len(binary),'bytes')
