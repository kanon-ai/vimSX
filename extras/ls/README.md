# おまけ：MSX-DOS2用 LS

**vimSXと一緒に、MSXでLinuxターミナルの気分を味わうためのおまけです。**
MSX-DOS2で使える小さなファイル一覧コマンドです。Linuxやシェルを移植したものではなく、GNU `ls` の完全互換でもありません。

[LS.COMをダウンロード](https://github.com/kanon-ai/vimSX/raw/refs/heads/main/extras/ls/LS.COM)し、ご自身のMSX-DOS2ディスクのカレントディレクトリ、またはPATHの通った場所へコピーしてください。V9968は不要です。vimSX本体とは独立して動作します。

```text
ls
ls -l
ls -w
ls -w 40
ls -la
ls -lh
ls B:\TOOLS\*.COM
ls -1 > FILES.TXT
```

| 指定 | 表示・動作 |
|---|---|
| なし / `-C` | 名前順、縦方向に並べる複数列表示 |
| `-l` | DOS属性・サイズ・更新日・時刻・名前 |
| `-w [数値]` | 複数列表示。数値があれば幅を指定（1～255桁） |
| `-1` / `-x` | 1行1件 / 横方向に並べる複数列表示 |
| `-a` / `-A` | 隠し・システムも表示 / 同左、ただし `.` と `..` は除外 |
| `-r` / `-t` / `-S` / `-U` | 逆順 / 更新日時の新しい順 / サイズの大きい順 / ディスク上の順序 |
| `-h` | `-l` のサイズを1024単位の整数K/M/Gに丸める |
| `-F` / `-p` | ディレクトリの末尾に `/` を付加 |
| `-d` | ディレクトリ自身を表示 |
| `--help` / `--version` / `--` | ヘルプ / バージョン / オプション終了 |

`-la` などの組み合わせ、複数パス、`*` と `?`、引用符付きパスに対応します。オプションは大小文字を区別します。COMMAND2の `UPPER=ON` 時も元の入力を取得します。

画面幅はDOSから取得します。標準出力がディスクファイルなら既定は1行1件です。`-w` を明示すればリダイレクト時も複数列にできます。
同じ場所に [TRYLS.BAT](TRYLS.BAT) を置き、`TRYLS` と入力すると、キーを押しながら各表示を順に試せます。

## Linux版との違いと制限

- Linuxの `-w` は幅の指定です。この版は数値なしの `-w` もワイド表示として受け付けます。`-l` と併用した場合は詳細表示です。
- `-l` の左5桁はDOS属性：`d`=ディレクトリ、`r`=読み取り専用、`h`=隠し、`s`=システム、`a`=アーカイブ。UNIXの権限・所有者・グループ・リンク数はありません。
- 8.3形式のファイル名とDOSの照合規則を使用します。名前のソートはバイト順です。ロングファイル名、再帰表示 `-R`、色分け、シンボリックリンクには非対応です。
- 1回の検索につき最大1,024件。超過するとエラーになり、その検索結果は出力しません。引数は32個、コマンド行は127バイトまでです。
- MSX-DOS2と通常メモリ約44KBが必要です。MSX-DOS1では動作しません。
- 40桁画面では詳細行が折り返されます。読みやすくするにはDOSで `MODE 80` を実行してください。
- 日時未設定なら `---------- --:--` と表示します。ルートなどを `-d` で表示すると、サイズ0・日時未設定として扱います。
- 終了コードは成功0、引数・検索・出力エラー2です。DOSエラーは16進コードで表示します。

## 検証

2026-11-23に、openMSX 21.0のローカル導入版、Panasonic FS-A1ST、DOSカーネル2.30、MSXDOS2.SYS / COMMAND2.COM 2.20で、**実際のDOS2を起動して `ls`・`ls -l`・`ls -w` の表示とプロンプトへの復帰を確認**しました。`*.TXT` を指定したリダイレクト出力と、20・3,000・0バイトのサイズ表示も一致しました。

[通常表示](validation/11-ls.png) / [詳細表示](validation/12-ls-l.png) / [ワイド表示](validation/13-ls-w.png) / [検証記録](validation/RESULT-ja.txt)

別途、生成したCOMをZ80 CPUエミュレータで実行する69項目のテストも通過しています。こちらはDOS APIをモデル化したテストです。実機・Nextorの各構成、全追加オプションの実DOS2動作、実機速度は未検証です。

## ビルド・テスト

Python 3とZ80対応SDCCを用意し、このディレクトリで実行します。使用した版はSDCC 4.6.0 #16555です。

```text
python build.py --sdcc C:\path\to\sdcc\bin\sdcc.exe
```

生成物は `LS.COM`、中間ファイルは `build/` です。WindowsでCPUモデルテストを再実行する場合はGCCも用意し、次を実行します。

```text
gcc -O2 tests/runner.c -o tests/runner.exe
python tests/verify.py
```

COMのSHA-256は [SHA256SUMS.txt](SHA256SUMS.txt) を参照してください。
このおまけにはエミュレータ、BIOS、DOSシステム、起動ディスク、個人環境用のWindowsランチャーは含めていません。

## ライセンス・免責

独自コードは [MITライセンス](LICENSE.txt) です。SDCCランタイムとテスト用CPUコアには、それぞれの原ライセンスが適用されます。[第三者ライセンス](THIRD_PARTY.md)を参照してください。

本ソフトウェアは、趣味・実験用のおまけとして「現状のまま」提供します。動作の正確性、互換性、特定目的への適合性、データの完全性を保証しません。適用法令で認められる範囲で、作者・著作権者は、使用等に起因する請求、データの消失・破損その他の損害について責任を負いません。正式な条件はMITライセンスの英文を参照してください。この説明はMITの利用許諾に追加の制限を設けるものではありません。

LS自身は一覧対象のファイルを変更しませんが、`>` によるリダイレクトではDOSが出力先ファイルを作成・上書きします。重要なデータはバックアップし、コピーで試してください。エミュレータでの検証は、すべての環境での動作や実機速度を保証するものではありません。

Linux、GNU、MSXなどの名称は操作感・対応環境を説明するためのもので、各権利者による公式製品や承認を意味しません。

## English

A small MSX-DOS2 bonus for enjoying a little Linux-terminal atmosphere alongside vimSX. It is an independent directory-listing utility, not Linux, a shell, or a fully compatible GNU `ls` port. Copy `LS.COM` onto your own DOS2 disk and try `ls`, `ls -l`, or `ls -w`. Bare `-w` is a convenience extension; `-l` shows DOS attributes rather than Unix permissions. No V9968 is required.

Tested with real DOS2 running inside openMSX (FS-A1ST, kernel 2.30, system and COMMAND2 2.20); physical hardware and Nextor configurations remain unverified. Supplied as-is, without warranty, under the MIT license, with third-party components retaining their licenses. To the extent permitted by law, the authors disclaim liability for claims or damages arising from use. Back up important data; output redirection can overwrite its destination.
