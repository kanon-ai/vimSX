# Third-party components

MSXSH.COM is built with SDCC 4.6.0 #16555. Its link map includes `divunsigned.rel`, `divsigned.rel`, `modunsigned.rel`, `modsigned.rel`, and `crtenter.rel` from `z80.lib`.

The matching unmodified sources from upstream revision 16555 and GPL text are in [third_party/sdcc](third_party/sdcc). They retain their GPL v2-or-later notices and SDCC linking exception. The independently written shell, supervisor, test programs and startup/build support use [MIT](LICENSE.txt).

- [SDCC source at revision 16555](https://sourceforge.net/p/sdcc/code/16555/tree/trunk/sdcc/device/lib/z80/)
- [SDCC library licensing](https://sourceforge.net/p/sdcc/wiki/SDCC%20Library%20Licenses/)

No GNU Bash code, MSX-DOS system file, BIOS, emulator executable or boot disk is distributed in this package. The test helper COM files are generated from their included source descriptions.
