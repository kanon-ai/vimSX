# Undo/Redo validation — 2026-11-23

Release v0.1.0-alpha.3 adds independent per-buffer Undo/Redo histories.
The binary hashes and machine-readable results are in `validation-undo.json`.

- 176 command cases: all five editions on R800; MAP on a 256KiB non-turbo-R Z80 profile; 9IN/9EX on 128KiB non-turbo-R Z80 profiles.
- Insert grouping, newline/Backspace, Shift-JIS deletion, line deletion, paste, replacement, counts, redo branching, savepoint flags and independent buffer histories pass byte-exact saved-file checks.
- All five editions pass history-ring wrap checks. MAP additionally passes old-transaction eviction and oversized-edit recovery checks.
- Official BlueMSX+ v3.2.0-alpha: actual key input, exact before/undo and edited/redo screenshot equality, two-file saves and DOS return pass on all five editions.
- ASCII full-width cursor restoration after delete/undo/redo matches ROM-derived pixels on all five editions; native KANJI passes the corresponding 16x16 checks on 9IN/9EX.
- MSX-JE converts a Japanese sentence and saves the exact Shift-JIS bytes after Undo/Redo on 9IN.
- Native 9IN/9EX preserve a 277,980-byte document across display changes and saving, from both 40- and 80-column startup.
- MAP Z80 passes 24 incremental/full repaint comparisons at 40/80 columns.
- Code and resident data stay below the mapper/MSX-JE window. Native glyphs are fetched on demand to fit Undo code and cursor backing storage; rendered glyph caching remains in VRAM.

The 80-column benchmark compares alpha.2 and alpha.3 under the same managed openMSX build. Mean character-insertion time increases about 2–4% in these fixtures due to journaling. This is emulator time, not a hardware performance claim. Full timing results, including other operations, are recorded in JSON.

An individual edit exceeding history capacity clears that buffer's history with a warning. See `UNDO-ja.md` for exact limits. No persistent undo, undo tree or crash recovery is implemented. No physical-hardware or broad Nextor certification is claimed.

The following sections are historical results for earlier binary hashes.

# Cursor validation — 2026-11-23

All five builds were tested on the official BlueMSX+ v3.2.0-alpha x64 package.
ASCII double-width cursor pixels and exact restoration match the ROM-derived glyphs.
The native KANJI cursor also matches both full-width cells and restores all pixels.
Mode switches, colors, line numbers, editing, byte-exact saves and DOS return passed.
See `CURSOR-ja.md` and `validation-cursor.json` for scope and final binary hashes.
Manual display selection and document encoding are unchanged.
No emulator or RTL patch was needed for this cursor defect.
The records below describe earlier builds, identified by their original hashes.

## 2026-11-23 incremental row-tail optimization

MAP/INT/EXT now skip redundant blank-tail clearing and limit ASCII deletion
clears to the previous row end. See `PERFORMANCE-20261123-ja.md` and
`validation-performance.json` for emulator-time comparisons, matching COM
hashes, incremental/full-repaint VRAM comparisons and regression results.
The first-cell fix below remains included. VI9IN/VI9EX binaries are unchanged.

## 2026-11-23 first-cell correction

The current MAP/INT/EXT binaries fix DOS Kanji first-cell erasure by limiting
ASCII margin cleanup to graphics mode. See `FIRST-CELL-FIX-ja.md` and
`validation-first-cell.json` for before/after pixel checks, saved-byte checks,
BlueMSX+ regression, and R800 timing. VI9IN/VI9EX COM bytes are unchanged.
Older validation records below identify their tested builds by hash.
The corrected 4MB openMSX validation is described in `FIL-COMPATIBILITY-ja.md`.

# Validation — 2026-11-22; CPU compatibility update 2026-11-23

This is emulator evidence, not physical-hardware certification.

## FIL emulator discrepancy isolated — 2026-11-23

