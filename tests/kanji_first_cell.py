from pathlib import Path
import subprocess,os,json,hashlib,sys
import argparse
ap=argparse.ArgumentParser(description='Check that DOS Kanji keeps the first glyph in both 40/80 column modes.')
ap.add_argument('--runtime',type=Path,required=True)
ap.add_argument('--boot-disk',type=Path,required=True)
ap.add_argument('--profile-root',type=Path,required=True)
ap.add_argument('--output',type=Path,required=True)
ap.add_argument('--variants',nargs='+',default=['MAP'])
ap.add_argument('--bin-dir',type=Path)
ap.add_argument('--expect-missing',action='store_true')
args=ap.parse_args()
root=Path(__file__).resolve().parents[1]
rt=args.runtime.resolve();before=args.expect_missing
out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
records=[]
for mode in args.variants:
    w=out/mode;w.mkdir();disk=w/'test.dsk';disk.write_bytes(args.boot_disk.read_bytes())
    com=(args.bin_dir or root/'bin')/f'VI{mode}.COM'
    (w/'CHECK.TXT').write_bytes(b':ssss\r\n:ssss\r\nEND')
    reg='V9968 regs' if mode=='9EX' else 'VDP regs'
    s=['set renderer SDLGL-PP','set throttle off','set maxframeskip 0','set power on','set deinterlace on','catch {diskmanipulator delete diska AUTOEXEC.BAT}']
    if mode=='9EX':s+=['after time 1 {set videosource V9968}']
    for f in [com,w/'CHECK.TXT']:
        s+=['catch {diskmanipulator delete diska '+f.name+'}','diskmanipulator import diska {'+f.as_posix()+'}']
    for t,keys in [(25,'MODE 80\r'),(40,'VI'+mode+' CHECK.TXT\r'),(70,'G:kanji\r'),(110,':mode40\r'),(145,':kanji\r'),(185,':ascii\r'),(215,':wq\r')]:
        s+=['after time '+str(t)+' {type_via_keybuf [binary format H* '+keys.encode().hex()+']}']
    for t,label in [(65,'ascii'),(100,'kanji80'),(175,'kanji40'),(205,'ascii40')]:
        s+=['after time '+str(t)+' {screenshot -raw {'+(w/(label+'.png')).as_posix()+'};set f [open {'+(w/(label+'.txt')).as_posix()+'} w];puts $f [binary encode hex [debug read_block {'+reg+'} 0 32]];close $f;set f [open {'+(w/(label+'-vram.bin')).as_posix()+'} wb];puts -nonewline $f [debug read_block {VRAM} 0 131072];close $f}']
    s+=['after time 240 {exit}'];sc=w/'run.tcl';sc.write_text('\n'.join(s))
    profile=args.profile_root.resolve()/('Panasonic_FS-A1ST-'+mode)/'user'
    p=subprocess.run([rt/'openmsx.exe','-control','stdio','-machine','vimSX_CPU_TEST',*(['-ext','V9968_VRAM4M'] if mode=='EXT' else []),'-diska',disk,'-script',sc],input=b'<openmsx-control>\n',capture_output=True,timeout=120,env=dict(os.environ,OPENMSX_SYSTEM_DATA=str(rt/'share'),OPENMSX_USER_DATA=str(profile),OPENMSX_HOME=str(w/'home'),SDL_AUDIODRIVER='dummy'))
    (w/'log.txt').write_bytes(p.stdout+p.stderr);assert p.returncode==0,(mode,p.returncode)
    print(mode, 'capture complete',flush=True)
    matched={}
    for label,wide in [('kanji80',True),('kanji40',False)]:
        v=(w/(label+'-vram.bin')).read_bytes()
        def glyph(row):
            result=[]
            for y in range(16):
                yy=row*16+y
                a=((yy%2)*65536+(yy//2)*256) if wide else yy*128
                result.extend(v[a:a+4])
            return bytes(result)
        matched[label]=glyph(0)==glyph(1)
        assert matched[label] != before,(mode,label,matched)
        assert any(b!=0x44 for b in glyph(1)),(mode,label,'empty oracle')
    source=(root/'tests/regression.py').read_text(encoding='utf-8');ns={'Path':Path}
    exec(source[source.index('def files('):source.index('results=[]')],ns)
    assert ns['files'](disk)['CHECK.TXT']==b':ssss\r\n:ssss\r\nEND'
    records.append(dict(variant=mode,first_glyph_equals_second_row=matched,saved_bytes_match=True,sha256=hashlib.sha256(com.read_bytes()).hexdigest()))
    (out/'results.json').write_text(json.dumps(records,indent=2))
