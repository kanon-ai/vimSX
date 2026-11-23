# vimSX

MSX-DOS2向けの小型Vi風テキストエディタです。通常のマッパRAMを使う版と、V9968の4MiB VRAM・描画機能を使う版の2タイプで、英数字・Shift-JISの文章やソースファイルを編集します。独立した実装であり、Vim本体の移植や完全互換版ではありません。

開発版です。2ファイルの編集とカット＆ペースト、検索・置換、Undo/Redo、行番号、配色、ASCII／KANJI表示、Z80／R800切り替えに対応します。動作環境・制限・免責を確認のうえ、元ファイルのコピーでお試しください。検証内容は [検証記録](docs/VALIDATION.md) を参照してください。

## 動作環境と起動

- MSX2+のZ80、またはMSX turbo R。通常版は合計256KB以上、V9968版は128KB以上のメモリマッパRAMが必要です。
- 起動時にBIOSの機種情報を確認します。turbo RのみR800 DRAMモードへ切り替え、終了時に元のCPU設定へ戻します。非turbo RはZ80のまま動作し、R800用BIOSを呼びません。
- MSX-DOS2環境。通常版はV9958を使用し、V9968を必要としません。V9968版には4MiB VRAM拡張が必要です。
- 日本語表示には漢字ROM、通常版のKANJI画面には漢字ドライバ、日本語変換には適切に導入したMSX-JEが必要です。
- Undo/Redoは公式BlueMSX+ v3.2.0-alphaと管理版openMSXで5種類を検証しました。過去の検証環境と結果は検証記録に区別して掲載しています。実機およびNextorの各構成での動作は未検証です。

`bin` のCOMファイルを、ご自身のDOSディスクへコピーして実行してください。

```text
VIMAP MAIN.C
VI9IN MAIN.C
VI9EX NOTE.TXT
```

| COM | 本文・コピー領域 | 画面出力 |
|---|---|---|
| `VIMAP.COM` | 本体・拡張マッパRAM | 本体V9958 |
| `VI9IN.COM` | 内蔵V9968のVRAM＋任意のマッパキャッシュ | 内蔵V9968（98h） |
| `VI9EX.COM` | 外付けV9968のVRAM＋任意のマッパキャッシュ | 外付けV9968（88h） |

外付け版では外付けV9968の映像出力を選んでください。終了後のDOS操作は本体側の映像出力で行います。ファイル名を省略すると空の文書を開き、保存名は `TEXT.TXT` になります。従来の `VIINT.COM` / `VIEXT.COM` は比較用に残していますが、この2タイプの起動には上記のCOMを使います。

V9968版のKANJI画面では、半角英数字・記号に専用の7×12ドット字体（8×16セル）を使います。外付け版は終了時に編集画面を消し、DOS操作用に本体VDP側を選択する案内を表示します。映像出力の自動切替は行いません。

配布物にエミュレータ、BIOS、DOS、MSX-JE、ディスクイメージ、フォントROMは含みません。ASCIIフォントは実行環境のBIOSから読み取ります。

グラフィック表示のNORMALカーソルは文字色と背景色を交換し、全角文字はASCII表示でも2セル幅を使います。INSERT・コマンド入力は縦線です。ASCII／KANJIおよび40桁／80桁の手動切り替え仕様は変えていません。[カーソルの検証とRTL照合範囲](docs/CURSOR-ja.md)を参照してください。

## 基本操作

| 操作 | キー／コマンド |
|---|---|
| 移動 | `h j k l` またはカーソルキー、`0`、`$` |
| 入力 | `i`、`a`、`A`。`Esc`でNORMALへ |
| 削除・コピー・貼り付け | `x`、`dd`、`yy`、`p`、`P`。`3dd`、`2p`なども対応 |
| 取り消し・やり直し | `u` / `Ctrl+R`、`:undo` / `:redo`。回数指定も対応 |
| 検索 | `/文字列`、`n` |
| 置換 | `:s/old/new/`、`:s/old/new/g`、`:%s/old/new/g` |
| 行番号 | `:set number` / `:set nonumber` |
| 2ファイルを開く・切り替える | `:e SECOND.TXT`、`:bn` / `:bp`、`:b1` / `:b2` |
| 保存・終了 | `:w`、`:wa`、`:wq`、`:wqa`、`:q`、`:q!` |
| 表示 | `:ascii` / `:kanji` |
| 配色 | `:color 1` 青＋白、`:color 2` 黒＋白、`:color 3` 黒＋濃緑、`:color dos` |
| CPU | `:z80` / `:r800` |

