# Schism Tracker ショートカット監査一覧（日本語 / 再監査版）

このファイルは `docs/shortcuts_ja.md` 掲載項目を、現行実装に対して再監査した結果です。  
（監査対象: `shortcuts_ja.md` 掲載項目 69 件）

- `✅` 実装確認済み（該当キー分岐を確認）
- `⚠️` 条件付き（画面状態/モード依存、または説明表現との乖離あり）

## 監査対象ソース

- `schism/page.c`（グローバル・アプリ・再生パラメータ）
- `schism/page_patedit.c`（パターンエディタ）
- `schism/shortcuts.c`（Web 版ショートカット再割り当てテーブル）

---

## 1) グローバル操作（25件）


| ショートカット                  | 判定  | 実装根拠                                   |
| ------------------------ | --- | -------------------------------------- |
| `F1` ヘルプ                 | ✅   | `schism/page.c` `SCHISM_KEYSYM_F1`     |
| `Shift+F1` MIDI画面        | ✅   | 同上 `SHIFT` 分岐                          |
| `Ctrl+F1` システム設定         | ✅   | 同上 `CTRL` 分岐                           |
| `F2` パターンエディタ            | ✅   | `SCHISM_KEYSYM_F2`                     |
| `F3` サンプルリスト             | ✅   | `SCHISM_KEYSYM_F3`                     |
| `Ctrl+F3` サンプルライブラリ      | ✅   | 同上 `CTRL` 分岐                           |
| `F4` インストゥルメントリスト        | ✅   | `SCHISM_KEYSYM_F4`                     |
| `Ctrl+F4` インストゥルメントライブラリ | ✅   | 同上 `CTRL` 分岐                           |
| `F5` 再生情報 / 曲再生          | ✅   | `SCHISM_KEYSYM_F5`                     |
| `Ctrl+F5` 曲再生            | ✅   | 同上 `CTRL` で `song_start()`             |
| `F6` 現在パターン再生            | ✅   | `SCHISM_KEYSYM_F6`                     |
| `Shift+F6` 現在オーダーから再生    | ✅   | 同上 `SHIFT` 分岐                          |
| `F7` マーク位置または現在行から再生     | ✅   | `SCHISM_KEYSYM_F7`                     |
| `Shift+F8` 一時停止/再開       | ✅   | `SCHISM_KEYSYM_F8` `SHIFT`             |
| `F8` 停止                  | ✅   | 同上 通常分岐で `song_stop()`                 |
| `F9` 読み込み                | ✅   | `SCHISM_KEYSYM_F9`                     |
| `Ctrl+L` 読み込み            | ✅   | `SCHISM_KEYSYM_l` + `CTRL`             |
| `Ctrl+R` 読み込み            | ✅   | `SCHISM_KEYSYM_r` + `CTRL`             |
| `Shift+F9` メッセージ         | ✅   | `SCHISM_KEYSYM_F9` `SHIFT`             |
| `F10` 保存                 | ✅   | `SCHISM_KEYSYM_F10`                    |
| `Ctrl+W` 保存画面            | ✅   | `SCHISM_KEYSYM_w` + `CTRL`             |
| `Shift+F10` エクスポート       | ✅   | `SCHISM_KEYSYM_F10` `SHIFT`            |
| `F11` Order/Panning      | ✅   | `SCHISM_KEYSYM_F11`                    |
| `2*F11` Order/Volume     | ⚠️  | 連打専用ではなく `F11` で `PANNING/VOLUMES` トグル |
| `Ctrl+F11` ログ            | ✅   | `SCHISM_KEYSYM_F11` `CTRL`             |
| `F12` 曲変数/ディレクトリ         | ✅   | `SCHISM_KEYSYM_F12`                    |
| `Ctrl+F12` パレット          | ✅   | `SCHISM_KEYSYM_F12` `CTRL`             |
| `Shift+F12` フォント         | ✅   | `SCHISM_KEYSYM_F12` `SHIFT`            |


---

## 2) 再生パラメータ系（4件）


| ショートカット                    | 判定  | 実装根拠                                               |
| -------------------------- | --- | -------------------------------------------------- |
| `{` `}` 再生スピード減/増          | ✅   | `schism/page.c` `LEFTBRACKET/RIGHTBRACKET + SHIFT` |
| `Ctrl+[ ]` テンポ減/増          | ✅   | 同上 `CTRL` 分岐                                       |
| `[ ]` グローバル音量減/増           | ✅   | 同上 修飾なし分岐                                          |
| `Alt+F1`～`Alt+F8` チャンネルトグル | ✅   | `ALT + F1..F8` ミュート分岐                              |


---

## 3) アプリ操作（9件）


