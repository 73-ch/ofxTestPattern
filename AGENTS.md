# ofxTestPattern — Agent Instructions

このリポジトリで作業するすべてのAIエージェント(Claude Code / Codex / Antigravity)向けの規約。
共通の開発ワークフローは `~/dev/agent-base/instructions/dev-workflow.md` に従うこと(必読)。

## プロジェクト概要

マルチ面インスタレーション(プロジェクション / LED)向けのテストパターン生成・配置・
出力を自動化する openFrameworks アドオン。`layout.json` を単一の真実として、
FBO管理・パターン描画・Slice合成・PNG/データ一式のエクスポート・自動パッキングを行う。

設計の正本は `DESIGN.md`。仕様に関わる変更は必ず DESIGN.md を先に更新してから実装する。

## 技術スタック

- C++ / openFrameworks 0.12.1(macOS で開発・検証)
- 依存は oF コアのみ(ofJson / ofTrueTypeFont / ofFbo)。外部アドオン・ライブラリを追加しない

## コマンド

```bash
cd example
make -j8 Debug          # ビルド(リポジトリは OF_ROOT/addons/ 直下に置くこと)
make RunDebug           # 起動

# ヘッドレス検証(30フレーム描画→書き出し→自動終了)
cd bin/example_debug.app/Contents/MacOS
OFXTP_EXPORT=1 ./example_debug                            # exportAll 実行
OFXTP_SNAPSHOT=1 ./example_debug                          # 出力FBOをPNG保存
OFXTP_LAYOUT=layout-autopack.json ./example_debug         # autoPack デモ
```

PRを出す前に `make -j8 Debug` が通ること、`OFXTP_EXPORT=1` 実行で
検証エラー(validation error)が出ないことを確認する。

## ディレクトリ構成

```
src/
  ofxTestPattern.h/.cpp      ファサード(公開API)
  ofxTpTypes.h               Surface / Output / Slice / Layout 定義
  ofxTpPatternRenderer.*     パターン描画
  ofxTpPacker.*              自動パッキング
  ofxTpExporter.*            PNG / crops.json / crops.csv 書き出し
example/                     サンプル(4面5出力、bin/data/layout.json)
DESIGN.md                    設計ドキュメント(正本)
```

## プロジェクト固有ルール

- **ピクセルパーフェクト原則**: 1pxラインは整数座標の矩形で描画、Slice合成は等倍texel転写。
  暗黙のスケーリング・フィルタリングを入れない。
- `layout.json` のスキーマを変える場合は DESIGN.md §2 と README(英日両方)を同時に更新する。
- 生成物(`bin/data/export/`, `snap_*.png`, `layout-packed.json`, `*.app`, `obj/`)はコミットしない。

## 触ってはいけない領域

- **実案件のデータ(案件名・実寸・レイアウト)をこのリポジトリに入れない。**
  example のデータは架空の "demo" 構成のみ。実案件の layout.json は案件側リポジトリ/フォルダで管理する。
