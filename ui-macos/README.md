# SpektraRoll macOS（SwiftUI）

D073 的 macOS 壳。它不是第二套引擎：所有颜色、胶片、渲染都在
`spektra-roll-cli` 里，这里只负责三栏界面、播放和调用 CLI。

## 前置

- macOS 14+（开发机 M1 / macOS 14.7.5 已验证环境）
- Xcode 16.2 + command line tools
- `cmake`（`brew install cmake`）
- `ffmpeg`（`brew install ffmpeg`；构建脚本会把它打进 app）
- OpenColorIO（可选，推荐）：`scripts/build-ocio-macos.sh` 编一个静态 OCIO 2.6.0 到 `~/Documents/ocio-2.6.0-static`；`build-macos-ui.sh` 会自动检测。
- 字体（可选但推荐）：`scripts/fetch-macos-fonts.sh`

## 构建

```bash
# 可选：先编静态 OpenColorIO 2.6.0（约 3–5 分钟）
scripts/build-ocio-macos.sh

# 一次完成：编 CLI -> 编 SwiftUI -> 组装 .app -> ad-hoc 签名 -> 打开
scripts/build-macos-ui.sh

# 或者双击
scripts/build-macos-ui.command
# OCIO 也可双击 scripts/build-ocio-macos.command

# 只编 SwiftUI（CLI 用已有 build/）
scripts/build-macos-ui.sh --skip-cli

# 不自动打开
scripts/build-macos-ui.sh --no-open
```

产物：

```
build-ui-macos/SpektraRoll.app
  Contents/MacOS/SpektraRoll
  Contents/Helpers/spektra-roll-cli
  Contents/Helpers/ffmpeg, ffprobe
  Contents/Resources/data/profiles
  Contents/Resources/data/ocio
  Contents/Resources/Fonts
```

如果 CLI 没有打包，App 会按顺序找：`SPEKTRA_CLI` → app 内 Helpers →
`build-red/` → `build/` → PATH。

## 当前 v1 功能

- 三栏：左侧「底片」缩略图、中间播放器、右侧 INFO/FILM/PRINT/ADVANCED/OUTPUT。
- 双击左侧空白导入视频/项目目录；导入时选「画面类型」或「OCIO 色彩空间」。
- 双击缩略图载入播放器；空格播放/暂停。
- 胶片效果单帧预览（暂停时用 CLI 渲染当前帧）；参数变化 180 ms 防抖自动刷新。
- 关闭胶片效果时显示输入色彩空间解码到 sRGB 的静帧（不再直接显示 Log 画面）。
- 播放时使用 960 px 代理：关闭胶片效果播显示代理；开启胶片效果播胶片代理，按参数 hash 缓存并自动切换。
- 生成胶片代理时播放器暂停（显示当前胶片静帧/进度），完成后自动切到胶片代理并继续播放。
- ADVANCED「预览代理」：默认 5 GB 上限（0 = 不限制）、自定义缓存目录、当前占用、手动清理；超限删除最旧代理。
- 视频下方直方图 + 宽容度/亮度范围/高光暗部裁切参考。
- 画面缩放/平移：滚轮/双指缩放、拖拽、适合 / 100%、双击切换。
- 播放器“导出”按钮直接跳到 OUTPUT。
- INFO 底部显示 **Mac** 配置：系统、屏幕、CPU、GPU、内存、存储、HDR、屏幕色彩空间（Windows 版标题为 **PC**）。
- INFO 引擎区首行 **APP**：未发布版本显示 `内部测试（D079） · 0.1.0`。
- 播放器 `导出` 右侧有 **配光** 按钮：暂停、测当前帧中灰、把曝光调到 18% 线性中灰、打开胶片效果并刷新；中性 C/M/Y 由胶片/相纸数据库自动查表。
- 片库按彩色负片 / 电影负片 / 反转片分类；相纸 / 电影印片分类。
- 画幅（Super 8 / 16mm / Super 16 / 35mm / Super 35 / 65mm / IMAX，全部 CINE）→ `--frame-width-mm`；画幅行显示物理宽度。
- LUT / 光谱 / 光谱 GPU 引擎；LUT 自动烘焙并缓存。
- 渲染 H.264 / HEVC / ProRes，SDR/PQ/HLG，进度 JSON、日志、取消。
- 设置和素材库持久化到 `~/Library/Application Support/SpektraRoll/state.json`。

