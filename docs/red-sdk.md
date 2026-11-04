# RED R3D / R3D NE · 自带 SDK 的本地插件（D071）

> **一句话**：SpektraRoll 不随仓库或安装包分发 RED SDK。你自己从 RED 获取 SDK、接受 EULA，用 MSVC 本地编译 `spektra-red.dll`；GPL 的 CLI 在运行时加载它。官方 `build\` 永远没有 RED 代码。
>
> 当前能做到：**读元数据 + 解码 R3D / R3D NE 帧**（CPU）。`red-decode` 输出 16-bit 帧，默认 **REDWideGamutRGB + Log3G10**，可以直接接 `lut apply`；`video` / 界面预览的直连是下一阶段。

---

## 1. 为什么是插件

RED 的 Windows SDK 9.2 只提供 MSVC 静态库（`R3DSDK-2017MD.lib` 等），而 SpektraRoll 的主程序是 MinGW 构建，C++ ABI 不兼容。直接链接会失败，也会把闭源 SDK 混进 GPL 二进制。

所以分成两层：

- **核心 CLI（GPL，MinGW）**：只负责找插件、调用 C ABI，不包含任何 RED 头文件。
- **`spektra-red.dll`（本地构建，MSVC）**：唯一包含 `R3DSDK.h` 的部分，直接使用你提供的 SDK。

官方 CI 和 `build\` 只编译核心 loader；没有插件时 `red-info` 会明确说“RED plugin not found”。

---

## 2. 你需要准备什么

- RED **R3D SDK 9.2.1**（Windows x64）。其他版本只要 API 兼容也可以试；R3D NE 需要较新的 SDK。
- **Visual Studio Build Tools 2022**（C++ 工作负载）。SDK 文档说明 2019/2022 可以使用 2017 库。
- SDK 目录里至少有：
  - `Include\R3DSDK.h`
  - `Lib\win64\R3DSDK-2017MD.lib`
  - `Redistributable\win\REDR3D-x64.dll`（以及同目录其他 DLL）

> 本机验证用的 SDK 是 `X:\agent\footage\R3DSDKv9_2_1`，样片是 `ZR_RED_Color_Science_Sunflower.R3D`（Nikon ZR，R3D NE，6048×3402，249 帧，23.976，TC 11:09:14:03）。这些路径只在本机脚本里作为默认值，不进入仓库。

---

## 3. 检查与构建

```powershell
.\scripts\build-red.cmd -Check -SdkRoot D:\RED_SDK
```

它会打印 header / library / runtime / MSVC 四项。然后：

```powershell
$env:SR_REDSDK_ROOT = "D:\RED_SDK"
.\scripts\build-red.cmd
```

产物是一个独立目录：

```text
build-red\
  spektra-roll-cli.exe   (普通 MinGW CLI 的副本)
  spektra-red.dll        (MSVC 插件)
  REDR3D-x64.dll ...     (SDK 的 Redistributable DLL，本地复制)
  obj\                   (编译中间文件)
```

`build-red\` 在 `.gitignore` 的 `build-*/` 范围内，不会被提交。脚本不会复制 SDK 头文件或静态库。

自检：

```powershell
.\build-red\spektra-roll-cli.exe red-info --json
.\build-red\spektra-roll-cli.exe red-info --in "D:\footage\A001.R3D" --json
```

---

## 4. 解码帧

### 单帧

```powershell
.\build-red\spektra-roll-cli.exe red-decode `
  --in "D:\footage\ZR_RED_Color_Science_Sunflower.R3D" `
  --frame 0 --scale half --format srgb --out out\red-original.png
```

- `--format log3g10`（默认）：REDWideGamutRGB + Log3G10，LUT 的输入域
- `--format linear`：线性 Rec.709
- `--format srgb`：Rec.709 + sRGB，直接看
- `--scale full|half|quarter`：6K 全分辨率、半分辨率、四分之一

### 序列

```powershell
.\build-red\spektra-roll-cli.exe red-decode `
  --in "D:\footage\ZR_RED_Color_Science_Sunflower.R3D" `
  --out-dir out\red-seq --start 0 --frames 12 `
  --scale half --format log3g10 --out-ext png
```

文件名是 `<输入名>_0000.png`…。也支持 `--out-ext exr`（浮点，不钳制）。

### 接到胶片

```powershell
# 1. 把 Log3G10/RWG 解码烘进 LUT（只需一次）
.\build-red\spektra-roll-cli.exe lut bake `
  --ocio-config data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio `
  --ocio-input "Log3G10 REDWideGamutRGB" `
  --film data\profiles\kodak_vision3_250d.json `
  --paper data\profiles\kodak_2383.json `
  --scan print --exposure 0.7 --out out\red-vision3-2383.cube

