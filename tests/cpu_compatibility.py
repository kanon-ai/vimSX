"""Exercise real MSX2/MSX2+/turbo R BIOS profiles and trace editor CPU calls.

Needs the custom openMSX runtime, separately supplied ROMs, a 720KiB DOS2/Kanji
boot disk, Pillow, and build/*.lst from tools/build.py. No ROMs are downloaded.
"""
from pathlib import Path
import argparse
import json
import hashlib
import os
import re
import subprocess
import xml.etree.ElementTree as ET

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--runtime', type=Path, required=True)
ap.add_argument('--rom-dir', type=Path, required=True)
ap.add_argument('--boot-disk', type=Path, required=True)
ap.add_argument('--output', type=Path, required=True)
ap.add_argument('--machines', nargs='+', default=['Sony_HB-F1XV', 'Panasonic_FS-A1ST', 'Panasonic_FS-A1F'])
ap.add_argument('--variants', nargs='+', choices=['MAP', 'INT', 'EXT', '9IN', '9EX'], default=['INT', 'EXT'])
ap.add_argument('--ram-kb', type=int, default=256)
args = ap.parse_args()
root = Path(__file__).resolve().parents[1]
runtime = args.runtime.resolve()
roms = args.rom_dir.resolve()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
source = (Path(__file__).parent / 'regression.py').read_text(encoding='utf8')
ns = {'Path': Path}
exec(source[source.index('def files('):source.index('for mode in')], ns)

def rom_paths(tree):
    for node in tree.findall('.//filename'):
        path = roms / node.text
        if path.is_file():
            node.text = path.as_posix()

def write_config(tree, path):
    path.write_text('<?xml version="1.0"?>\n<!DOCTYPE msxconfig SYSTEM "msxconfig2.dtd">\n'
                    + ET.tostring(tree.getroot(), encoding='unicode'), encoding='utf8')

