# Third-party components

The MIT license at the repository root applies to vimSX's original code,
tools, tests and documentation. It does not replace third-party licenses.

## SDCC runtime

The supplied COM files link runtime routines from SDCC's `z80.lib`.
The link maps identify these modules:

`modunsigned`, `memmove`, `mul`, `divunsigned`, `_strncmp`, `_memset`,
`strlen`, `_strcmp`, `modsigned`, `divsigned`.

Their upstream source files and copyright notices are retained under
`third_party/sdcc`. These files are licensed under GPL version 2 or later
with SDCC's special linking exception. The exception permits linking with
other files, some compiled with SDCC, without that linking alone imposing
the GPL on the executable. See the actual exception in each source header
and the accompanying [GPL text](third_party/sdcc/COPYING).

Upstream reference: SDCC SVN revision 16555, directory
<https://svn.code.sf.net/p/sdcc/code/!svn/bc/16555/trunk/sdcc/device/lib/>.
The seven assembly files are in `z80/`; the three underscore-prefixed C
files are in that directory's root. These source copies document the
linked routines; `tools/build.py` uses the installed toolchain's library.

[SDCC's library licensing explanation](https://sourceforge.net/p/sdcc/wiki/SDCC%20Library%20Licenses/)

The tested Windows toolchain reports SDCC 4.6.0 #16555 and was obtained
from the SDCC distribution bundled in
[MSXgl, commit ab4b26feda36189e677aa23b56acfdc3218f7953](https://github.com/aoineko-fr/MSXgl/tree/ab4b26feda36189e677aa23b56acfdc3218f7953/tools/sdcc).
The compiler/toolchain itself is not redistributed here, and no MSXgl
application library is linked into vimSX.

## Runtime dependencies not bundled

MSX BIOS and Kanji ROMs, MSX-DOS2/Nextor, MSX-JE and its dictionaries,
BlueMSX+/openMSX, and boot disk images are not distributed in this repository.
Supply appropriately licensed copies separately. The ASCII character generator
is read from the running machine's BIOS; a BIOS font ROM is not embedded in
the original editor source.

vimSX is an independent project, not an official Vim or MSX product.