# 2. 单帧
.\build-red\spektra-roll-cli.exe lut apply --lut out\red-vision3-2383.cube `
  --in out\red-log3g10.png --out out\red-graded.png

# 3. 序列
.\build-red\spektra-roll-cli.exe lut apply --lut out\red-vision3-2383.cube `
  --in-dir out\red-seq --out-dir out\red-seq-graded --out-ext png

# 4. 直接出视频（D071：音频会一起封装；支持 segment）
.\build-red\spektra-roll-cli.exe video --lut out\red-vision3-2383.cube `
  --in D:\footage\A001.R3D --out out\red-clip.mov `
  --frames 12 --red-scale half --codec prores `
  --segment 0.2 --resume --jobs 2

# 5. UI 自检（需要 build-ui）
.\build-ui\spektra-roll-ui.exe --red-selftest
.\build-ui\spektra-roll-ui.exe --red-render-selftest
```

双击演示：`scripts\demo-red.cmd`。它会解一帧 sRGB、一帧 Log3G10、烘 Vision3 250D + 2383、套 LUT，最后直接渲染 3 帧视频。

### 界面

选 `.r3d` → 左栏“R3D 解码”选 full/half/quarter → 预览/渲染。R3D 会自动切 ProRes；LUT 引擎会用 `Log3G10 REDWideGamutRGB` 烘表，光谱引擎会把 R3D 解成 sRGB。单条“加入队列”可用；批量 R3D 还没接。

---

## 5. 实测（本机，Nikon ZR R3D NE）

| 项 | 结果 |
|---|---|
| 元数据 | 6048×3402，249 帧，23.976 fps，TC 11:09:14:03，音频 0 通道 |
| half 解码 | 3024×1701，16-bit PNG；约 **3.8 s/帧**（含 SDK 初始化与约 30 MB PNG 写盘） |
| full 解码 | 6048×3402，16-bit PNG；约 **14 s/帧**（含约 100 MB PNG 写盘） |
| 解码后端 | **CPU**（`OPTION_RED_NONE`）；GPU 解码（CUDA/OpenCL/Metal）还没接 |
| 格式 | Log3G10/RWG、Linear Rec.709、sRGB Rec.709 |
| 序列 | `red-decode --out-dir` + `lut apply --in-dir` 正常 |
| 直读视频（D070） | `video --in clip.R3D`：half LUT **2.0 s/帧**；half 光谱 GPU **2.29 s/帧**；quarter **0.69 s/帧** |
| 视频 timecode | `--start 1` 时 `11:09:14:03 → 11:09:15:03` |
| segment / jobs / resume（D071） | 4 帧 / 2 段 / 2 workers 正常；删第 2 段后只补缺失段 |
| UI 直连（D071） | `--red-selftest` 3024×1701 **7993 ms**；`--red-render-selftest` 2 帧 ProRes |
| 音频（D071） | 插件写 AU；ffmpeg 封装 PCM（ProRes）/ AAC（H.264/HEVC）。样片 **0 音频通道**，真实 R3D 音频未测；合成 AU 封装已验证 |

6K 全分辨率很吃 CPU；预览/批量先用 `--red-scale half`，成片再用 `full`。

---

## 6. 还没做

- **真实 R3D 音频样片验证**（代码已接通，样片 0 通道）
- **UI 批量 R3D 入队**（单条预览/渲染已可用）
- GPU 解码（CUDA / OpenCL / Metal）、并行解码
- 6K 全分辨率的高性能路径

这些是下一阶段的候选。

---

## 7. 常见问题

**`red-info` 显示 `built: false`**
：插件没找到。确认 `build-red\spektra-red.dll` 和 `spektra-roll-cli.exe` 在同一目录，或设 `SPEKTRA_RED_PLUGIN` 指向 DLL 的完整路径。

**`RED SDK initialization failed`**
：SDK 的 Redistributable DLL 没在插件旁边。把它们复制到 `build-red\`，或设 `SPEKTRA_RED_SDK` 指向 DLL 目录。

**`R3D load failed: not an R3D file` / `unsupported clip format`**
：SDK 太旧，或文件不是 R3D NE。用 REDCINE-X 确认；R3D NE 需要支持它的 SDK。

**CMake / 主程序要不要加 `SR_WITH_REDSDK`**
：不要。D069 起主程序始终只编译 loader；插件由 `scripts\build-red.cmd` 单独构建。旧版 D068 的 `SR_WITH_REDSDK` 已移除。

**能把 `build-red\` 发给别人吗**
：不要。它包含 RED 的 DLL 和带 SDK 的插件，只限你自己使用。官方包永远不带这些。

---

## 8. 许可证

- RED SDK 是闭源、需要接受 RED EULA 的软件；头文件、静态库、示例源码不能放进仓库。
- `build-red\` 只适合本地自用；再分发要同时满足 GPL 和 RED EULA。
- 官方 `build\` / CI 不包含 RED 代码，也不会加载插件。