results = []
for machine in args.machines:
    turbo = machine.endswith('ST')
    for mode in args.variants:
        w = out / (machine + '-' + mode)
        w.mkdir(exist_ok=False)
        user = w / 'user'
        (user / 'machines').mkdir(parents=True)
        (user / 'extensions').mkdir()
        tree = ET.parse(runtime / 'share/machines' / (machine + '.xml'))
        rom_paths(tree)
        for mapper in tree.findall('.//MemoryMapper')+tree.findall('.//PanasonicRAM'):
            mapper.find('size').text = str(args.ram_kb)
        if mode in ('INT', '9IN'):
            vdp = tree.find('.//VDP')
            vdp.find('version').text = 'V9968'
            ET.SubElement(vdp, 'vram4m').text = 'true'
            ET.SubElement(vdp, 'timing').text = '0'
            for io in vdp.findall('io'):
                io.set('num', '5')
        profile = 'vimSX_CPU_TEST'
        write_config(tree, user / 'machines' / (profile + '.xml'))
        extensions = ['V9968_VRAM4M'] if mode in ('EXT', '9EX') else []
        if not turbo:
            dos = ET.parse(runtime / 'share/extensions/msxdos2.xml')
            rom_paths(dos)
            write_config(dos, user / 'extensions/vimSX_DOS2.xml')
            extensions.append('vimSX_DOS2')
        disk = w / 'test.dsk'
        disk.write_bytes(args.boot_disk.read_bytes())
        (w / 'A.TXT').write_bytes('alpha\r\n日本語\r\nomega'.encode('cp932'))
        (w / 'B.TXT').write_bytes(b'target')
        listing = (root / 'build' / (mode + '.lst')).read_text()
        address = {key: int(re.search(r'([0-9A-F]{8})\s+\d+ _' + key + ':', listing)[1], 16) + 0x103
                   for key in ('main', 'cpu', 'machineVersion')}
        com = (root / 'bin' / ('VI' + mode + '.COM')).read_bytes()
        cpu_signature = com[address['cpu']-0x100:address['cpu']-0x100+12].hex()
        script = ['set renderer SDLGL-PP', 'set throttle off', 'set maxframeskip 0', 'set power on',
                  'catch {diskmanipulator delete diska AUTOEXEC.BAT}',
                  'set tracking 0', 'set entered 0', 'set exited 0', 'set cpu_calls {}',
                  'set detected_version -1',
                  f'debug set_bp {address["main"]} {{!$entered && [debug read memory 0x100] == 195 && [debug read memory 0x101] == {address["main"] & 255} && [debug read memory 0x102] == {address["main"] >> 8}}} {{set tracking 1;set entered 1}}',
                  f'debug set_bp {address["cpu"]} {{$tracking && [binary encode hex [debug read_block memory {address["cpu"]} 12]] eq "{cpu_signature}"}} {{lappend cpu_calls [reg A]}}',
                  f'debug set_bp {address["machineVersion"] + 13} {{$tracking && $detected_version == -1 && [binary encode hex [debug read_block memory {address["machineVersion"]} 12]] eq "{com[address["machineVersion"]-0x100:address["machineVersion"]-0x100+12].hex()}"}} {{set detected_version [reg A]}}',
                  'debug set_bp 0 {$tracking} {set tracking 0;set exited 1}']
        if mode == '9EX':
            script += ['after time 1 {set videosource V9968}']
        if mode in ('MAP','9IN','9EX'):
            count_address=int(re.search(r'([0-9A-F]{8})\s+\d+ _mapperCount:',listing)[1],16)+0x6000
            script += ['after time 47 {set f [open {'+(w/'mapper-count.txt').as_posix()+'} w];puts $f [debug read memory '+str(count_address)+'];close $f}']
        if machine == 'Panasonic_FS-A1F':
            # Hold DEL during boot to skip the built-in Cockpit application.
            script.insert(script.index('set power on'), 'keymatrixdown 8 8')
            script += ['after time 12 {keymatrixup 8 8}']
        for f in (root / 'bin' / ('VI' + mode + '.COM'), w / 'A.TXT', w / 'B.TXT'):
            script += ['catch {diskmanipulator delete diska ' + f.name + '}',
                       'diskmanipulator import diska {' + f.as_posix() + '}']
        ops = [('VI' + mode + ' A.TXT\r', 'boot'), (':z80\r', 'z80'), (':r800\r', 'r800'),
               ('iX\bQ\x1b', 'edit'), ('jyy', 'yank'), (':e B.TXT\r', 'second'), ('p', 'paste'),
               (':bn\r', 'first'), ('dd', 'cut'), ('/omega\r', 'search'),
               (':%s/omega/end/g\r', 'replace'), (':kanji\r', 'kanji'), (':ascii\r', 'ascii'),
               (':set number\r', 'number'), (':color 2\r', 'color'), (':wa\r', 'save'), (':q\r', 'exit')]
        for n, (keys, label) in enumerate(ops):
            t = 25 + n * 25
            script += [f'after time {t} {{type_via_keybuf [binary format H* {keys.encode().hex()}]}}',
                       'after time ' + str(t + 22) + ' {screenshot -raw {' + (w / (label + '.png')).as_posix() + '}}']
        end = 25 + len(ops) * 25
        script += ['after time ' + str(end) + ' {set f [open {' + (w / 'trace.txt').as_posix()
                   + '} w];puts $f "$entered $exited";puts $f $cpu_calls;puts $f $detected_version;close $f;exit}']
        sc = w / 'run.tcl'
        sc.write_text('\n'.join(script), encoding='utf8')
        run = subprocess.run([runtime / 'openmsx.exe', '-control', 'stdio', '-machine', profile,
                              *[arg for ext in extensions for arg in ('-ext', ext)], '-diska', disk, '-script', sc],
                             input=b'<openmsx-control>\n', capture_output=True,
                             env=dict(os.environ, OPENMSX_SYSTEM_DATA=str(runtime / 'share'),
                                      OPENMSX_USER_DATA=str(user), OPENMSX_HOME=str(w / 'home'), SDL_AUDIODRIVER='dummy'),
                             timeout=180)
        (w / 'emulator.log').write_bytes(run.stdout + run.stderr)
        assert run.returncode == 0, (machine, mode, run.returncode, run.stdout, run.stderr)
        trace = (w / 'trace.txt').read_text().splitlines()
        assert trace[0] == '1 1', (machine, mode, trace)
        calls = [int(s, 0) for s in trace[1].split()] if len(trace) > 1 else []
        print(machine, mode, 'BIOS generation:', trace[2], 'CPU calls:', calls, flush=True)
        assert int(trace[2], 0) == (3 if turbo else 1 if machine == 'Panasonic_FS-A1F' else 2), trace
        if turbo:
            assert calls[:4] == [3, 2, 0, 2] and len(calls) == 5, calls
        else:
            assert calls == [], calls
        files = ns['files'](disk)
        assert files['A.TXT'] == b'Qalpha\r\nend', (machine, mode, files.get('A.TXT'))
        assert files['B.TXT'] == 'target\r\n日本語\r\n'.encode('cp932'), (machine, mode, files.get('B.TXT'))
        result=dict(machine=machine, variant=mode, ram_kb=args.ram_kb,cpu_calls=calls, saved_bytes_match=True,sha256=hashlib.sha256(com).hexdigest())
        if mode in ('MAP','9IN','9EX'):
            count=int((w/'mapper-count.txt').read_text(),0);result['mapper_segments']=count
            if mode!='MAP':assert count==(0 if args.ram_kb<256 else 1),(mode,count)
        results.append(result)
        (out / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf8')
        print(machine, mode, 'PASS', calls, flush=True)
(out / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf8')