The custom openMSX used below still tests R#21 bit6 for FIL, while the
target RTL, vimSX and the BlueMSX+ reference implementation use R#20 bit5.
Internal and external diagnostic runs prove that toggling R#20 bit5 leaves
the image pixel-identical, while toggling the old R#21 bit6 changes it.
Restoring the registers restores the original image exactly. Neither COM
nor emulator files were patched. This is a register-specification mismatch,
not a claim that openMSX generally cannot display high-resolution graphics.
See [the detailed Japanese report](FIL-COMPATIBILITY-ja.md).
Native FIL visual acceptance on this openMSX remains unavailable.

## KANJI at both DOS widths — 2026-11-23 follow-up

The current follow-up binaries and evidence are recorded in
`validation-kanji80.json`. The earlier sections below describe earlier
checkpoints, not the hashes of this follow-up build.

MAP/INT/EXT now select the DOS Kanji driver's narrow or interlaced wide
mode from the last ASCII width. The driver receives LINL40 explicitly.
The separator and left-margin cleanup use the BIOS R#0/R#9 shadows and
write both interlace fields. SCRMOD alone was insufficient: the driver
can leave it at zero while displaying SCREEN 7. The usable text areas
are 31 half-width cells by 10 rows at 256x212, and 63 by 21 at 512x424.
This does not mean 80 half-width Kanji-mode cells. ASCII remains 40/80.
Direct exit from KANJI also restores the original DOS width, even after
changing to the opposite width inside the editor.

VI9IN/VI9EX retain their existing 512x424 FIL renderer, 60 half-width
cells by 23 rows and dedicated Latin font. Their COM files are unchanged.
All five editions' narrow/wide KANJI screens were inspected in live
BlueMSX+ windows, including Japanese glyphs, font proportions and the
separator. API-only interlace captures are not used to assess font quality.
Geometry checks also cover startup widths, numbers, colors, reversible
editing, save, ASCII restoration and direct KANJI exit. MSX-JE conversion
checks compare the saved Shift-JIS bytes of `今晩は、こんにちは。`.

Additional regression exercises exact-width rows, Japanese boundaries,
Enter/Backspace, scrolling, 900-line cross-document copy/cut/paste and
277,980-byte VRAM documents across repeated mode changes. The evidence
file distinguishes the renderer regression checkpoint from the final
DOS-exit-only adjustment. Each record carries its tested binary hash.

The DOS-driver KANJI renderer still has noticeable scrolling latency;
this update is a geometry/restoration correction, not a scrolling
performance improvement. A 29-move wide-KANJI test exceeded its initial
20-emulated-second wait; allowing completion passed the row and save
assertions. Several sandboxed emulator runs completed the editor's save
but terminated with Windows status 0xC0000008. Those runs are not counted
as passes; host-environment reruns require normal emulator termination.
BlueMSX+ uses 100% CPU/VDP settings and isolated review disks. No emulator
or ROM patch was made. Physical hardware remains unverified.

## Exact-width wrapping and mode-independent VRAM storage — 2026-11-23

The earlier wrap/storage checkpoint is recorded in `validation-wrap.json`.
Replace the previous MODE80 V9968 binaries: that build could corrupt documents
larger than the caches when switching display modes and subsequently saving.
The correction does not repair files already saved with damaged contents.

The renderer now consumes a following newline, or recognizes end of file,
after an exactly full display row. It retains the next-row insertion caret
when editing at the right edge. Tests cover full and double-width lines,
Japanese character boundaries, line numbers, Enter/Backspace, both scroll
directions, and byte-identical saves in every edition.

Document and clipboard transfers now use one physical odd/even VRAM layout
regardless of SCREEN 5/7. Non-interleaved modes transfer the two 128-byte
halves of each cache page separately. The internal version uses the BIOS
R#0 shadow rather than SCRMOD: the DOS Kanji driver can report SCRMOD=0
while the VDP actually uses SCREEN 7. External storage is initialized in a
known non-interleaved mode before loading the first document. Display and
atlas storage remain below the 256KiB document boundary.