非turbo Rでは `:z80` はそのまま継続し、`:r800` は「R800 requires MSX turbo R」と表示して編集を継続します。

コロンコマンドと検索はEnterで実行します。ASCIIは起動時のDOSの桁数を引き継ぎ、40桁ならSCREEN 5（256×212）、80桁ならSCREEN 7（512×212）のカラー画面を使用します。どちらも25行構成（本文23行、区切り、状態・コマンド行）です。`:mode40` / `:mode80`（`:mode 40` / `:mode 80`も可）で編集中に切り替えられます。`:ascii` は最後に選択したASCIIの桁数へ戻ります。通常版・従来版のKANJI画面は、40桁側では256×212（本文31半角桁×10行）、80桁側では512×424の標準インターレース（本文63半角桁×21行）で、漢字ドライバの字体を使用します。V9968版のKANJI画面はFILを使う512×424、60半角桁（30全角桁）・本文23行です。漢字ROMの16×16ドットを省略せず描画します。日本語変換はKANJIのINSERT状態でMSX-JEを使用します。Ctrl+Spaceで変換入力を開始し、Spaceで候補、Enterで確定、Ctrl+Spaceで終了します。ホストWindowsのIMEではありません。

操作の詳細は [コマンド一覧](docs/COMMANDS-ja.txt) を参照してください。

通常版で高解像度のKANJI画面にするには、`:mode80`、続いて`:kanji`を実行します。現在の`:mode40` / `:mode80`はASCIIへ切り替わるため、KANJIを続ける場合は再度`:kanji`が必要です。KANJIの本文幅は半角63桁相当（全角最大31文字）で、ASCIIの80桁とは異なります。

Undo/Redoは2ファイルで独立しています。挿入開始からEscまでを1回の編集として扱い、新しい編集でRedo側の履歴を破棄します。通常版は1文書あたり8KiB（空き不足時4KiB）、V9968版は256KiBを履歴に使用します。1回の編集が履歴容量を超えると、その文書の履歴を消去して警告を表示します。詳細は [Undo/Redo](docs/UNDO-ja.md) を参照してください。

## メモリの使い方

通常版はDOS2のマッパ管理APIで空きセグメントを確保し、Undo/Redo履歴を予約したうえで、残りを2文書と共通コピー領域へ均等に割り当てます。搭載RAMの全量が編集容量になるわけではありません。DOSやドライバの使用量により容量は変わります。終了時はDOSがユーザーセグメントを回収します。最低容量未満・空き不足では編集画面へ入らず終了します。

V9968版のVRAM配置は以下のとおりです。

| アドレス | 用途 |
|---|---|
| `000000–03FFFF` | 画面領域として予約（256KiB） |
| `040000–13FFFF` | 文書1のギャップバッファ（1MiB） |
| `140000–23FFFF` | 文書2のギャップバッファ（1MiB） |
| `240000–340000` | 共通コピー／カットレジスタ（最大1MiBの本文＋行末LF） |
| `340100–3800FF` | 文書1のUndo/Redo履歴（256KiB） |
| `380100–3C00FF` | 文書2のUndo/Redo履歴（256KiB） |

V9968版の本文は合計2MiBが上限です。残る4MiB内の領域をすべて使い切る構成ではありません。メインRAMには1KiBのページキャッシュ、フォント、描画差分、管理情報などを置きます。256KB以上の構成で空きセグメントがある場合は、追加で16KiBのマッパキャッシュを確保します。128KB構成や確保失敗時は追加キャッシュなしで動作します。キャッシュの置換時に変更内容をVRAMへ書き戻します。

