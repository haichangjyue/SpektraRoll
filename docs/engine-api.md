# SpektraRoll · 引擎 API（D072 Phase 0）

> 这份文档是 **Qt UI 和 macOS SwiftUI UI 共同遵守的契约**。UI 只做壳：所有颜色、胶片、渲染逻辑都在 `spektra-roll-cli` 里。任何 UI 都不允许自己实现 LUT、胶片模型或预览解码。

- 版本：`0.1.0`（跟随 `spektra-roll-cli version`）
- 协议版本：**preview protocol 2**
- 状态：Phase 0 已完成；后续只做**加法**，不破坏已有字段
- 相关文件：`data/ui-schema.json`（参数唯一来源）、`design/tokens.json`（视觉唯一来源）、`docs/ui-parity.md`（功能对照）

---

## 1. 进程模型

UI 有两种调用方式：

| 方式 | 用途 | 生命周期 |
|---|---|---|
| **一次性命令** | `capabilities` / `catalog` / `ocio-info` / `red-info` / `lut bake` / `video` / `convert` | 启动 → 退出 |
| **常驻 `preview` 进程** | 打开素材、拖动时间线、预览帧 | UI 打开期间常驻；退出时发 `quit` |

UI 不链接 `sr_core`，也不直接调用 Metal/D3D11。SwiftUI 用 `Foundation.Process`；Qt 用 `QProcess`。

### 1.1 工作目录

- 默认从**仓库根目录**运行；`data/profiles`、`data/ocio` 是相对路径。
- macOS `.app` 里把 `data/` 放进 `Contents/Resources`，用绝对路径传 `--film` / `--paper` / `--ocio-config`，或把 CLI 的 `cwd` 设为 `Contents/Resources`。
- `catalog --json` 返回的 `path` 是**绝对路径**，UI 可以直接使用。

### 1.2 输出约定

- **stdout**：人类可读文本 + 可选的 JSON 事件行。
- **stderr**：warning / error / 调试信息。
- **退出码**：`0` 成功；非 `0` 失败。
- 机器解析规则：**只解析以 `{` 开头的行**；其余行原样进日志面板。
- JSON 行是单行、UTF-8、以 `\n` 结尾。并行分段时可能交错，但每行都是完整 JSON（CLI 内部有互斥锁）。

---

## 2. 能力握手：`capabilities --json`

UI 启动时调用一次，决定哪些控件可用、哪些要禁用。

```powershell
.\build\spektra-roll-cli.exe capabilities --json
```

输出（示例，字段只增不减）：

```json
{
  "version": "0.1.0",
  "platform": "windows",
  "arch": "x86_64",
  "build": {"ocio": true, "redLoader": true, "d3d11": true, "metal": false, "exr": true, "exrZip": true},
  "ocio": {"available": true, "version": "2.4.2"},
  "red": {"built": false, "available": false, "sdkVersion": "", "message": "RED plugin not found; ..."},
  "gpu": {"backend": "d3d11", "spectralAvailable": true, "textureAvailable": true, "error": ""},
  "engines": ["lut", "spectral", "spectral-gpu"],
  "textureEngines": ["auto", "cpu", "gpu"],
  "inputs": ["srgb", "apple-log", "s-log3", "s-log3-sgamut3", "c-log3", "v-log", "logc3", "logc4", "bmd-film", "davinci"],
  "scans": ["print", "negative", "slide"],
  "codecs": ["h264", "hevc", "prores"],
  "progress": ["text", "json"],
  "preview": {"protocol": 2, "frameTransport": "file", "commands": ["hello", "look", "open", "backend", "texture", "frame", "raw", "quit"]},
  "commands": ["version", "capabilities", "catalog", "gen", "convert", "video", "preview", "ocio-info", "red-info", "red-decode", "gpu-info", "lut info", "lut gen", "lut bake", "lut apply", "lut compare"],
  "dataRoots": ["data/profiles", "data/ocio"]
}
```

UI 规则：