| ショートカット                  | 判定  | 実装根拠                     |
| ------------------------ | --- | ------------------------ |
| `Ctrl+N` 新規ソング           | ✅   | `SCHISM_KEYSYM_n + CTRL` |
| `Ctrl+S` 保存              | ✅   | `SCHISM_KEYSYM_s + CTRL` |
| `Ctrl+Q` 終了              | ✅   | `SCHISM_KEYSYM_q + CTRL` |
| `Ctrl+Shift+Q` 確認なし終了    | ✅   | 同上 `SHIFT` 分岐            |
| `Ctrl+Alt+Enter` フルスクリーン | ✅   | `RETURN + CTRL + ALT`    |
| `Ctrl+M` マウスカーソル切替       | ✅   | `SCHISM_KEYSYM_m + CTRL` |
| `Ctrl+D` 入力グラブ切替         | ✅   | `SCHISM_KEYSYM_d + CTRL` |
| `Ctrl+I` サウンド再初期化        | ✅   | `SCHISM_KEYSYM_i + CTRL` |
| `Ctrl+E` 画面リフレッシュ        | ✅   | `SCHISM_KEYSYM_e + CTRL` |


---

## 4) パターンエディタ主要（31件）

### 4-1) 移動（7件）


| ショートカット                 | 判定  | 実装根拠                              |
| ----------------------- | --- | --------------------------------- |
| `Up / Down`             | ✅   | `schism/page_patedit.c` `UP/DOWN` |
| `Ctrl+Home / Ctrl+End`  | ✅   | `HOME/END` の `CTRL` 分岐            |
| `Left / Right`          | ✅   | `LEFT/RIGHT`                      |
| `Tab / Shift+Tab`       | ✅   | `TAB`                             |
| `PgUp / PgDn`           | ✅   | `PAGEUP/PAGEDOWN`                 |
| `Ctrl+PgUp / Ctrl+PgDn` | ✅   | `PAGEUP/PAGEDOWN` の `CTRL` 分岐     |
| `Home / End`            | ✅   | `HOME/END`                        |


### 4-2) 入力・編集（11件）


| ショートカット               | 判定  | 実装根拠                                 |
| --------------------- | --- | ------------------------------------ |
| `0-9` 数値入力            | ⚠️  | 列・編集状態依存 (`pattern_editor_insert` 系) |
| `0-9, A-F` エフェクト値     | ⚠️  | 同上（文脈依存）                             |
| `A-Z` エフェクト入力         | ⚠️  | 同上 + `kbd_get_effect_number()`       |
| `.` クリア               | ⚠️  | 同上（現在列/マスク依存）                        |
| ``` ノートオフ等            | ⚠️  | 同上（表示モード依存を含む）                       |
| `Shift+`` ノートフェード     | ⚠️  | 同上（修飾・文脈依存）                          |
| `Space` 再利用           | ✅   | `SPACE` 分岐                           |
| `Enter` デフォルト取得       | ✅   | `RETURN` 分岐                          |
| `Ins / Del`           | ✅   | `INSERT/DELETE`                      |
| `Alt+Ins / Alt+Del`   | ✅   | `INSERT/DELETE` の `ALT` 分岐           |
| `Ctrl+Backspace` Undo | ✅   | `BACKSPACE + CTRL`                   |


### 4-3) 再生（4件）


| ショートカット           | 判定  | 実装根拠              |
| ----------------- | --- | ----------------- |
| `4` カーソルノート再生     | ✅   | `SCHISM_KEYSYM_4` |
| `8` 現在行再生         | ✅   | `SCHISM_KEYSYM_8` |
| `Ctrl+F6` 現在行から再生 | ✅   | `CTRL+F6` 分岐      |
| `Ctrl+F7` 再生マーク   | ✅   | `CTRL+F7` 分岐      |


### 4-4) ブロック（9件）


| ショートカット          | 判定  | 実装根拠                                  |
| ---------------- | --- | ------------------------------------- |
| `Alt+B` 開始マーク    | ✅   | `pattern_editor_handle_alt_key()` `b` |
| `Alt+E` 終了マーク    | ✅   | 同上 `e`                                |
| `Alt+L` 列/パターン選択 | ✅   | 同上 `l`                                |
| `Shift+矢印` 範囲選択  | ✅   | `SHIFT` 選択分岐                          |
| `Alt+U` マーク解除    | ✅   | `pattern_editor_handle_alt_key()` `u` |
| `Alt+C` コピー      | ✅   | 同上 `c`                                |
| `Alt+P` 貼り付け     | ✅   | 同上 `p`                                |
| `Alt+O` 上書き貼り付け  | ✅   | 同上 `o`                                |
| `Alt+Z` カット      | ✅   | 同上 `z`                                |


---

## 監査サマリ

- 監査対象: `docs/shortcuts_ja.md` 掲載 69 項目
- `✅ 実装確認済み`: 62
- `⚠️ 条件付き`: 7

条件付き項目は「未実装」ではなく、編集コンテキストに依存して説明が1対1にならない項目です。

## Web 再割り当て対応状況

- `System Configuration -> Shortcuts -> Configure...` から主要ショートカットを再割り当て可能
- 重複割り当ては拒否し、衝突先機能名を表示
- Rebind 中は `Enter` 割り当て不可
- 画面表示はグループ区切りとスクロール（`PgUp/PgDn`）に対応

## 備考

- 監査判定は `helptext` より実装 (`schism/page*.c`, `schism/shortcuts.c`) を優先
- Web 版では OS/ブラウザ予約キーにより一部キーが先に消費される可能性あり（実装有無とは別問題）

