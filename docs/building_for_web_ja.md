# Web 版ビルド手順 (WASM)

このドキュメントは、Schism Tracker を Emscripten でブラウザ実行するための、
実用的な第一段階の手順をまとめたものです。

## 現在の Web ポート用スキャフォールドの状態

このリポジトリに追加されている `scripts/build-web.sh` は、最小構成の
ブラウザ向けビルドパイプラインを素早く立ち上げるためのスクリプトです。

- `emconfigure` / `emmake` 経由で `configure` + `make` を実行
- SDL2 バックエンドを強制
- ホスト依存バックエンド (ALSA/JACK/X11) を無効化
- ソースパスにスペースが含まれる場合の問題を自動回避
- 出力を `build-web/dist` に生成

これは段階的導入のための土台であり、本番品質の完全な Web ポートではありません。

また現状の Emscripten ビルドでは、初期移行を優先するために外部 `utf8proc`
リンクの代わりとして、最小限の内部 UTF-8 フォールバック実装を使用しています。

## 前提条件

- Emscripten SDK (`emcc`, `emconfigure`, `emmake`) が `PATH` にあること
- Schism の通常ビルドに必要なツール:
  - `autoconf`, `automake`, `libtool`, `pkg-config`, `python3`, `perl`

macOS / Linux では、emsdk インストール後に以下を実行:

```sh
source /path/to/emsdk/emsdk_env.sh
```

## ビルド

リポジトリルートで:

```sh
./scripts/build-web.sh
```

成果物は以下に出力されます:

- `build-web/dist/schismtracker.js`
- `build-web/dist/schismtracker.wasm`
- `build-web/dist/schismtracker.data` (リンカ設定によって生成される場合)
- `build-web/dist/index.html`

## ローカル実行

任意の静的 Web サーバーで実行できます。例:

```sh
cd build-web/dist
python3 -m http.server 8080
```

`http://localhost:8080` を開いてください。

## Web 特有の制約

- 音声は通常、ユーザー操作後に開始されます。
- ブラウザショートカットが Tracker のキー操作と競合する場合があります。
- ネイティブのファイルシステムパスは使えないため、import/export と
永続化ストレージの利用が前提です。
- このスキャフォールドでは初期段階の簡略化のため、スレッドは意図的に無効化しています
(COOP/COEP 設定要件を回避)。

### アプリショートカット設定

Web 版にはショートカット設定ページを追加しています:

- `System Configuration (Ctrl-F1) -> Shortcuts: Configure...` から遷移
- `Rebind` で選択項目のキー割り当てを変更
- 重複割り当ては拒否し、衝突先機能名を表示
- `Defaults / Save / Back` は画面上のボタンで操作

ブラウザ/OS によっては `Ctrl+F1` が先に予約キーとして消費される場合があります。
その場合は `Ctrl+Shift+F1` でショートカット設定画面を開いてください。

### IndexedDB 永続化 (IDBFS)

生成される `index.html` は Emscripten の IDBFS を `/persistent` にマウントし、
`HOME=/persistent` を設定します。さらに `addRunDependency` /
`removeRunDependency` を使って `syncfs(true)` 完了まで `main()` を遅延させ、
起動とストレージ状態の整合性を保ちます。

`~/.config/schism` 以下の設定と、デフォルトの module/sample ディレクトリは
この永続領域に配置されます。開いたモジュールは再利用のため
`/persistent/modules/` にコピーされます。`syncfs(false)` はページ離脱時、
タブ非表示時、定期実行で呼び出されます。IDBFS が使えない環境
（例: 一部プライベートモード）では、Open フローは `/tmp` のみへフォールバックします。

Web ツールバーには `/persistent/modules` を管理する UI もあります。
`Stored Action` のプルダウンと `Run Action` ボタンで次を実行できます:

- `Load Stored`: 選択中の保存済みモジュールをロード
- `Refresh List`: IndexedDB 側を再走査して一覧を更新
- `Rename Stored`: 選択中モジュール名を変更 (サニタイズあり)
- `Delete Stored`: 選択中モジュールを削除

保存済み一覧は「最終更新日時の新しい順」で表示されます。

表示倍率はツールバーから切り替えできます:

- `Fit Window`: ビューポートに合わせて表示 (デフォルト)
- `x1`: 固定 `640x400` 論理解像度
- `x2`: 固定 `1280x800`
- `x3`: 固定 `1920x1200`

### Web MIDI (入力/出力)

Web ランナーはブラウザの Web MIDI API を初期化し、Schism の MIDI ページに
仮想ポート `Web MIDI I/O` を1つ公開します。

- ブラウザの MIDI 入力デバイスから受けたメッセージは、Schism の MIDI イベント
  パイプラインへ転送されます。
- Schism から送出される MIDI メッセージは、利用可能なブラウザ MIDI 出力へ送信
  されます。
- ブラウザ許可とセキュアコンテキスト要件が適用されます（`localhost` は可）。

## 次の推奨ステップ

1. Chromium / Firefox で起動と音声出力を確認する
2. ドラッグ&ドロップ読み込みと明示的な save/export フローを拡張する
3. 必要箇所で `__EMSCRIPTEN__` による機能ガードを追加する
4. この Web ビルドスクリプトを PR で実行する CI ジョブを追加する

