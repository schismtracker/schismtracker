# Schism Tracker ショートカット一覧（日本語）

この一覧は、現在の Schism Tracker 実装に含まれる `helptext/global-keys` と
`helptext/pattern-editor` を元に、実用度の高い項目を整理したものです。

## ショートカット再割り当て（Web 版）

- `System Configuration -> Shortcuts -> Configure...` で設定可能
- `Rebind` で選択中機能のキーを変更
- 重複割り当ては拒否され、衝突先機能名を表示
- `Enter` キーは Rebind 対象外
- 環境により `Ctrl+F1` がブラウザ/OSに奪われる場合があるため、その際は
`Ctrl+Shift+F1` でショートカット設定画面を開く

## グローバル操作

- `F1` ヘルプ（画面依存）
- `Shift+F1` MIDI 画面
- `Ctrl+F1` システム設定
- `F2` パターンエディタ
- `F3` サンプルリスト
- `Ctrl+F3` サンプルライブラリ
- `F4` インストゥルメントリスト
- `Ctrl+F4` インストゥルメントライブラリ
- `F5` 再生情報 / 曲再生
- `Ctrl+F5` 曲再生
- `F6` 現在パターン再生
- `Shift+F6` 現在オーダーから曲再生
- `F7` マーク位置または現在行から再生
- `Shift+F8` 一時停止 / 再開
- `F8` 停止
- `F9` モジュール読み込み（`Ctrl+L`/`Ctrl+R` でも可）
- `Shift+F9` メッセージエディタ
- `F10` モジュール保存（`Ctrl+W` でも可）
- `Shift+F10` エクスポート（WAV/AIFF）
- `F11` Order/Panning
- `2*F11` Order/Channel Volume
- `Ctrl+F11` ログ画面
- `F12` 曲変数 / ディレクトリ設定
- `Ctrl+F12` パレット設定
- `Shift+F12` フォントエディタ

## 再生パラメータ系

- `{` `}` 再生スピード減/増
- `Ctrl+[ ]` テンポ減/増
- `[ ]` グローバル音量減/増
- `Alt+F1` ～ `Alt+F8` チャンネル 1～8 のトグル

## アプリ操作

- `Ctrl+N` 新規ソング
- `Ctrl+S` 保存
- `Ctrl+Q` 終了
- `Ctrl+Shift+Q` 確認なし終了
- `Ctrl+Alt+Enter` フルスクリーントグル
- `Ctrl+M` マウスカーソルトグル
- `Ctrl+D` 入力グラブ切替
- `Ctrl+I` サウンドドライバ再初期化
- `Ctrl+E` 画面リフレッシュ / キャッシュ再識別

## パターンエディタ（主要）

### 移動

- `Up / Down` スキップ値単位で移動
- `Ctrl+Home / Ctrl+End` 1行ずつ移動
- `Left / Right` フィールド移動
- `Tab / Shift+Tab` ノート列単位移動
- `PgUp / PgDn` ハイライト単位移動
- `Ctrl+PgUp / Ctrl+PgDn` パターン先頭/末尾
- `Home / End` 列先頭末尾 → 行先頭末尾 → パターン先頭末尾

### 入力・編集

- `0-9` 数値入力（項目依存）
- `0-9, A-F` エフェクト値入力
- `A-Z` エフェクト入力
- `.` 現在フィールド消去
- ``` ノートオフ / パン表示トグル
- `Shift+`` ノートフェード
- `Space` 最後に使った値を再利用
- `Enter` デフォルト値取得
- `Ins / Del` 現在チャンネル行の挿入/削除
- `Alt+Ins / Alt+Del` 行全体挿入/削除
- `Ctrl+Backspace` Undo（`(*)` 対応操作）

### 再生

- `4` カーソル位置ノート再生
- `8` 現在行再生
- `Ctrl+F6` 現在行から再生
- `Ctrl+F7` 再生マーク設定/解除

### ブロック

- `Alt+B` 開始マーク
- `Alt+E` 終了マーク
- `Alt+L` 列/パターン全体マーク
- `Shift+矢印` 範囲選択
- `Alt+U` マーク解除
- `Alt+C` コピー
- `Alt+P` 貼り付け
- `Alt+O` 上書き貼り付け
- `Alt+Z` カット

## 補足

- 一部キーは画面状態・設定・モード（Classic/Multichannel など）により動作が変わります。
- 先頭に `2*` が付く項目は「同じキーを連続で2回押す」意味です。
- より詳細な全項目は以下を参照してください:
  - `helptext/global-keys`
  - `helptext/pattern-editor`