The large-document test saves all 277,980 input bytes unchanged after
MODE80, KANJI, MODE40, MODE80, KANJI and ASCII transitions, starting from
both DOS widths. It exceeds the resident/mapper caches and a 256KiB VRAM
bank. Native tests include the 128KiB Sony HB-F1XV Z80 configuration and
the 256KiB turbo R configuration. Two-document regression also changes
modes between 900-line copies, cuts and pastes, and compares both saved files.

BlueMSX+ checks use isolated disks, actual key events, 100% CPU/VDP settings,
ASCII pixel round trips, save/relaunch/discard, and DOS commands after exit.
Native KANJI is inspected in both internal and external live windows.
MSX-JE conversion checks save `今晩は、こんにちは。` as exact Shift-JIS bytes.
The older openMSX is used for CPU, storage and row-cache checks; its FIL
image is not evidence of native KANJI visual quality. Emulator binaries and
ROMs are unchanged. Physical machines and all Nextor setups remain untested.

Performance records are emulator time between key-read entries, including
key polling. They are not host elapsed time or physical-machine benchmarks.
The benchmark starts on turbo R in R800 before selecting Z80/R800, so its
startup number is not a Z80 startup measurement. Long-line Z80 editing still
has noticeable latency. In the same 80-column native benchmark, Z80 insertion
was 93.53 ms/key (previously 101.85), and R800 insertion was 20.83 ms/key
(previously 23.03). These are results for that workload, not general speedup
ratios. The legacy software renderer needs longer to drain repeated cursor
moves; the row test waits for completion before checking its result.
Full Vim compatibility is not claimed.

## MODE40 / MODE80 and native high-resolution KANJI — 2026-11-23

This is a historical checkpoint. `validation-mode80.json` records its COM hashes,
individual results and emulator-time measurements. Earlier sections below are
historical checkpoints; their hashes must not be mistaken for this build.

All five editions (MAP, INT, EXT, 9IN, 9EX) were started after DOS `MODE 40`
and `MODE 80`. Tests checked inherited width, both command spellings, line
numbers, colors, KANJI/ASCII transitions, reversible insertion/Backspace,
exact saved bytes, restored DOS state, and a DOS command after exit.
Native KANJI uses SCREEN 7 with V9968 FIL and the expanded R#2 mask:
512x424 pixels, 60 half-width columns, 23 text rows. Its glyph atlas is
outside the displayed image and below the 256KiB document-storage boundary.

BlueMSX+ tests used independent disk/runtime copies and actual key events at
100% CPU and VDP speed. They checked mode/color/number changes, pixel-exact
ASCII round trips, save, relaunch, discard, and DOS commands after both exits.
The actual internal and external Windows displays were visually inspected.
MSX-JE's `今晩` candidate was inspected in the internal window; confirming
`今晩は、こんにちは。` produced the exact Shift-JIS file bytes.

Capture limitations are explicitly separated from editor verification:
BlueMSX+ agent PNG captures of FIL can lack the other interlace field, while
the live window correctly combines both. The older custom openMSX used for
automated memory/editing tests does not render FIL correctly. Its tests
establish editing, storage and return-to-DOS behavior, not FIL visual quality.
No emulator, ROM or Windows keyboard patch was made.

Regression checks cover two documents, cut/copy/paste, search/replacement,
unsaved guards, 900-line copies and capacity rejection. Native 128KiB and
256KiB MSX-JE cases pass. Non-turbo-R Sony HB-F1XV tests verify that R800 BIOS
calls are not used. The final mapper clear-loop optimization was followed by
new mode, BlueMSX+, Japanese-input, editing and CPU-compatibility checks;
its earlier large-copy results are retained under their own binary hashes.

Timing is measured between editor key-read entries in emulator time. It
includes processing and key polling, and is not a physical-hardware claim.
The benchmark starts in R800 on turbo R before the selected CPU command, so
its startup figure must not be presented as a Z80 startup measurement.
Native 80-column insertion in the benchmark is approximately 23 ms/key on
R800 and 102 ms/key on Z80; long-line Z80 editing still has noticeable waits.
Detailed mode/CPU results are retained in the JSON report.