V9968版は矩形塗りつぶし・コピーによる文字描画とASCIIの1行スクロールを使います。変更のない行やカーソル移動だけでは本文を描き直しません。長い論理行の走査・大きなコピー処理は依然CPU時間を要します。

## 現状の制限

- 初期公開の開発版です。元ファイルのコピーで試してください。保存は直接上書きで、原子的な置き換え・自動バックアップ・クラッシュ復旧はありません。保存中の障害でファイルが不完全になる可能性があります。
- 正規表現、Visualモード、分割表示、Vimプラグイン、シンタックス強調表示は未実装です。実装済みコマンドは上記一覧の範囲です。
- 文字コードはASCII / Shift-JISを想定します。UTF-8やバイナリの編集用ではありません。読み込み時にCRを除去し、保存時にLFをCRLFへ変換します。改行形式をそのまま保持する機能はありません。
- 最大2ファイルです。両方を開いた後に3つ目へ入れ替える機能はありません。`:q!` は両ファイルの未保存変更を破棄します。
- 変換候補や連文節変換の挙動はMSX-JE／辞書に依存します。エディタ独自の変換エンジンはありません。
- Z80はR800より遅く、長い行や画面内の文字数によって操作の待ち時間が増えます。既知の表示・入力問題がすべて解消したとは保証しません。

## おまけ：LSでLinuxターミナル気分

MSX-DOS2で `ls`・`ls -l`・`ls -w` を使える [LSコマンド](extras/ls/README.md) を追加しました。vimSXと一緒に、Linuxターミナルの気分を味わうためのおまけです。V9968は不要で、エディタとは独立して動作します。

[LS.COM](extras/ls/LS.COM) をご自身のDOS2ディスクへコピーしてお試しください。GNU `ls` の完全互換ではありません。趣味・実験用の無保証のソフトウェアです。[使い方・検証範囲・免責](extras/ls/README.md)を確認してください。

## ビルド

Python 3.10以降とZ80対応SDCCを用意してください。検証済みは **SDCC 4.6.0 #16555** です。

```sh
python tools/build.py
```

SDCCがPATHにない場合:

```sh
python tools/build.py --sdcc /path/to/sdcc
```

Windowsでは `--sdcc "C:\path\to\sdcc\bin\sdcc.exe"` と指定できます。結果は `bin/VIMAP.COM`、`bin/VI9IN.COM`、`bin/VI9EX.COM` です。`--variants MAP` のように対象を指定できます。ビルドスクリプトはエミュレータの設定やシステムのキーマッピングを変更しません。

[検証記録](docs/VALIDATION.md) / [第三者ライセンス](THIRD_PARTY.md)

## ライセンス

vimSXの独自コード・ドキュメントは [MIT](LICENSE) です。SDCCランタイムの部分はそれぞれの原ライセンス（GPL v2以降＋リンク例外）に従います。MSX、Vimなどの名称は互換性・由来の説明に使用しており、各プロジェクト・権利者による公式製品や承認を示しません。

## 免責・利用上の注意

本ソフトウェアは、MITライセンスに基づき「現状のまま」提供します。動作の正確性、特定目的への適合性、互換性、データの完全性などについて保証しません。適用法令で認められる範囲で、作者・著作権者は、本ソフトウェアの使用等に起因する請求、損害その他の責任を負いません。正式な条件は [LICENSE](LICENSE) の英文を参照してください。この説明はMITの利用許諾に追加の制限を設けるものではありません。

本版は開発版です。実機・Nextorの各構成では未検証で、エミュレータでの検証結果はすべての環境での動作や速度を保証しません。重要な文書やソースは別途バックアップを取り、コピーで動作確認してください。保存は直接上書きで、自動バックアップ、障害時の復旧機能はありません。Undo/Redo履歴は終了時に失われます。データの消失・破損が起きた場合も、本ソフトウェアによる復旧は保証できません。

