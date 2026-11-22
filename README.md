# vimSX

MSX-DOS2向けの小型Vi風テキストエディタです。MSX turbo RとV9968の4MiB VRAMを使い、英数字・Shift-JISの文章やソースファイルを編集します。独立した実装であり、Vim本体の移植や完全互換版ではありません。

## 動作環境と起動

- MSX turbo R（Z80 / R800切り替え対応）。起動時はR800 DRAMモードを選択し、終了時に元のCPU設定へ戻します。
- MSX-DOS2互換環境、V9968の4MiB VRAM拡張。通常のV9938/V9958だけの環境には対応しません。
- 日本語表示・変換には、別途適切に導入した漢字ドライバとMSX-JEが必要です。
- 検証はカスタムBlueMSX+ / openMSX環境で実施しました。実機およびNextorの各構成での動作は未検証です。

`bin` のCOMファイルを、ご自身のDOSディスクへコピーして実行してください。

```text
VIINT MAIN.C
VIEXT NOTE.TXT
```

`VIINT.COM` は内部V9968（98h）、`VIEXT.COM` は外部V9968（88h）を本文保存に使います。**外部版でも画面表示には内部VDPを使います。** 外部V9968側に画面を出す版ではありません。ファイル名を省略すると空の文書を開き、保存名は `TEXT.TXT` になります。

配布物にエミュレータ、BIOS、DOS、MSX-JE、ディスクイメージ、フォントROMは含みません。ASCIIフォントは実行環境のBIOSから読み取ります。

## 基本操作

| 操作 | キー／コマンド |
|---|---|
| 移動 | `h j k l` またはカーソルキー、`0`、`$` |
| 入力 | `i`、`a`、`A`。`Esc`でNORMALへ |
| 削除・コピー・貼り付け | `x`、`dd`、`yy`、`p`、`P`。`3dd`、`2p`なども対応 |
| 検索 | `/文字列`、`n` |
| 置換 | `:s/old/new/`、`:s/old/new/g`、`:%s/old/new/g` |
| 行番号 | `:set number` / `:set nonumber` |
| 2ファイルを開く・切り替える | `:e SECOND.TXT`、`:bn` / `:bp`、`:b1` / `:b2` |
| 保存・終了 | `:w`、`:wa`、`:wq`、`:wqa`、`:q`、`:q!` |
| 表示 | `:ascii` / `:kanji` |
| 配色 | `:color 1` 青＋白、`:color 2` 黒＋白、`:color 3` 黒＋濃緑、`:color dos` |
| CPU | `:z80` / `:r800` |

コロンコマンドと検索はEnterで実行します。ASCIIはSCREEN 5上の40桁×25行構成（本文23行、区切り、状態・コマンド行）です。KANJI画面の桁数・行数は漢字ドライバに依存します。日本語変換はKANJIのINSERT状態でMSX-JEを使用します。検証環境ではCtrl+Spaceで変換入力を開始し、Spaceで候補、Enterで確定、Ctrl+Spaceで終了しました。ホストWindowsのIMEではありません。

操作の詳細は [コマンド一覧](docs/COMMANDS-ja.txt) を参照してください。

## VRAMの使い方

| アドレス | 用途 |
|---|---|
| `000000–03FFFF` | 画面領域として予約（256KiB） |
| `040000–13FFFF` | 文書1のギャップバッファ（1MiB） |
| `140000–23FFFF` | 文書2のギャップバッファ（1MiB） |
| `240000`以降 | 共通コピー／カットレジスタ（最大1MiBの本文＋行末LF） |

本文は合計2MiBが上限です。残る4MiB内の領域をすべて使い切る構成ではありません。メインRAMには1KiBのVRAMページキャッシュ、フォント、描画差分、管理情報などを置きます。

## 現状の制限

- 初期公開の開発版です。元ファイルのコピーで試してください。保存は直接上書きで、原子的な置き換え・自動バックアップ・クラッシュ復旧はありません。保存中の障害でファイルが不完全になる可能性があります。
- Undo/Redo、正規表現、Visualモード、分割表示、Vimプラグイン、シンタックス強調表示は未実装です。実装済みコマンドは上記一覧の範囲です。
- 文字コードはASCII / Shift-JISを想定します。UTF-8やバイナリの編集用ではありません。読み込み時にCRを除去し、保存時にLFをCRLFへ変換します。改行形式をそのまま保持する機能はありません。
- 最大2ファイルです。両方を開いた後に3つ目へ入れ替える機能はありません。`:q!` は両ファイルの未保存変更を破棄します。
- 変換候補や連文節変換の挙動はMSX-JE／辞書に依存します。エディタ独自の変換エンジンはありません。
- Z80はR800より遅く、長い行や画面内の文字数によって操作の待ち時間が増えます。既知の表示・入力問題がすべて解消したとは保証しません。

## ビルド

Python 3.10以降とZ80対応SDCCを用意してください。検証済みは **SDCC 4.6.0 #16555** です。

```sh
python tools/build.py
```

SDCCがPATHにない場合:

```sh
python tools/build.py --sdcc /path/to/sdcc
```

Windowsでは `--sdcc "C:\path\to\sdcc\bin\sdcc.exe"` と指定できます。結果は `bin/VIINT.COM` と `bin/VIEXT.COM` です。ビルドスクリプトはエミュレータの設定やシステムのキーマッピングを変更しません。

[検証記録](docs/VALIDATION.md) / [第三者ライセンス](THIRD_PARTY.md)

## ライセンス

vimSXの独自コード・ドキュメントは [MIT](LICENSE) です。SDCCランタイムの部分はそれぞれの原ライセンス（GPL v2以降＋リンク例外）に従います。MSX、Vimなどの名称は互換性・由来の説明に使用しており、各プロジェクト・権利者による公式製品や承認を示しません。

## English

vimSX is a small, independently implemented Vi-like editor for MSX-DOS2, MSX turbo R and the V9968 4MiB VRAM extension. It is not a port of Vim and does not implement full Vi/Vim compatibility.

Two documents, up to 1MiB each, and a shared cut/copy register live in VRAM. A small cache and rendering state remain in main RAM. Use `VIINT filename` for internal V9968 storage or `VIEXT filename` for external V9968 storage at ports 88h/89h. Both variants display on the internal VDP.

Supported features include normal/insert modes, arrows and hjkl, line/character cut, line yank, paste across two files, literal search/replacement, line numbers, color presets and Z80/R800 switching. Japanese display and conversion require a separately installed Kanji driver and MSX-JE. ASCII uses a 40-column graphics console; Kanji layout depends on the driver.

This is an early development release tested in custom emulators, not validated on physical hardware or across Nextor configurations. It has no undo, atomic save, recovery, regular expressions, splits, Visual mode, plugins or syntax highlighting. Files are interpreted as ASCII/Shift-JIS; CR bytes are stripped on load and LF is written as CRLF. Keep backups. `:q!` discards unsaved changes in both buffers.

Build with Python 3.10+ and SDCC (`python tools/build.py`, or pass `--sdcc`). The release was built with SDCC 4.6.0 #16555. Source is MIT-licensed; linked SDCC runtime code retains its own GPL-with-linking-exception terms. No emulators, ROMs, DOS, MSX-JE or disk images are bundled.