| 字段 | UI 行为 |
|---|---|
| `ocio.available == false` | 禁用 OCIO 分组，显示“这个构建没有 OpenColorIO” |
| `red.built == false` | 禁用 R3D 预览/渲染，显示 `red.message` |
| `gpu.spectralAvailable == false` | 引擎下拉里保留“光谱 GPU”，但标注“无设备，将回退 CPU” |
| `gpu.textureAvailable == false` | “GPU 质感”不可选 |
| `preview.protocol` | 小于 2 时不要用 `hello` |
| `progress` 包含 `json` | 渲染时传 `--progress json` |

> `capabilities` 会探测 GPU 设备并尝试加载 RED 插件，约几十毫秒；不要每帧调用。

---

## 3. 片库：`catalog --json`

UI 不要自己扫描 `data/profiles/*.json`，也不要在 Swift 里复制 `support/stage/type/illuminant` 规则。

```powershell
.\build\spektra-roll-cli.exe catalog --json
```

输出：

```json
{
  "version": "0.1.0",
  "root": "data/profiles",
  "counts": {"films": 20, "papers": 8},
  "films": [
    {
      "id": "kodak_portra_400",
      "name": "Kodak Portra 400",
      "path": "X:\\agent\\SpektraRoll\\data\\profiles\\kodak_portra_400.json",
      "type": "negative",
      "support": "film",
      "stage": "filming",
      "referenceIlluminant": "D55",
      "viewingIlluminant": "D50",
      "targetPrint": "kodak_portra_endura",
      "use": "still",
      "antihalation": "strong",
      "positive": false,
      "printFilm": false
    }
  ],
  "papers": [
    {
      "id": "kodak_portra_endura",
      "name": "Kodak Portra Endura",
      "path": "X:\\agent\\SpektraRoll\\data\\profiles\\kodak_portra_endura.json",
      "type": "negative",
      "support": "paper",
      "stage": "printing",
      "referenceIlluminant": "D50",
      "viewingIlluminant": "D50",
      "targetPrint": "",
      "use": "still",
      "antihalation": "",
      "positive": false,
      "printFilm": false
    }
  ]
}
```

规则（与 Qt UI 现有逻辑一致）：

- `films`：`support=film`、`stage=filming`、`type=negative|positive`、参考光 `D55|T`，且 `targetPrint` 在 `papers` 里存在（反转片没有 target，直接保留）。
- `papers`：`support=paper`，或 `support=film && stage=printing`（2383/2393 电影印片，`printFilm=true`）。
- 选负片时，UI 用 `targetPrint` 自动切相纸/印片；选反转片（`positive=true`）时切 `--scan slide` 并停用相纸。
- `catalog` 约 200 ms（29 个 JSON），只在启动/刷新时调用。

人类可读版本：`catalog`（不带 `--json`）。

---

## 4. 渲染进度：`video --progress json`

默认仍是原来的文本进度，不影响现有 Qt UI。加 `--progress json` 后，stdout 会多出这些**单行 JSON 事件**（其他人类文本仍可能存在，客户端过滤 `{`）：

```json
{"event":"start","version":"0.1.0","width":3840,"height":2160,"fps":50.000000,"totalFrames":48,"segmented":false,"engine":"lut","codec":"prores","output":"out\\clip.mov"}
{"event":"progress","frame":24,"total":48,"percent":50.00}
{"event":"progress","frame":48,"total":48,"percent":100.00}
{"event":"done","frames":48,"parts":1,"elapsedMs":13088,"output":"out\\clip.mov"}
```

| 字段 | 说明 |
|---|---|
| `start.totalFrames` | 0 表示“到片尾未知” |
| `progress.frame` | 当前已处理到的**全片帧号**（分段时也是全局帧号） |
| `progress.total` | 总帧数；0 时 `percent = -1` |
| `progress.percent` | 0–100；用于进度条 |
| `done.frames` | 实际渲染帧数；`--resume` 全部命中时为 0 |
| `done.parts` | 分段数；非分段为 1 |
| `done.elapsedMs` | 总耗时（毫秒） |

并行分段（`--jobs > 1`）时 `progress` 事件可能不按帧号顺序到达；UI 用 `frame/total` 或 `done` 更新进度即可，不要假设单调。

失败时：进程以非 0 退出，错误在 stderr（`error: ...`），stdout 可能没有 `done`。UI 应把退出码和 stderr 一起展示。

