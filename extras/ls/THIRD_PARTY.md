# Third-party components

## SDCC runtime

`LS.COM` was built with SDCC 4.6.0 #16555. Its link map lists `divunsigned.rel`, `mul.rel`, `_modulong.rel`, `_divulong.rel`, and `crtenter.rel` from the Z80 runtime library.

The corresponding upstream sources are included without modification in `third_party/sdcc/`. Their copyright and license notices are retained. These files use GPL version 2 or later with the SDCC linking exception; see the headers and the repository's [GPL text](../../third_party/sdcc/COPYING). The exception allows linking this runtime into an executable without, by itself, imposing the GPL on the entire executable. The independently written LS source remains under [MIT](LICENSE.txt).

Upstream revision: [SDCC r16555](https://sourceforge.net/p/sdcc/code/16555/tree/trunk/sdcc/device/lib/).
The included files are `device/lib/_divulong.c`, `device/lib/_modulong.c`, and `device/lib/z80/{divunsigned.s,mul.s,crtenter.s}`.

[SDCC library licensing](https://sourceforge.net/p/sdcc/wiki/SDCC%20Library%20Licenses/).

## chips Z80 test core

`tests/z80.h` is the third-party chips Z80 CPU emulator by Andre Weissflog, licensed under the zlib/libpng license. Its full notice remains in the file. It is used only by the host-side tests and is not linked into `LS.COM`.

Upstream: https://github.com/floooh/chips

No emulator executable, BIOS, DOS system file, or boot disk is included with this bonus.