使用するBIOS、DOS、漢字ROM・ドライバ、MSX-JEなどは、ご自身で適切な利用権限を確保してください。これらの第三者製品は配布物に含めていません。

## English

This is a development release. For the mapper's wide Kanji layout, run `:mode80` followed by `:kanji`; width commands return to ASCII. Wide Kanji provides 63 half-width text columns, not 80. See the requirements, limitations, disclaimer and [validation report](docs/VALIDATION.md).

vimSX is a small, independently implemented Vi-like editor for MSX-DOS2 on MSX2+ or MSX turbo R. It is not a port of Vim and does not implement full Vi/Vim compatibility.

The BIOS generation is read through the main-ROM slot. Only turbo R enters R800 DRAM mode and restores its prior CPU mode on exit. Other machines stay on Z80; `:r800` reports that turbo R is required. Non-turbo-R tests use 256KiB mapper RAM and a DOS2 cartridge.

Use `VIMAP filename` for standard mapper RAM (256KiB minimum, no V9968 required). After reserving undo history, available DOS2 user segments are shared between two documents and a cut/copy register; capacity depends on free RAM. Use `VI9IN filename` or `VI9EX filename` for internal or external V9968 storage and display (128KiB mapper RAM minimum). These versions support two 1MiB documents in VRAM and optionally use one extra 16KiB mapper segment as a cache on configurations with at least 256KiB. The external version displays on the external V9968 output.

Supported features include normal/insert modes, arrows and hjkl, line/character cut, line yank, paste across two files, literal search/replacement, undo/redo, line numbers, color presets and Z80/R800 switching. Japanese conversion requires a separately installed MSX-JE. ASCII inherits the DOS startup width (40 or 80 columns), using 256x212 or 512x212 graphics with 23 text rows. Use `:mode40` / `:mode80` to switch, and `:ascii` to return to the last ASCII width; the mapper version's Kanji layout depends on the installed driver. Native V9968 Kanji uses 512x424 flat-interlace graphics, 60 half-width columns and 23 text rows, with full 16x16 Japanese glyphs. Ctrl+Space toggles conversion in Kanji insert mode.

This is an early development release tested on official BlueMSX+ v3.2.0-alpha and a managed openMSX build, not validated on physical hardware or across Nextor configurations. It has no atomic save, recovery, regular expressions, splits, Visual mode, plugins or syntax highlighting. Files are interpreted as ASCII/Shift-JIS; CR bytes are stripped on load and LF is written as CRLF. Keep backups. `:q!` discards unsaved changes in both buffers.

Build with Python 3.10+ and SDCC (`python tools/build.py`, or pass `--sdcc`). The release was built with SDCC 4.6.0 #16555. Source is MIT-licensed; linked SDCC runtime code retains its own GPL-with-linking-exception terms. No emulators, ROMs, DOS, MSX-JE or disk images are bundled.

`u` and `Ctrl+R` undo and redo; counts and `:undo` / `:redo` are supported. Each buffer has an independent history. An insert session through Esc is one transaction. Mapper builds reserve 8KiB per buffer (4KiB with limited free segments); V9968 builds reserve 256KiB per buffer without reducing the two 1MiB document regions. If a single edit exceeds history capacity, that buffer's history is cleared with a warning; the edit remains but cannot be undone. See [Undo/Redo](docs/UNDO-ja.md).

### Disclaimer

The software is provided as-is under the [MIT license](LICENSE), without warranties, including fitness for a particular purpose. To the extent permitted by applicable law, the authors and copyright holders disclaim liability for claims or damages arising from its use or other dealings. The license text contains the governing terms; this summary adds no restrictions to the MIT grant.

This development release is not certified for physical hardware or all Nextor configurations. Emulator results do not guarantee compatibility, performance or data integrity elsewhere. Back up important files and test copies first: saving overwrites files directly, and automatic backups and crash recovery are unavailable. Undo/redo history is lost on exit. Obtain appropriate rights to the separately required BIOS, DOS, Kanji ROM/driver and MSX-JE.