### 4.1 播放代理：`video --display --max-side N`

D074 #3 的播放时代理也用同一个 `video` 命令，不引入第二套颜色逻辑：

```powershell
# 原片代理：输入色彩空间 -> 显示 sRGB，不上胶片
spektra-roll-cli video --display --input s-log3 --in clip.mp4 --out display.mov `
  --in-matrix bt709 --max-side 960 --codec h264 --trc srgb --crf 20 --preset veryfast `
  --progress json

# 胶片代理：当前 LUT / 光谱参数，低分辨率
spektra-roll-cli video --lut current.cube --in clip.mp4 --out film.mov `
  --in-matrix bt709 --max-side 960 --codec h264 --trc srgb --crf 20 --preset veryfast `
  --progress json
```

- `--max-side N` 在解码后、调色前按长边等比缩小（偶数尺寸），只缩小不放大。
- `--display` 只做输入 decode → linear Rec.709 → sRGB，不接受 `--lut/--engine/--film/--paper/--scan`；OCIO 输入用 `--ocio-config/--ocio-input`。
- `--display` 的 `start` 事件 `engine` 字段是 `"display"`，其余事件与 4 相同。
- 两个代理都整段渲染，保留源音频和 timecode；UI 按参数 hash 缓存，参数变化时使胶片代理失效。
- R3D 暂不支持 `--max-side`（macOS 也不用 R3D）。

---

## 5. 常驻预览：`preview` protocol 2

启动：

```powershell
.\build\spektra-roll-cli.exe preview --ffmpeg <ffmpeg> [--in-matrix auto] [--cache-mb 1024] [--max-side 1600]
```

UI 通过 stdin 写命令、stdout 读响应；`frame` / `raw` 把 RGBA 写到 UI 指定的临时文件。

### 5.1 命令

| 命令 | 响应 | 说明 |
|---|---|---|
| `hello` | `{"event":"hello",...}` | 协议握手；UI 启动后先发 |
| `look <bake args...>` | `ok look lut\|spectral\|spectral-gpu` | 设置 LUT / 光谱 / OCIO / 质感参数 |
| `open "<clip>"` | `ok open <w> <h> <cached> <total> <fps> <ms>` | 打开素材；长片会生成代理 |
| `texture <11 numbers>` | `ok texture` | 直接设置质感参数（见 Qt 的 `texture` 命令） |
| `backend auto\|cpu\|gpu` | `ok backend <mode>` | 切换 GPU 质感 |
| `frame <index> "<out.rgba>"` | `ok frame <index> <w> <h>` | 写**已调色**的 8-bit RGBA |
| `raw <index> "<out.rgba>"` | `ok raw <index> <w> <h>` | 写**未调色**的 8-bit RGBA（按住看原片） |
| `quit` | `ok bye` | 退出 |

错误响应：`error <message>`（单行）。`hello` 的响应：

```json
{"event":"hello","protocol":2,"version":"0.1.0","frameTransport":"file","histogram":false,"commands":["hello","look","open","backend","texture","frame","raw","quit"]}
```

### 5.2 帧数据

- 文件格式：**紧密排列的 8-bit RGBA**，长度 = `w * h * 4`，无头、无 padding。
- 字节序：R、G、B、A。
- 颜色：显示编码 sRGB（预览用）。
- UI 读取后包成 `CGImage`（SwiftUI）或 `QImage::Format_RGBA8888`（Qt）。
- 临时文件放在 UI 的缓存目录；每帧覆盖同一个文件即可。

### 5.3 预览流程（SwiftUI 参考）

1. 启动 `preview` 进程，发 `hello`，检查 `protocol >= 2`。
2. 发 `look`（烘焙参数/引擎/OCIO/质感），发 `open`。
3. 用户拖动时间线 → 发 `frame N <tmp>` → 读文件 → 显示。
4. 按住“看原片” → 发 `raw N <tmp>`。
5. 关闭窗口 → 发 `quit`，等待进程退出；超时则 terminate。

> `preview` 是内部命令；不要把它当稳定 CLI 给最终用户。协议变更走 `protocol` 版本号。

---

## 6. 其它机器可读命令

### 6.1 `ocio-info --json`

