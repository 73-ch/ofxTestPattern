# ofxTestPattern

Test pattern generation, layout slicing and export for multi-surface installations
(projection, LED walls) in openFrameworks.

[English](#english) / [日本語](#日本語)

---

## English

A single `layout.json` (Surface / Output / Slice definitions) is the source of
truth: the addon allocates FBOs, renders test patterns, and composites
surface crops onto output canvases from it — pixel-perfect (1px lines drawn as
rects at integer coords, slices are 1:1 texel copies, no implicit scaling).

See [DESIGN.md](DESIGN.md) for the full design document (Japanese).

### Quick start

```cpp
ofxTestPattern tp;

void ofApp::setup() {
    tp.load("layout.json");
    tp.enableKeyShortcuts();   // t: cycle mode / p: cycle pattern / e: export
}

void ofApp::update() {
    tp.begin("Front");         // app content (shown in CONTENT / OVERLAY modes)
    // ... draw ...
    tp.end();
    tp.update();               // re-render patterns + composite outputs
}

void ofApp::draw() {
    tp.drawOutput("Front-out", 0, 0);
    // or feed tp.getOutputFbo("Front-out") to Syphon / NDI / extra windows
}
```

Concepts: a **Surface** is a logical canvas at native resolution (e.g. an LED
wall of 4160x1620). An **Output** is a physical canvas (e.g. 3840x2160). A
**Slice** maps a rect of a surface onto a position in an output — so a surface
wider than the output gets strip-split, and an oversized surface spans two
outputs, all described as data.

Display modes: `CONTENT` (app content only), `PATTERN` (test pattern), `OVERLAY`
(pattern over content). Pattern types: GRID (numbered grid + circles + color
bars + label, the default), COLOR_BARS, WHITE, CHECKER, GRADIENT_H/V, MOVING
(sweeping bar + frame counter for latency checks).

### Export

`tp.exportAll("export")` (or key `e`) writes into bin/data/export/:
per-surface pattern PNGs (`_src`), composited output PNGs (`_mapped`),
annotated versions with slice outlines / ids / coordinates baked in
(`_overlay`), transparent slice-outline layers (`_parts`), per-slice and
per-output masks (white = active area), plus `crops.json` and `crops.csv`
(flat table, loads straight into a TouchDesigner Table DAT).

`exportAll` uses each surface's current pattern type. To build a hand-off
set covering every pattern, `tp.exportAllPatterns("export")` runs the same
export once per pattern type into `export/GRID/`, `export/COLOR_BARS/`, …
and restores the previous pattern types afterwards.

### Capture

`tp.capture("capture")` (or key `c`) saves the **live picture** — not the test
pattern — as timestamped PNGs: every surface's content FBO at native
resolution (`_src_<stamp>`) and every output as currently composited, i.e.
exactly what the current mode shows (`_out_<stamp>`). Useful for handing
actual output frames to clients, media servers or content teams.

### Auto-packing

`tp.autoPack()` generates slices for surfaces that have none, packing them
into the free space of the outputs: the largest possible piece is placed
intact and the remainder is guillotine-split, mirroring how these layouts are
designed by hand. With `constraints.moduleSize` set (e.g. 128 for LED
modules), cut sizes and placement origins snap to the module grid — if
something cannot fit aligned it fails loudly with a report instead of
distorting. Call `tp.save()` to write the result back.

Validation runs on every load / autoPack: unknown references, out-of-bounds
rects, overlapping slices, uncovered surface pixels, module misalignment.

### Example

`example/` is a realistic 4-surface / 5-output setup (strip-split of a
4160px-wide wall, a floor spanning two 4K canvases). Build and run with
`make -j8 Debug && make RunDebug`. Environment variables for headless
verification (renders 30 frames, writes files, quits): `OFXTP_SNAPSHOT=1`
(run capture), `OFXTP_EXPORT=1` (run exportAll), `OFXTP_LAYOUT=<file>`
(e.g. `layout-autopack.json`, a slice-less copy demonstrating autoPack),
`OFXTP_SAVE=1` (save packed layout).

Requires only the oF core (ofJson, ofTrueTypeFont, ofFbo). Tested with
openFrameworks 0.12.1 on macOS.

---

## 日本語

マルチ面インスタレーション向けのテストパターン生成・配置・出力アドオン。
`layout.json`(Surface / Output / Slice 定義)を単一の真実として、
FBO自動生成・テストパターン描画・出力キャンバスへのSlice合成を行う。
描画はピクセルパーフェクト(1pxラインは整数座標の矩形、Slice合成は等倍texel転写)。

設計・要件・ロードマップは [DESIGN.md](DESIGN.md) を参照。

### クイックスタート

```cpp
ofxTestPattern tp;

void ofApp::setup() {
    tp.load("layout.json");
    tp.enableKeyShortcuts();   // t: モード巡回 / p: パターン種巡回 / e: エクスポート
}

void ofApp::update() {
    tp.begin("Front");         // 本番コンテンツ(CONTENT/OVERLAYモードで表示)
    // ... draw ...
    tp.end();
    tp.update();               // パターン再描画 + 出力合成
}

void ofApp::draw() {
    tp.drawOutput("Front-out", 0, 0);
    // または tp.getOutputFbo("Front-out") を Syphon/NDI/多窓へ
}
```

### エクスポート

`tp.exportAll("export")`(またはキー `e`)で bin/data/export/ に一式書き出し:
src / mapped / overlay(Slice枠・id・座標焼き込み)/ parts(透過背景の枠レイヤー)の各PNG、
Slice別・Output別マスクPNG、`crops.json` / `crops.csv`(TD Table DAT対応のフラット表)。

`exportAll` は各Surfaceの現在のパターン種で書き出す。全パターン種の配布セットを作るには
`tp.exportAllPatterns("export")`: パターン種ごとに `export/GRID/`, `export/COLOR_BARS/`, …
へ同じ一式を書き出し、終了後に元のパターン種へ戻す。

### キャプチャ

`tp.capture("capture")`(またはキー `c`)で、テストパターンではなく**実際に出ている絵**を
タイムスタンプ付きPNGで保存: 全Surfaceのコンテンツ(ネイティブ解像度、`_src_<stamp>`)+
全Outputの合成結果(現在モードの見たまま、`_out_<stamp>`)。
実出力フレームをクライアントや映像制作チームに渡す用途。

### 自動パッキング

`tp.autoPack()` は slices 未定義の Surface を出力の空き領域へ自動配置する
(最大矩形を無傷で置き、余りをギロチン分割。`moduleSize` 指定時はカット寸法・配置原点を
モジュール倍数にスナップし、収まらない場合は歪めずエラー+レポート)。
結果は `tp.save()` で layout.json に書き戻せる。
検証(参照整合・はみ出し・重複・被覆・モジュール整合)は load / autoPack のたびに自動実行。

### サンプル

`example/` が現実的な構成(4面・5出力、Front右端320pxの3分割、
Floorの2キャンバス分割)のサンプル。`make -j8 Debug && make RunDebug` で起動。環境変数(CI・確認用、
30フレーム後に書き出して終了): `OFXTP_SNAPSHOT=1` capture実行 /
`OFXTP_EXPORT=1` exportAll実行 / `OFXTP_LAYOUT=<file>` レイアウト差し替え
(`layout-autopack.json` = slices無しの自動パッキングデモ) /
`OFXTP_SAVE=1` パッキング結果を layout-packed.json に保存。

### 実装状況 / Status

- [x] Phase 1: layout.json load/save / FBO自動生成 / GRID他パターン / モード切替 / Slice合成 / プレビュー
- [x] Phase 2: PNG一括書き出し(exportAll: src / mapped / overlay / parts)
- [x] Phase 3: crops.json/csv / mask書き出し / 完全検証(被覆・重複・はみ出し・moduleSize)
- [x] Phase 4: 自動パッキング(moduleSize量子化、空き領域追跡、充填率レポート)
- [ ] Phase 5: TD / Resolume / AE向けExporter、回転Slice、SLICE_IDランタイムパターン
