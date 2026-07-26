# ofxTestPattern 設計ドキュメント

テストパターンの生成・配置・出力を自動化する openFrameworks アドオン。
手作業のPixelmap制作ワークフローを置き換え、
「設計(Claude Code等) → 表示確認(oF) → 最終出力(PNG/データ)」を1つのデータで回す。

## 0. 決定事項

| 論点 | 決定 |
| --- | --- |
| データの正 | `layout.json` を単一の真実とする。手書き/AI生成/アドオン内自動パッキングのいずれでも同じスキーマ |
| パターン生成 | GPUリアルタイム描画(動体パターン可)+ 同一コードからPNG一括書き出し |
| 操作系 | APIのみ + opt-in の最小キー操作。GUIアドオン非依存 |
| モジュール制約 | 分割・配置位置の量子化のみ(`moduleSize`、例: 128px)。モジュールグリッド管理は将来拡張 |
| グリッド | **正方グリッド・px指定・中央基準**。端は端数マスとして許容。full squares数は幾何から算出して表示 |
| 描画原則 | **ピクセルパーフェクト**。1pxラインは矩形で整数座標に描画、Slice合成は等倍texel転写(スケーリング・フィルタリングなし)、暗黙のリサイズ禁止 |
| エクスポート | 汎用JSON/CSV + PNG一式(src/mapped/overlay/parts/**mask**)。TD/Resolume/AE向けは同一モデル上のプラガブルなExporterとして追加 |

## 1. コンセプトモデル

```
Surface  … 論理面。名前 + ネイティブ解像度 (例: Front 4160x1620)
           FBOを自動アロケート。ホストのコンテンツ or テストパターンを保持
Output   … 物理出力キャンバス (例: 3840x2160)。Sliceを合成したFBOを保持
Slice    … Surfaceの矩形クロップをOutputへ配置する1件のマッピング
           { surface, srcRect, output, dstPos, rot(将来), id }
Packer   … 制約付き自動パッキング。mapping未定義のSurfaceを自動配置
Exporter … layout.json / crops.json / crops.csv / PNG一式の書き出し
```

- Surface : Output は多対多。1 Surface が複数 Output に跨る分割(Floor → Floor-1/2)も、
  1 Output に複数 Surface を詰めるのも Slice の列で表現できる。
- Slice の `id` は人間可読(`front-A` 等)。overlay 焼き込み・export・議論の共通キーにする。

## 2. layout.json スキーマ

```jsonc
{
  "name": "demo",
  "constraints": {
    "moduleSize": 128        // 分割線・配置位置をこの倍数にスナップ。0で無効
  },
  "style": {
    "gridColor": "auto",     // Surfaceごとに上書き可
    "font": ""               // 空ならoF標準
  },
  "surfaces": [
    {
      "name": "Front",
      "width": 4160, "height": 1620,
      "gridSize": 200,       // px。正方・中央基準。0/省略で自動(h/8に近いキリの良い値)
      "color": "#00FFFF"
    },
    { "name": "Left",  "width": 2560, "height": 1440, "color": "#00FFFF" },
    { "name": "Right", "width": 2560, "height": 1440, "color": "#00FFFF" },
    { "name": "Floor", "width": 3840, "height": 2400, "color": "#0064FF" }
  ],
  "outputs": [
    { "name": "Front-out", "width": 3840, "height": 2160 },
    { "name": "Floor-1",   "width": 3840, "height": 2160 },
    { "name": "Floor-2",   "width": 3840, "height": 2160 }
  ],
  "slices": [
    { "id": "front-A", "surface": "Front", "src": [0,    0, 3840, 1620],
      "output": "Front-out", "dst": [0, 0] },
    { "id": "front-B", "surface": "Front", "src": [3840, 0,  320,  540],
      "output": "Front-out", "dst": [0, 1620] },
    { "id": "front-C", "surface": "Front", "src": [3840, 540, 320, 540],
      "output": "Front-out", "dst": [320, 1620] },
    { "id": "front-D", "surface": "Front", "src": [3840, 1080, 320, 540],
      "output": "Front-out", "dst": [640, 1620] }
  ]
}
```

- `slices` が空、または一部Surfaceのslice未定義 → ロード時/明示呼び出しで Packer が生成。
- ロード時に必ず検証を実行(§5)。エラーはconsoleへ、`getValidationReport()` でも取得可。

## 3. ランタイムAPI

```cpp
ofxTestPattern tp;

// setup
tp.load("layout.json");            // FBO自動アロケート含む
tp.save("layout.json");            // autoPack結果の書き戻し等

// コンテンツ描画(通常運用時)
tp.begin("Front");                 //   Surface FBOへ
  // ... app content ...
tp.end();

// 表示モード(全体 / Surface個別)
tp.setMode(ofxTestPattern::CONTENT);   // パターン非表示、コンテンツのみ
tp.setMode(ofxTestPattern::PATTERN);   // テストパターンに置き換え
tp.setMode(ofxTestPattern::OVERLAY);   // コンテンツの上にパターンを重畳(α)
tp.setPatternType("Front", Pattern::GRID);   // "*" で全Surface

// 出力
tp.update();                       // Slice合成(mapped FBO更新)
tp.drawOutput("Front-out");        // 出力キャンバスを描画
tp.getOutputFbo("Floor-1");        // 多窓/Syphon/NDI等はホスト側で自由に
tp.drawSurface("Front", x,y,w);    // 単面プレビュー
tp.drawPreview(ofRectangle);       // 全Surface+Outputを整列サムネイル表示
                                   // (自動レイアウト、ラベル付き)

// キー操作(opt-in)
tp.enableKeyShortcuts();           // t: モード巡回 / p: パターン種巡回
                                   // e: 一括エクスポート / TAB: プレビュー切替

// エクスポート(§4)
tp.exportAll("export/");           // PNG一式 + crops.json + crops.csv
tp.capture("capture/");            // 実出力のキャプチャ: 全Surfaceコンテンツ(ネイティブ解像度)
                                   // + 全Output合成結果(現在のモードの見たまま)を
                                   // タイムスタンプ付きPNGで保存
tp.autoPack();                     // 未マップSurfaceを自動配置
```

## 4. テストパターン / エクスポート仕様

### パターン種(初期セット)
1. **GRID**(既定): 番号付きグリッド(横: 中央から±n、縦: A/B/C + a/b/c 象限)、
   センタークロス、サークル、外周1pxボーダー、ラベルブロック
   (名前 / WxH / アスペクト比 / full squares)、カラーバー — 現行Pixelmap互換
2. **COLOR_BARS** / **WHITE** / **R/G/B/黒** — 輝度・色確認
3. **CHECKER**(サイズ可変)/ **GRADIENT_H/V** — 階調・スケーリング確認
4. **MOVING**: 移動バー + フレームカウンタ — 遅延・テア・フレーム落ち確認
5. **SLICE_ID**: Slice領域の塗り分け + id/座標ラベル — マッピング確認用
   (現行 overlay/parts 相当をランタイム表示)

### PNG書き出し(`exportAll`)
```
export/
  {name}_{surface}_src.png            … Surfaceネイティブ解像度のパターン
  {name}_{output}_mapped.png          … Output解像度の合成結果
  {name}_overlay_{...}.png            … 上記 + Slice枠・id・座標の焼き込み
  {name}_parts_{...}.png              … Slice枠のみ(白地)
  masks/{name}_mask_{sliceId}.png     … Output解像度、該当Slice領域=白のマスク
  masks/{name}_mask_{output}.png      … Output内の全有効領域マスク
  crops.json / crops.csv
```

### キャプチャ(`capture`)
テストパターンではなく **実際に出ている絵** を渡す用途(施工先・映像制作との共有等)。
```
capture/
  {name}_{surface}_src_{yyyymmdd-HHMMSS}.png   … Surfaceコンテンツ(ネイティブ解像度)
  {name}_{output}_out_{yyyymmdd-HHMMSS}.png    … Output合成結果(現在モードの見たまま)
```
タイムスタンプ付きなので連続キャプチャしても上書きされない。キー `c` でも実行。

### crops.json / crops.csv(汎用)
```json
{ "slices": [ { "id": "front-A", "surface": "Front",
    "src": {"x":0,"y":0,"w":3840,"h":1620},
    "output": "Front-out", "dst": {"x":0,"y":0}, "rot": 0 } ] }
```
CSVは同内容のフラット表(TouchDesigner Table DAT でそのまま読める列構成)。
TD tox / Resolume Advanced Output / AE用JSXは、同じモデルを読む Exporter として後段で追加。

## 5. Packer / 検証

### 自動パッキング
- 方針: 現行の手動設計を再現する **「最大矩形を無傷で置き、余りをストリップ分割」** 戦略
  (guillotine系)。分割数最小化を優先。
- `moduleSize` 量子化: カット位置・配置位置とも倍数にスナップ。
  スナップにより収まらない場合はエラー(黙って歪めない)。
- 出力: slices生成 + レポート(Output毎の充填率、Surface毎の分割数)。
- 収まらない場合は必要Output数を提案(Floor→2枚のようなSurface分割も提案)。

### 検証(load時 / autoPack後 / save前に必ず実行)
- src矩形の全被覆(Surfaceの取り漏らしピクセルなし)・重複なし
- dst矩形のOutput内収まり・Output内での重複なし
- moduleSize整合(warning)
- Surface/Output名の参照整合

## 6. ファイル構成

```
ofxTestPattern/
├── addon_config.mk
├── DESIGN.md
├── src/
│   ├── ofxTestPattern.h / .cpp      … ファサード(上記API)
│   ├── ofxTpTypes.h                 … Surface / Output / Slice / Layout 構造体
│   ├── ofxTpPatternRenderer.h/.cpp  … パターン描画(FBOに対して)
│   ├── ofxTpPacker.h/.cpp           … 自動パッキング + 検証
│   └── ofxTpExporter.h/.cpp         … JSON/CSV/PNG書き出し
└── example/                         … サンプル構成(4面5出力、余りストリップ分割・
    └── bin/data/layout.json           Floor 2キャンバス分割・autoPackデモ入り)
```

依存: oFコアのみ(ofJson=nlohmann内蔵、ofTrueTypeFont、ofFbo)。

## 7. 実装フェーズ

1. **Phase 1**: Types + layout.json load/save + Surface FBO + GRIDパターン + drawSurface/drawOutput + モード切替
2. **Phase 2**: 全パターン種 + drawPreview + キー操作 + PNG一括書き出し
3. **Phase 3**: crops.json/csv + mask書き出し + 検証
4. **Phase 4**: Packer(自動パッキング + 提案レポート)
5. **Phase 5**: 個別Exporter(TD / Resolume / AE)、回転Slice対応