This checkpoint retained the exact-width blank-row defect. Later long-document
tests also exposed VRAM corruption across display-mode changes. Both are
superseded by the correction above; the original reports remain as historical
evidence, not validation of the latest binaries.

## Native KANJI font and external exit correction — 2026-11-23

This correction supersedes the native COM hashes in the earlier two-type
report. Hashes and results for that checkpoint are in `validation-kanji-fix.json`.
The MAP binary is unchanged. The emulator executable and ROMs are unchanged.

Native KANJI printable ASCII now uses original 7x12 single-stroke glyphs in
8x16 cells, generated by `tools/make_ank_font.py`, rather than vertically
doubling the BIOS 8x8 font. Other BIOS single-byte glyphs are centered at
their native height. Full-width Japanese ROM glyphs are unchanged.

On external exit, the old editor image is cleared and replaced by an exit
notice directing DOS interaction to the main VDP. This does not switch the
host emulator's selected video source or redirect DOS onto external hardware.

Both editions were tested in BlueMSX+ at 100% speed and 100% VDP timing:
mixed ASCII/Japanese display, numbers, colors, KANJI/ASCII round trips,
insertion and byte-exact save, `:wq`, relaunch and `:q!`. The ASCII screen
before/after the mode changes matched pixel-for-pixel. DOS commands after
both exit paths created marker files, confirming that execution returned
to DOS. External exit screenshots show the notice instead of stale text.

Additional openMSX checks passed for the 128KiB MSX-JE sentence conversion
in both editions, and for Shift-JIS/cache/gap/wrap boundaries, full-width
cut/paste, numbers and mode changes. User launch disks were backed up and
only the corresponding COM file was replaced; other root files matched
byte-for-byte. Physical hardware remains unverified.

## Mapper / native V9968 editions — 2026-11-23

The new editions are `VIMAP.COM`, `VI9IN.COM` and `VI9EX.COM`.
These tests use the existing custom openMSX runtime and user-supplied ROMs
and DOS2/MSX-JE disk. No emulator or ROM patch was made for the editor.

| Edition / profile | Mapper RAM | Result |
|---|---:|---|
| MAP, Sony HB-F1XV, original V9958 | 256KiB | Editing, CPU guard, mode/color changes and exact saves passed |
| MAP, Panasonic FS-A1ST, original V9958 | 256KiB | Editing, Z80/R800 switching, restoration and exact saves passed |
| MAP, Sony, two mapper slots | 128+128KiB | Both slots used; 19,600-byte copy and restart passed |
| MAP, Sony, insufficient RAM | 128KiB | Refused startup, returned to DOS, source file unchanged |
| Native internal/external, Sony | 128KiB | Both passed; zero extra mapper cache segments allocated |
| Native internal/external, turbo R | 256KiB | Both passed; one extra mapper cache segment allocated |

MSX-JE conversion tests enter and save `今晩は、こんにちは。`, checking
the exact Shift-JIS bytes, including the absence of stray toggle-key spaces.
Native editions are tested at both 128KiB and 256KiB, on internal and external
video outputs. The mapper edition uses the installed DOS Kanji console.
Physical MSX keyboard-matrix tests toggle KANA, tap and hold the I key,
and verify that the saved text consists only of repeated `に`, in both
ASCII and KANJI modes and on both V9968 video outputs. These do not test
Windows IME interception or every host keyboard mapping.

Regression tests cover two-file cut/copy/paste, unsaved-file protection,
literal replacement, full-width glyph cutting/restoration, cache-page and
gap boundaries, wrapped lines, numbers and mode changes. Text images are
compared after reversible edits, excluding only the cursor underline and
status area. Large-copy tests use 900 lines (26,100 disk bytes), on 512KiB
for MAP and 256KiB for native V9968. Capacity-exceeding repeated paste is
rejected without changing the saved documents.