## 已验证（2026-11-05，MacBook Air M1）

- `.app` 启动、三栏、思源黑体/Noto Sans CJK SC、导入缩略图、AVPlayer 播放、空格、INFO、胶片效果、渲染。
- 渲染输出：640×360 / 3.00 s / H.264 / yuv420p。
- D076：`video --display/--max-side 960` CLI 已验证；Mac 本机 `swift build` 通过，显示代理/胶片代理已生成（`~/Library/Caches/SpektraRoll/preview-cache/`）。
- D077：INFO Mac 面板截图通过；生成胶片代理时播放日志 `block film proxy` → `switch film` → `unblock`；缓存上限用 3 个 2 MB 假文件验证，超限后自动删除。
- D079：配光在 `C0113.MP4` 上测得 **+0.66 EV**，ADVANCED 曝光滑块同步、胶片效果刷新；`SPEKTRA_AUTOGRADE=1` 可复现。
- D080：静态 OCIO 2.6.0 在 Mac 上构建成功；bundle 的 `capabilities` 报 `ocio.available=true`，`ocio-info` 列出 S-Log3 空间，`lut bake --ocio-input "S-Log3 S-Gamut3.Cine"` 成功；`otool -L` 确认没有 Homebrew OCIO/Imath/yaml/pystring 等 dylib 依赖。
- 远程验收见 `docs/macos-dev-notes.md`（SSH Background domain、自截图、Stage Manager）。

## 调试开关

仅环境变量生效，正常启动不受影响：

```bash
open -F --env SPEKTRA_ALL_SPACES=1 --env SPEKTRA_CAPTURE=1 \
  build-ui-macos/SpektraRoll.app
```

`SPEKTRA_AUTOLOAD` / `SPEKTRA_AUTOPREVIEW` / `SPEKTRA_AUTOPLAY` / `SPEKTRA_AUTOGRADE` / `SPEKTRA_PLAYER_DEBUG` / `SPEKTRA_TAB` / `SPEKTRA_PROXY_LIMIT_GB` / `SPEKTRA_AUTORENDER` / `SPEKTRA_SEEK` / `SPEKTRA_QUEUE_SELFTEST` / `SPEKTRA_PROXY_DIR` 见 `docs/macos-dev-notes.md`。

## 已知限制（v1）

- D082 起有时间线拖动/逐帧预览、渲染队列和区间输入；`胶片效果` 的实时播放用 360–720 px LUT 代理，生成期间暂停并显示进度。
- 代理仍是整段预渲染，但 6 秒 4K50 在 M1 上从启动到就绪约 **13 秒**（原 20–30 秒）；长片会自动降到 540/360 px。
- 切换缓存目录会迁移旧目录里的代理文件。
- R3D 在 macOS 明确不支持（没有 RED 插件）；导入时提示。
- 防光晕层是 UI 级近似（开启后光晕 ×0.6），等引擎加独立参数。
- 有 Y/M 滤色片偏移（D082）；仍没有裁切、预闪、扫描仪 unsharp、空间物理模型。
- 未公证；首次打开需要右键 → 打开，或 `xattr -d com.apple.quarantine`。
- **OCIO 已可用**（D080）：静态 OCIO 2.6.0 前缀，`build-macos-ui.sh` 自动检测；`ocio-info` / `lut bake --ocio-input` / 显示变换已验证。D081 起 `--engine spectral-gpu` + OCIO 已接通（CPU 批量解码后进 Metal 光谱着色器），`--exr-linear` 遇到 OCIO 显示变换时写显示前线性 Rec.709。

## 设计来源

- `docs/ui-design-macos.md`：布局、交互、v1 范围。
- `design/tokens.json`：颜色/字体/间距/组件。
- `data/ui-schema.json`：参数和 CLI 映射。
- `docs/engine-api.md`：CLI 契约。

不复制 SpektraLab 的源码、图标或截图；只参考三栏工作流。
