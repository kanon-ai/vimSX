"""Build both MSX-DOS2 COM variants using an installed SDCC toolchain."""
from pathlib import Path
import argparse
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sdcc', default=os.environ.get('SDCC', 'sdcc'),
                    help='SDCC executable name or full path')
    args = ap.parse_args()
    compiler = shutil.which(args.sdcc)
    if not compiler:
        ap.error('SDCC not found; install SDCC or pass --sdcc /path/to/sdcc')
    compiler = Path(compiler).resolve()
    env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
    if os.name == 'nt':
        env['GCC_EXEC_PREFIX'] = compiler.parent.as_posix() + '/'
    (ROOT / 'build').mkdir(exist_ok=True)
    (ROOT / 'bin').mkdir(exist_ok=True)
    subprocess.run([compiler, '--version'], check=True, env=env)
    for mode in ('INT', 'EXT'):
        prefix = Path('build') / mode
        subprocess.run([compiler, '-mz80', '--no-std-crt0', '--code-loc', '0x103',
                        '--data-loc', '0x6000', *(['-DEXTERNAL'] if mode == 'EXT' else []),
                        'src/vimsx.c', '-o', prefix.with_suffix('.ihx')],
                       cwd=ROOT, env=env, check=True)
        image = bytearray(65536)
        end = 259
        for line in (ROOT / prefix.with_suffix('.ihx')).read_text().splitlines():
            if not line.startswith(':'):
                raise ValueError('Invalid Intel HEX record')
            record = bytes.fromhex(line[1:])
            if sum(record) % 256 or len(record) != record[0] + 5:
                raise ValueError('Invalid Intel HEX checksum/length')
            if record[3] == 0:
                address = int.from_bytes(record[1:3], 'big')
                if address < 259 or address + record[0] > 0x6000:
                    raise ValueError('Code overlaps DOS entry or data region')
                image[address:address + record[0]] = record[4:-1]
                end = max(end, address + record[0])
            elif record[3] != 1:
                raise ValueError('Unexpected extended Intel HEX record')
        map_text = (ROOT / prefix.with_suffix('.map')).read_text()
        entry = int(re.search(r'^\s*([0-9A-F]+)\s+_main\s', map_text, re.M)[1], 16)
        if not 259 <= entry < end or end >= 0x6000:
            raise ValueError('Invalid COM entry or code size')
        image[256:259] = bytes([195]) + entry.to_bytes(2, 'little')
        target = ROOT / 'bin' / ('VI' + mode + '.COM')
        target.write_bytes(image[256:end])
        print(target.name, target.stat().st_size, 'bytes', flush=True)

if __name__ == '__main__':
    main()