At 256KiB, this DOS2/driver combination leaves ten allocatable segments on
Sony (48KiB per document) and six on turbo R (16KiB per document). Capacity
therefore depends on free segments, not just installed RAM. An oversized
file was rejected without modifying its disk contents.

An artificial FS-A1ST configuration reduced to 128KiB did not reach the COM
entry point. It is **not** counted as a successful minimum-RAM test; the
128KiB result above is from the DOS2-booting Sony profile. Real turbo R tests
use its normal 256KiB configuration. Physical hardware and Nextor remain
unverified. MSX-JE behavior with other engines/dictionaries is not certified.

### Timing

Mean emulated milliseconds between key-read entries, including BIOS input
delivery, using `tests/benchmark.py`. These are neither host wall-clock
times nor pure CPU-cycle measurements. Tests run unthrottled to finish
sooner; reported values use the emulator's machine clock. No video was made.
All operations execute, without skipping input or editing work.

| Test | Prior VIINT Z80 | New VI9IN Z80 | Prior VIINT R800 | New VI9IN R800 |
|---|---:|---:|---:|---:|
| 120-character wrapped-line insertion | 213.6 | 111.8 | 57.6 | 24.0 |
| Backspace in that long line | 483.2 | 191.7 | 99.8 | 34.5 |
| Repeated short source-code lines | 524.8 | 88.3 | 101.9 | 19.6 |
| Vertical movement, including viewport changes | 719.8 | 171.3 | 126.6 | 37.8 |

The native results above include the optional 16KiB mapper cache. On the
same Sony MSX2+ profile, comparing 128KiB (no cache) with 256KiB (cache),
long-line insertion changes from 129.0 to 111.3ms, Backspace from 217.5 to
191.6ms, and short-line input from 93.6 to 88.1ms. This supports retaining
the optional mapper cache rather than assuming VRAM access is faster.
Long logical lines and large copies still take noticeable time on Z80.

The new MAP edition measured 133.8/38.1ms for long-line insertion and
118.8/36.5ms for short-line input (Z80/R800), on the same turbo R benchmark
profile. Functional MAP tests separately use a stock V9958 with no V9968.

See `validation-types.json` for machine results, saved-byte checks, timings
and tested binary hashes. Test scripts require separately supplied ROMs
and a bootable DOS2/MSX-JE disk; none are distributed here.

## Non-turbo-R CPU compatibility — 2026-11-23

The same internal/external COM pair is tested on three real-BIOS profiles:

| Profile | CPU | RAM configuration | Internal / external V9968 |
|---|---|---|---|
| Panasonic FS-A1F (MSX2) | Z80 | Mapper expanded to 256KiB, DOS2 cartridge | Both passed |
| Sony HB-F1XV (MSX2+) | Z80 | Mapper expanded to 256KiB, DOS2 cartridge | Both passed |
| Panasonic FS-A1ST (turbo R) | Z80 / R800 | Existing turbo R configuration | Both passed |

No BIOS ROM or emulator executable is patched for these tests. The internal
profile substitutes V9968 for the VDP; the external profile adds V9968 at 88h.
FS-A1F uses DEL during boot to bypass the built-in Cockpit application.

Tests cover startup, `:z80`, `:r800`, insertion/Backspace, search/replacement,
Japanese line copying/cutting across two files, ASCII/KANJI, numbers, color,
save-all and exit, checking both saved files byte-for-byte. Debugger traces
confirm **zero calls** to the editor's turbo-R CPU wrapper on MSX2/MSX2+.
The turbo-R trace confirms GETCPU, initial R800 selection, explicit Z80 and
R800 selection, and restoration on exit. The BIOS generation is read using
RDSLT from the main-ROM slot, not from DOS's page-0 RAM.

```sh
python tests/cpu_compatibility.py --runtime /path/to/openmsx-runtime --rom-dir /path/to/systemroms --boot-disk /path/to/boot.dsk --output test-output/cpu
```

