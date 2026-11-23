# GREP / FIND ― Linuxターミナル気分のおまけ

**MSX-DOS2でLinuxターミナルの気分を味わうための、ジョーク／お遊び寄りの実験ツールです。実用性も安全性も不確定で、実務用・重要データ用の道具としては扱わないでください。** GNU grep / findの移植や完全互換品ではありません。

[GREP.COM](GREP.COM) と [FIND.COM](FIND.COM) を、自分で用意したDOS2ディスクのカレントディレクトリまたはPATH上へコピーします。vimSXやV9968は不要です。

## 免責

本ソフトウェアは、[MITライセンス](LICENSE.txt)に基づき現状のまま、無保証で提供します。検索結果の正確性、実用性、安全性、互換性、データの完全性、特定目的への適合性を保証しません。適用法令で認められる範囲で、作者・著作権者は利用等に起因する請求・損害その他の責任を負いません。正式な条件はライセンス本文に従い、この説明はMITの利用許諾に追加の制限を設けるものではありません。

**検索・一覧表示を目的とするプログラムでも、バグや環境差による誤動作・データ消失を否定できません。** 大事な原本・唯一のコピー・重要な実媒体には使わず、バックアップを取り、破棄してよいコピーやエミュレーター上で遊んでください。`>` による出力先ファイルの上書きにも注意してください。

openMSXの特定のDOS2環境では動作を確認しましたが、実機、他機種、Nextor、他ドライバー、すべての入力では未検証です。テスト通過は安全性の認定ではありません。

## GREP：固定文字列だけを検索

```bat
grep "ERROR" LOG.TXT
grep -n -i "hello" *.TXT
grep -r "TODO" SRC
grep -rc "TODO" SRC
grep -l "TODO" *.C
grep "alpha beta" INPUT.TXT
grep -e "-option" INPUT.TXT
grep "ERROR" < INPUT.TXT
grep "ERROR" INPUT.TXT > MATCH.OUT
```

| 指定 | 動作 |
|---|---|
| `-r` / `-R` | ディレクトリを再帰検索。ファイル省略時は現在のディレクトリ |
| `-n` | 1から始まる行番号 |
| `-i` | ASCIIバイトのA–Zとa–zを同一視 |
| `-v` | 一致しない行を選ぶ |
| `-c` | 選んだ行数をファイルごとに表示 |
| `-l` | 選んだ行を含むファイル名だけ表示（`-c`より優先） |
| `-H` / `-h` | ファイル名の接頭辞を表示／非表示 |
| `-F` | 固定文字列。省略時も同じ |
| `-e PATTERN` | パターンを明示。指定できるパターンは1個 |
| `--` | 以後をオプションとして解釈しない |

`-in`、`-rc`のようにまとめて指定できます。複数ファイル、ワイルドカード指定、再帰時はファイル名を表示します。ワイルドカードが実際には1ファイルにしか一致しなくても表示します。

`a.b`、`[x]*`などはそのままの文字列です。正規表現として解釈しません。空文字列 `""` はすべての行に一致し、空ファイルには行がないため一致しません。

通常はファイル省略で標準入力を読みます。ファイル位置の `-` も標準入力です。終了コードは **0：選んだ行あり、1：なし、2：エラー**。途中で一致してもI/Oエラーがあれば2を返します。

ファイルは512バイトずつ読み、サイズが64KBを超えても検索できます。**1行はLFを除いて最大8192バイト**（CRがあれば含む）。超えると診断を出し、そのファイルの処理を中止して終了コード2にします。LF/CRLF、末尾改行なしに対応し、出力の行末はCRLFです。Ctrl-Zを独自にEOFとして扱いません。

文字列はバイト列として比較します。同じ符号化のShift-JIS文字列は一致検索できますが、Unicode変換・文字種変換・日本語の大小変換は行いません。`-i`はASCII用です。Shift-JISの第2バイトにもASCII範囲の値があるため、日本語の厳密な比較には`-i`を使わないでください。NULを含む入力もバイトとして扱い、GNU grepのような「binary file matches」への置き換えはありません。

## FIND：ディレクトリを再帰して名前を表示

```bat
find .
find SRC -name "*.C"
find . -type d
find . -name "*.TXT" -type f -maxdepth 2
find SRC INCLUDE -name "*.H"
```

開始パスの省略時は `.`。開始点自身を深さ0として表示し、サブディレクトリへ再帰します。

| 指定 | 動作 |
|---|---|
| `-name GLOB` | 最後の名前部分を`*`・`?`で照合。DOS向けにASCIIの大小は区別しない |
| `-type f` / `-type d` | ファイルのみ／ディレクトリのみ |
| `-maxdepth N` | 深さを0～32に制限 |

条件はANDです。ファイルの内容は検索しません。正規表現、`-exec`、削除、権限・所有者検索、サイズ条件等はありません。条件に一致しなくても子ディレクトリの探索は続けます。終了コードは **0：正常終了（一致なしを含む）、2：エラー**。

## 共通の範囲・制限

- MSX-DOS2と通常のDOS2ファイルAPIを利用します。名前やパスはDOSの8.3形式を前提とし、検証済みのファイル名はASCIIです。
- 隠し・システム属性の項目も探索対象です。`.`・`..`のディレクトリエントリーは再帰対象から除きます。表示順はDOSの列挙順で、ソートしません。
- `/`は`\`へ変換。`.`のパス要素は表示に残しても、DOS APIへ渡す際に取り除きます。それ以外のパス解釈はDOS側に依存します。
- パスは最大127バイト、再帰は最大32階層です。DOS側のパス制限に先に達する場合もあります。黙って切り詰めずエラーにします。
- 引数は最大32個。空白入りの検索文字列は二重引用符で囲みます。引用符自体のエスケープ指定、複数パターン、オプションのGNU完全互換はありません。
- 本体にファイル作成・削除・書き換え用のコマンドは実装していませんが、安全性を保証するものではありません。

## ビルドと検証

```sh
python build.py --sdcc /path/to/sdcc
python tests/check.py
```

Python 3とSDCC（確認済み：4.6.0 #16555）を使用。共通の`search.c`を切り替えて2つのCOMを生成します。

`tests/check.py`は同梱されたopenMSX実行記録の出力・終了コード・リダイレクト結果を再検査します。これ自体はエミュレーターを起動しません。[検証手順と範囲](VALIDATION.md)も確認してください。

独自コードはMIT。[SDCCランタイムの原ライセンスとソース](THIRD_PARTY.md)を同梱しています。エミュレーター、BIOS、DOSシステムファイル、起動ディスクは同梱しません。

## English disclaimer

These are playful, joke-like experimental extras for enjoying a Linux-terminal feeling on MSX-DOS2. Their usefulness and safety are uncertain. They are not GNU ports, production utilities, or certified safe tools. Do not use important originals or your only copy; experiment with disposable copies and keep backups. Emulator test results do not guarantee correctness, compatibility, safety, or data integrity on other systems. Output redirection can overwrite files.

Provided AS IS under the MIT license, without warranties. To the extent permitted by applicable law, the authors and copyright holders disclaim liability for claims or damages arising from use. The license text governs; this summary adds no restriction to its grant.