```powershell
.\build\spektra-roll-cli.exe ocio-info --json --ocio-config data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio
```

返回 `version / config / configFile / linearSpace / colorSpaces[] / displays[]`。UI 用 `colorSpaces` 填下拉。没有 OCIO 的构建返回非 0，并在 stderr 说明。

### 6.2 `solve --filter-pack --json`

```powershell
.\build\spektra-roll-cli.exe solve --filter-pack --json --film data\profiles\kodak_portra_400.json --paper data\profiles\kodak_portra_endura.json
```

返回 `film / paper / stock / filters{c,m,y}`，即 `neutral_print_filters.json` 里该胶片/相纸的中性放大机滤色片。UI 用它显示中性值；渲染时用 `--filter-offset C,M,Y` 在中性值上加减。

### 6.3 `red-info --json`

```powershell
.\build\spektra-roll-cli.exe red-info --in clip.R3D --json
```

返回 `built / available / sdkVersion / libraryPath / message`，有 `--in` 时还有 `clip`（尺寸、帧数、fps、音频通道、timecode）。UI 用 `built/available` 决定是否允许 `.r3d`。

### 6.4 `gpu-info`

人类可读的 GPU 诊断；`capabilities --json` 已包含同等信息，UI 不需要单独调用。

---

## 7. 一次性命令的参数稳定性

UI 传参时以 `data/ui-schema.json` 为准。几个容易错的点：

| 参数 | 规则 |
|---|---|
| `--couplers` | 必须带值：`on` / `off`；不是 presence flag |
| `--gamut` | `oklch` / `off` |
| `--scan` | `print` / `negative` / `slide`（也接受 `positive` / `reversal`，但 UI 用 `slide`） |
| `--in-matrix` | `auto`（默认，读素材标签）/ `bt2020` / `bt709`；无标签的 FX30 要显式 `bt709` |
| `--progress` | `text`（默认）/ `json` |
| `--resume` | presence flag；单独使用默认 10 s 分段 |
| `--jobs` | `0` = 自动；`1` = 顺序 |
| `--red-scale` | `full` / `half` / `quarter`；仅 R3D |
| `--ocio-*` | 对 `--engine spectral` 和 `--engine spectral-gpu` 都有效（D081：GPU 后端在 CPU 上批量解码）；LUT 路径要先 `lut bake --ocio-input` |
| `--exr-linear` + `--ocio-display/--ocio-view` | 跳过显示变换，直接写显示前线性 Rec.709（D081）；`--exr-trc` 不参与 |
| `--filter-offset` | `C,M,Y` 三个数，叠加在数据库中性值上；与 `--cc` 互斥（D082） |
| `--start` / `--frames` | 区间起点（秒）和帧数；UI 用显式 `rangeEnabled` 决定是否发送 `--frames`（D082） |

---

## 8. 版本与兼容

- `capabilities.version` 是 CLI 版本；`preview.protocol` 是独立协议版本。
- 新增字段/命令是兼容的；删除或改语义必须提升 `preview.protocol` 或大版本。
- UI 启动时做一次握手；不匹配时显示“CLI 版本过旧，请重新构建”，不要静默降级。
- 建议的构建门槛：`preview.protocol >= 2`、`commands` 包含 `catalog` 和 `capabilities`。

---

## 9. 验证命令（Phase 0 已跑）

```powershell
# 能力握手
.\build\spektra-roll-cli.exe capabilities --json

# 片库（应为 20 films / 8 papers）
.\build\spektra-roll-cli.exe catalog --json

# 预览握手
'hello' + "`n" + 'quit' | .\build\spektra-roll-cli.exe preview --ffmpeg J:\ffmpeg\bin\ffmpeg.exe

# 渲染 JSON 进度
.\build\spektra-roll-cli.exe video --lut out\cine-portra400.cube `
  --in X:\agent\footage\C0113.MP4 --out out\phase0-progress.mov `
  --frames 48 --codec prores --in-matrix bt709 --progress json
```

结果：`capabilities` / `catalog` JSON 可被 `ConvertFrom-Json` 解析；`catalog` 为 20/8；`hello` 返回 protocol 2；渲染输出 `start/progress/done`；`ctest` 11/11。
