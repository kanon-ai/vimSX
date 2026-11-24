# MSXSH 0.2 — MSX-DOS2 でシェル遊び

Linux ターミナルの気分を味わうための、小さな bash 風シェルです。GNU Bash の移植ではなく、独立した限定実装です。**実用性・安全性は未確定のジョーク／実験用ソフト**として扱ってください。無保証で、損失について責任を負いません。重要なデータやオリジナルのフロッピーでは使わず、コピーした環境で遊んでください。

[スクリプト構文ガイド](SYNTAX-ja.md) / [MSXSH.COM](MSXSH.COM) / [ソース・サンプル付き ZIP](https://github.com/kanon-ai/vimSX/raw/refs/heads/main/extras/msxsh/MSXSH-0.2.zip)

## 起動

MSX-DOS2 のディスクへ `MSXSH.COM` をコピーして実行します。

```text
A>MSXSH
A:/$ echo hello
hello
A:/$ exit
A>
```

`MSXSH -c "echo hello; pwd"` で一行実行、`MSXSH TEST.SH` でスクリプトを実行できます。スクリプトの引数、BAT 実行は未対応です。

MSX-DOS2、プライマリメモリマッパに空き 4 セグメント（64KB）、TPA 上端 D000h 以上が必要です。openMSX の Panasonic_FS-A1ST（256KB）で検証しています。実機・Nextor・他機種の動作保証はありません。

## できること

- 内蔵コマンド: `cd`、`pwd`、`echo [-n]`、`export NAME[=value]`、`unset NAME`、`true`、`false`、`history`、`help`、`exit [0..255]`
- 制御構文: `if` / `elif` / `else` / `fi`、`for NAME in ...; do ...; done`、`break` / `continue`
- 変数: `NAME=value`、`export NAME`、`unset NAME`。条件式: `test` / `[ ... ]`
- 外部 COM: カレントディレクトリ、次に MSX-DOS の `PATH`（`;` 区切り）から検索
- `;`、`&&`、`||`、単引用符・二重引用符、`$NAME`、`${NAME}`、`$?`
- `<`、`>`、`>>` による標準入力・標準出力のファイル指定
- `|` によるマッパ RAM 上の小容量パイプ
- 対話入力の左右移動、挿入、Backspace、Delete、上下キーの履歴（8 件）、Ctrl-A/E、Ctrl-C、空行で Ctrl-D

```sh
ls
ls -l
ls -w
find . -name '*.TXT' | grep README
grep -r TODO . | grep -i fix
echo hello | grep hello | grep -n hello
grep alpha INPUT.TXT >MATCH.TXT
echo added >>MATCH.TXT
export PATH='A:/BIN;B:/BIN'
false || echo failed
```

LS / GREP / FIND は別のプログラムです。同じディスクか `PATH` 上に置いてください。シェル側ではワイルドカードを展開せず、文字列のまま各コマンドへ渡します。`export` は DOS の環境変数を書き換えます。

## if / for と変数

```sh
NAME=MSX
if [ -n "$NAME" ]; then
    echo "Hello $NAME"
else
    echo "No name"
fi

for item in apple banana "two words"; do
    if [ "$item" = banana ]; then
        continue
    fi
    echo "$item"
done
```

条件はコマンドの終了状態で判断します（0 が真）。`elif` と入れ子にも対応し、条件部には GREP やパイプも書けます。`for` の値は開始時に一度だけ展開し、繰り返すたびに変数を設定します。ループ変数は終了後も最後の値を保持します。`break` / `continue` は一番内側のループに作用し、数値引数は未対応です。

`NAME=value` はシェル内の変数を設定します。`export NAME` または `export NAME=value` で DOS の環境にも渡し、その後の代入も環境へ反映します。既に DOS 環境に存在する非空の変数への代入も環境へ反映します。`unset NAME` はシェル変数と DOS の環境の両方から削除します。ローカル変数は終了時に消えますが、DOS 環境への変更は残ります。

値の参照は `$NAME` / `${NAME}`、直前の終了状態は `$?` です。単引用符では展開せず、二重引用符では展開します。複数代入も可能ですが、同じコマンド中の値は代入前にまとめて展開します。`NAME=value command` のようなコマンド限定代入、算術展開、`${NAME:-default}`、位置パラメータ、`read` は未対応です。

`test` / `[ ... ]` は文字列の `=` / `==` / `!=`、`-n` / `-z`、ファイルの `-e` / `-f` / `-d`、整数の `-eq` / `-ne` / `-lt` / `-le` / `-gt` / `-ge`、先頭の `!` に対応します。整数は -32768～32767。`-a` / `-o` の代わりに複数の test を `&&` / `||` でつないでください。

制御ブロックは最大 4095 バイト（改行・コメントを含む）、入れ子 4 段（elif も 1 段）、内部ノード 96 個です。コマンド／for ヘッダは各 255 バイト、for の展開済みリストは区切り NUL を含め 512 バイトです。シェルが保持する変数は 8 個、名前 31 バイト、値 255 バイトまでです。空きは `unset` で再利用できます。

スクリプトはブロックを最後まで構文確認してから実行します。不完全なブロック、制御構文の誤り、ブロック上限超過は状態 2 でスクリプトを停止し、残りの本文を単独コマンドとして実行しません。対話入力では `> ` が続きを求めるプロンプトで、Ctrl-C で入力中のブロックを破棄できます。

制御ブロック全体を `|` / `&&` / `||` / リダイレクトにつなぐ構文は未対応です。これらは条件部または本文中の通常コマンドに指定してください。複数行の引用文字列や引用中の改行は未対応です。

[examples/CONTROL.SH](examples/CONTROL.SH) を MSXSH.COM と GREP.COM のあるディレクトリへ置き、DOS から `MSXSH CONTROL.SH` で試せます。サンプルで使う NAME / fruit / pattern / DEMO_MODE は未使用にしてください。

## パイプの仕様

**1 段の出力は最大 4096 バイト**。マッパの 16KB セグメント内に 4KB のバッファを二つ置き、交互に使います。前段の終了後に次段を実行する逐次方式で、ディスクの一時ファイルは作りません。ファイルへの明示的な `>` / `>>` は通常どおりディスクに書きます。

4096 バイトは通り、4097 バイト以上に達する書き込みはエラーになります。その書き込みは保存せず、残りの段を実行せず、シェルの終了状態を 2 にします。既に実行したコマンドの副作用は取り消しません。後続の別コマンドは実行できます。

対応するのは DOS2 のファイルハンドル 0 / 1 に対する READ / WRITE を使うプログラムです。パイプを DUP・CLOSE・入力 SEEK する操作や DOS1 コンソール関数の使用はエラーにします。BIOS 直接出力、画面の直接操作、独自のハンドル操作には対応しません。任意の DOS アプリケーションを透明に接続できるものではありません。

パイプの通常の終了状態は最終段の値です。`pipefail` はありません。`<` / `>` の明示指定はその段のパイプ入力／出力より優先します。`cd` / `export` / `unset` / `exit` / `break` / `continue` と変数代入はパイプ中ではエラーです。

## 制限

- GNU Bash / POSIX sh 互換を保証しません。`while` / `until` / `case` / 関数 / 配列 / 算術展開 / コマンド置換 / サブシェル / ジョブ制御 / バックグラウンド処理はありません。
- 行 255 バイト、64 トークン、32 引数、展開領域 1024 バイト、外部 COM のコマンドテール 126 バイト。外部引数中のリテラル二重引用符は未対応です。
- 変数展開後の単語分割はしません。代入は `NAME=value` を使います。大文字小文字・ファイル名制約は DOS 側に従います。
- 外部 COM は最大 48640 バイト。シェルを含む下位 48KB を退避・復帰しますが、外部プログラムが常駐領域や DOS のシステム領域を壊した場合は復帰できません。
- FCB 引数の自動生成、シェルの入れ子、常駐プログラム、COM 以外の実行形式は未対応です。
- 入力編集はバイト単位です。日本語の文字単位編集、折り返し行のカーソル制御には対応していません。
- `2>`、`2>&1`、ヒアドキュメント、エスケープによる改行継続はありません。
- 標準入力がコンソールの外部コマンドは入力待ちになることがあります。パイプは同時実行ではありません。

## ビルドと検証

Windows、Python 3、SDCC 4.6.0 でビルドします。

```powershell
python build.py --sdcc C:/path/to/sdcc/bin/sdcc.exe
```

[DESIGN.md](DESIGN.md) に退避・復帰の設計、[VALIDATION.md](VALIDATION.md) に実行検証の結果と再現手順を記載しています。テストには利用者が用意した DOS2 起動ディスク、BIOS、openMSX が必要です。これらの実行ファイル・ROM・DOS ファイルは配布に含めません。

独自コードは [MIT License](LICENSE.txt)、SDCC ランタイムについては [THIRD_PARTY.md](THIRD_PARTY.md) を参照してください。

## 免責

本ソフトウェアとサンプルは、Linux ターミナルの気分を味わうためのジョーク／実験用のおまけです。**実用性・安全性は未確定**であり、重要なデータの処理や実務での利用に適するとは保証しません。破棄してよいコピーやエミュレータで試してください。`>` は既存の出力先を上書きし、外部コマンドにもそれぞれの副作用があります。データの消失・破損、停止などからの復旧機能はありません。

MIT ライセンスに従い「現状のまま」提供し、動作の正確性、互換性、特定目的への適合性、データの完全性等を保証しません。適用法令で認められる範囲で、作者・著作権者は使用等に起因する請求、損害その他の責任を負いません。正式な条件は [LICENSE.txt](LICENSE.txt) の英文を参照してください。この説明は MIT の利用許諾に追加の制限を設けるものではありません。

検証は [記録した openMSX 環境](VALIDATION.md) の範囲に限り、実機・Nextor・任意の外部 COM との互換性を保証しません。必要な BIOS・DOS 等はご自身で適切な利用権限を確保してください。これらは同梱していません。

The software and samples are playful experiments, with uncertain practical usefulness and safety. They are provided as-is under the MIT license, without warranties. To the extent permitted by applicable law, authors and copyright holders disclaim liability arising from use. The license text governs; this summary adds no restrictions to its grant. Test disposable copies, not important data. Emulator tests do not establish physical-hardware compatibility or data safety.
