# SpektraRoll macOS（SwiftUI）

D073 的 macOS 壳。它不是第二套引擎：所有颜色、胶片、渲染都在
`spektra-roll-cli` 里，这里只负责三栏界面、播放和调用 CLI。

## 前置

- macOS 14+（开发机 M1 / macOS 14.7.5 已验证环境）
- Xcode 16.2 + command line tools
- `cmake`（`brew install cmake`）
- `ffmpeg`（`brew install ffmpeg`；构建脚本会把它打进 app）
- 字体（可选但推荐）：`scripts/fetch-macos-fonts.sh`

## 构建

```bash
# 一次完成：编 CLI -> 编 SwiftUI -> 组装 .app -> ad-hoc 签名 -> 打开
scripts/build-macos-ui.sh

# 或者双击
scripts/build-macos-ui.command

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
- 胶片效果单帧预览（暂停时用 CLI 渲染当前帧）。
- 片库按彩色负片 / 电影负片 / 反转片分类；相纸 / 电影印片分类。
- 画幅（35mm / Super 35 / 16mm / Super 16 / 65mm / IMAX）→ `--frame-width-mm`。
- LUT / 光谱 / 光谱 GPU 引擎；LUT 自动烘焙并缓存。
- 渲染 H.264 / HEVC / ProRes，SDR/PQ/HLG，进度 JSON、日志、取消。
- 设置和素材库持久化到 `~/Library/Application Support/SpektraRoll/state.json`。

## 已知限制（v1）

- 没有时间线拖动/逐帧预览；`胶片效果` 是暂停时单帧渲染。
- R3D 在 macOS 明确不支持（没有 RED 插件）；导入时提示。
- 防光晕层是 UI 级近似（开启后光晕 ×0.6），等引擎加独立参数。
- 没有裁切、印片机 Y/M/预闪、扫描仪 unsharp、空间物理模型。
- 未公证；首次打开需要右键 → 打开，或 `xattr -d com.apple.quarantine`。
- OCIO 在本地用 Homebrew 动态库；正式分发前要静态链接或捆绑 dylib。

## 设计来源

- `docs/ui-design-macos.md`：布局、交互、v1 范围。
- `design/tokens.json`：颜色/字体/间距/组件。
- `data/ui-schema.json`：参数和 CLI 映射。
- `docs/engine-api.md`：CLI 契约。

不复制 SpektraLab 的源码、图标或截图；只参考三栏工作流。