Run `tools/build.py` first: this test uses the generated linker listings for
breakpoint addresses. ROMs and the boot disk must be supplied separately.
Standard V9938/V9958 VRAM alone remains unsupported. Stock 64KiB RAM machines,
physical hardware and arbitrary Nextor configurations are not validated.

## Scope

- Internal and external V9968 4MiB builds: two-buffer editing, Japanese
  line yank/cut/paste, character cut/paste, repeat counts, failed-open rollback,
  third-file refusal, unsaved-other-buffer exit guard, and save-all.
- Bulk fixture: 400 lines of ASCII/Shift-JIS text, copy all and cut 200 lines
  across documents, with byte-exact saved-file checks.
- Rendering: a Shift-JIS lead byte at offset 255 (cache-page boundary),
  wide glyph at a visual wrap boundary, insertion/backspace restoring the
  original text with different gap positions, ASCII/KANJI switching and
  line-number toggling. Both variants passed glyph-area comparisons and
  byte-exact saves. Screenshots exclude cursor underlines and changed status.
- EOF filler: a three-line CRLF file previously changed the first `~` row
  when the cursor moved to the last text line. The revised renderer produces
  identical text/filler pixels at the first line, last line and EOF cursor.

The bundled tests require your own compatible custom openMSX build with
`Panasonic_FS-A1ST_V9968.xml` and `V9968_VRAM4M` extension support, BIOS files,
and a bootable 720KiB FAT12 DOS2/Kanji disk (standard 1024-byte clusters).
The rendering test also requires Pillow (`python -m pip install Pillow`). They copy the disk into a dedicated output
directory before editing; use a fresh output directory for each run.
They inject keys into the emulator's BIOS key buffer for deterministic tests.
They are not recordings of physical-keyboard or host-IME behavior.

```sh
python tests/regression.py --runtime /path/to/openmsx-runtime --boot-disk /path/to/boot.dsk --output test-output/regression
python tests/render_boundaries.py --runtime /path/to/openmsx-runtime --boot-disk /path/to/boot.dsk --output test-output/render
```

The scripts currently expect `openmsx.exe` and its `share` directory, as used
by the tested Windows package. They disable emulator throttling for functional
regression testing only; performance measurements below use speed 100%.

## Z80 performance sample

Custom BlueMSX+, Panasonic FS-A1ST profile, internal V9968 4MiB, Z80,
emulator speed 100%. The same deterministic input is applied before and
after the display-traversal optimization. Measurements run from entry to the
key reader to its next entry, including key handling and redraw. Numbers are
mean emulator clock-counter ticks, not host wall-clock timings.

| Operation | Samples | Before | After | Reduction |
|---|---:|---:|---:|---:|
| Insert ASCII | 120 | 623,930 | 414,050 | 33.6% |
| Backspace | 20 | 1,771,389 | 1,388,702 | 21.6% |
| Up/down movement | 30 | 5,041,467 | 3,140,146 | 37.7% |

Insertion grows an initially empty document to 120 `a` characters, followed
by 20 Backspaces. Then 18 newline-plus-30-character lines are appended before
15 upward and 15 downward movements. These are fixture-specific improvements,
not a general speed guarantee. Z80 operation can still be slow on populated
screens and long lines. The optimization avoids per-character cache lookup
while rendering; it does not replace the editing algorithms.

The separate line-number timing mean was excluded because the original
counter samples contain a master-clock rollover. No speed claim is based
on that invalid mean.

## Build identity

The public source is rebuilt with SDCC 4.6.0 #16555 and compared byte-for-byte
with the emulator-tested internal and external COM files. `bin/SHA256SUMS`
records the released binary hashes. Other SDCC versions may produce different
binaries and need their own validation.

## Remaining limits

No physical hardware, broad Nextor compatibility, or exhaustive host-keyboard
matrix validation is claimed. Earlier development exposed KANA/host-IME and
rendering problems; the checks above cover specific regressions, not every
configuration. The README lists unsupported editor commands and save risks.
