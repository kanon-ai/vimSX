# MSXSH 0.2 — openMSX / MSX-DOS2 検証記録

2026-11-24、openMSX の実際の MSX-DOS2 環境で実行しました。ホスト上の模擬 DOS 呼び出しだけによる試験ではありません。

- openMSX: 21.0-unknown / Release（ローカル導入版）
- machine: Panasonic_FS-A1ST、256KB mapper RAM
- MSX-DOS: 2.20 の起動ディスクの複製
- SDCC: 4.6.0 #16555、Z80
- MSXSH.COM: 19828 バイト
- SHA-256: `e78fd9323144086b785e0db7512bdb24cbbe187d90f802fa3e6f74ed2a34e189`

各試験で新規のディスク複製へ COM を取り込み、エミュレータ側のディスクから取り出した MSXSH.COM と配布バイナリが完全一致することを確認しています。ディスク取り込みは既存ファイルを置換しないため、MSXSH が入っていない元ディスクから始めます。

## 自動比較: 115 ケース PASS（従来の 48 ケースを含む）

標準出力と各コマンドの `$?` を期待値と比較し、必要なエラーメッセージと生成ファイルも確認しました。

- 引用符、変数展開、export / unset、条件付き実行、カレントディレクトリ、PATH。
- RET、JP 0、TERM の外部終了、終了コード 7 の伝達。
- 外部プログラムが 0200h～BFEFh を上書きした後も、シェルの変数・実行位置・状態が復帰。
- 2 段／3 段パイプ、空入力、最終段の終了コード。
- 4096 バイトの全内容が 2 段／3 段とも完全一致。
- 4097 バイトの出力はエラー 2。後段のファイルが作られず、次の独立コマンドは実行可能。
- DOS コンソール出力、DUP、CLOSE の非対応パイプ操作を検出。
- 入力／出力／追記、外部コマンドのリダイレクト、明示指定によるパイプの上書き。
- ファイルオープン失敗後に標準入出力を復元して処理を継続。
- 既存 LS / LS -l / LS -w、GREP 再帰、FIND 再帰をシェルから実行。
- 不明なコマンド、非 COM、空 COM、入れ子シェル、不正構文を拒否。
- 通常の外部 COM を 100 回起動。
- ユーザーマッパを確保する外部 COM をさらに 100 回起動。前後とも空き 2 セグメント。
- シェルの起動前／終了後は空き 6 セグメントで一致。確保した 4 セグメントを回収。

記録は [validation/v02/suite/results.json](validation/v02/suite/results.json)、期待値は [validation/v02/suite/expected.json](validation/v02/suite/expected.json) です。

## 追加した制御構文と変数

上記 115 ケースには、次の新機能の比較が含まれます。

- if / elif / else、複数行、入れ子、条件部の外部コマンドとパイプ、分岐の終了コード。
- for の順序・空リスト・空の値・引用した空白・開始時の展開・入れ子・break / continue。
- ループ内の外部実行とメモリ上書きからの復帰、変数と進行位置の保持。
- ローカル変数が子 COM に渡らず、export / export 後の再代入 / DOS から継承した変数への代入が渡ること。
- unset、変数表の 8 件上限と再利用、名前 31/32 バイト、値 255/256 バイトの境界。
- for リストの 512 バイト成功／513 バイト拒否。
- 文字列・整数・ファイル・ルートディレクトリの test と不正な式。

独立した 10 スクリプトで、fi / done 欠落、構文エラー、5 段の入れ子、96/97 ノード、4KB ブロック超過、ループ内 exit 7、改行のない最終 fi を検証しました。エラー時には未完了ブロックの本文とその後のコマンドが実行されないことを確認しています。

同じ試験で、対話入力から複数行の for / if を組み立てて実行し、入力途中の Ctrl-C で破棄して状態 130 となることも確認しました。記録は `validation/v02/control-modes` です。

DEMO.SH と CONTROL.SH の出力・終了コード、および `-c` 経由の for 実行も比較しました。記録は `validation/v02/samples` です。

## 対話操作と不足メモリ

- `-c` 実行で出力 `CLI`、終了コード 1。
- 対話入力の Backspace 編集、上矢印による履歴再実行、history 表示。
- 環境変数を設定し、外部 LS から戻った後も変数を参照可能。
- `exit` の終了コード 0、COMMAND2 のプロンプトへ戻って `echo BACKDOS` を実行。
- RAM ディスクで空きを 3 セグメントに減らすと起動を拒否し、終了コード 2。途中で確保したセグメントも回収し、空き 3 のまま。
- RAM ディスクを解除すると再起動し、`RECOVERED` を出力。
- 最終行に改行のないスクリプトでも `FINAL` を出力し正常終了。

対話入力とメモリ不足のログ・画面は `validation/v02/interactive` と `validation/v02/lowmem` にあります。矢印編集の全組合せ、日本語編集、実機、全種類のディスクエラー、任意の COM の互換性を網羅する試験ではありません。

## 再実行

Python 3、openMSX、利用権のある BIOS と DOS2 起動ディスク、および別配布の LS / GREP / FIND を用意します。

```powershell
python tests/prepare.py --work C:/test/suite --boot C:/media/dos2.dsk --search C:/tools/msx-search --ls C:/tools/msx-ls --control
openmsx -machine Panasonic_FS-A1ST -diska C:/test/suite/boot.dsk -script C:/test/suite/run.tcl
python tests/check.py C:/test/suite

python tests/interactive.py --work C:/test/interactive --boot C:/media/dos2.dsk --ls C:/tools/msx-ls
openmsx -machine Panasonic_FS-A1ST -diska C:/test/interactive/boot.dsk -script C:/test/interactive/run.tcl
python tests/lowmem.py --work C:/test/lowmem --boot C:/media/dos2.dsk
openmsx -machine Panasonic_FS-A1ST -diska C:/test/lowmem/boot.dsk -script C:/test/lowmem/run.tcl
python tests/check_modes.py C:/test/interactive C:/test/lowmem
```

各エミュレータが終了してから checker を実行してください。`--work` は存在しない新しいディレクトリにします。Tcl のデバッガフックは出力と終了コードを観測するだけで、実行結果を改変しません。

追加の制御構文試験は `tests/control_modes.py --work C:/test/control --boot C:/media/dos2.dsk` で準備し、同様に生成された boot.dsk / run.tcl を openMSX で起動します。`tests/check_control_modes.py C:/test/control` で比較できます。サンプルは `tests/samples.py`（--work / --boot / --search 指定）と `tests/check_samples.py` を使います。

保存済み記録も次のコマンドで再比較できます。

```powershell
python tests/check.py validation/v02/suite
python tests/check_modes.py validation/v02/interactive validation/v02/lowmem
python tests/check_control_modes.py validation/v02/control-modes
python tests/check_samples.py validation/v02/samples
```

既存 LS、GREP、FIND の SHA-256 は作業前の値と一致し、それらのソースや実装は変更していません。

別ディレクトリでのソースからの再ビルドでも、検証済み COM と全バイト一致しました。ローカル環境に依存する起動ランチャーは配布に含めていません。

0.2 はシェルの制御構文・変数・関連テスト／文書の変更で、常駐ローダ／パイプの resident.s は 0.1 と同一です。
