# Validation — 2026-11-22

This is emulator evidence, not physical-hardware certification.

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
