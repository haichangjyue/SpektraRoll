# RED R3D（含 R3D NE）· 自带 SDK 的本地构建（D068）

> **一句话**：SpektraRoll 不随仓库或安装包分发 RED SDK，也不提供带 RED 支持的官方二进制。你自己从 RED 获取 SDK、接受 EULA，然后本地编译一个只给自己用的版本。
>
> 当前是 **D068 骨架**：能报告 SDK 版本、能读 `.R3D` 的元数据（尺寸/帧数/帧率/音频/timecode），**还不能渲染 R3D 帧**。渲染先用 REDCINE-X 导出 ProRes 4444。

> 代码按 R3D SDK 7.x 的公开 API 写（`InitializeSdk` / `Clip::LoadFrom` / 元数据访问）。更新版本的 SDK 如果改了签名，可能需要在 `src/core/src/red_sdk.cpp` 里做小调整；官方构建不包含这段代码，所以不会影响普通用户。

---

## 1. 为什么是“自带 SDK”

RED R3D SDK 是闭源、需要同意 RED EULA 的 SDK：

- 不能把它的头文件、静态库、文档、示例源码放进仓库。
- 动态库只能按 RED 的条款随“你的程序”分发；和本项目的 GPL-3.0-or-later 发布方式混在一起风险很高。
- R3D NE（Nikon ZR 的 REDCODE RAW 变体）需要较新的 SDK；旧 SDK 会拒绝这类文件。

所以官方构建（CI、安装包、`build\`）**永远编译 stub**：

```text
red-info -> "this build was compiled without the RED R3D SDK"
```

只有你自己运行 `scripts\build-red.ps1` 才会得到带 SDK 的本地版本。这个版本**只限自用**；如果你要再分发，需要自己同时满足 GPL 和 RED EULA。

---

## 2. 你需要准备什么

1. 从 RED 官方获取 **R3D SDK**，并接受它的 EULA。
2. 一个 SDK 目录，里面至少有：
   - `include/R3DSDK.h`（有的包是 `include/red/R3DSDK.h`）
   - Windows：`R3DSDK.lib` + `R3DSDK.dll`
   - macOS：`libR3DSDK.dylib`
3. Windows 上用和项目一致的 MinGW 构建，或自己用 MSVC 也能编（CMake 会自动选编译器）。

> SDK 版本要支持 R3D NE。拿不准时，用 REDCINE-X Pro 打开你的 `.R3D` 确认它能读；能读的同一套 SDK 再拿来编译。

---

## 3. 检查 SDK

```powershell
.\scripts\build-red.cmd -Check -SdkRoot D:\RED_SDK
```

或者：

```powershell
$env:SR_REDSDK_ROOT = "D:\RED_SDK"
.\scripts\build-red.cmd -Check
```

它会打印找到的 header / library / runtime 路径。三项都找到才继续。

---

## 4. 本地编译

```powershell
$env:SR_REDSDK_ROOT = "D:\RED_SDK"
.\scripts\build-red.cmd
```

产物：

```text
build-red\spektra-roll-cli.exe
```

这个构建和普通 `build\` 相互独立；官方 `build\` 不受影响。

### 运行时动态库

SDK 的 `InitializeSdk` 需要一个**包含动态库的目录**。默认使用 CMake 找到的 SDK 运行时目录；如果运行时报“SDK initialization failed”，显式指定：

```powershell
$env:SPEKTRA_RED_SDK = "D:\RED_SDK\bin"
.\build-red\spektra-roll-cli.exe red-info
```

macOS 同理，指向 `libR3DSDK.dylib` 所在目录。

---

## 5. 自检

```powershell
.\build-red\spektra-roll-cli.exe red-info
.\build-red\spektra-roll-cli.exe red-info --json
.\build-red\spektra-roll-cli.exe red-info --in "D:\footage\A001_C001.R3D" --json
```

没有 SDK 的普通构建会显示：

```text
RED R3D SDK:   not built
note:          this build was compiled without the RED R3D SDK; ...
```

带 SDK 的本地构建会显示 SDK 版本，`--in` 还会打印：

- 分辨率、帧数、帧率
- 音频通道数
- minimum color version
- timecode

---

## 6. 当前能做什么、不能做什么

| 能力 | D068 骨架 |
|---|---|
| 识别 `.R3D` 扩展名 | 所有构建都支持 |
| 读 R3D 元数据 | 仅带 SDK 的本地构建 |
| 直接渲染 `.R3D` 帧 | **还没有**（下一阶段） |
| 预览 `.R3D` | **还没有**；UI 会提示先用 REDCINE-X |
| 官方包带 RED | 不会 |

### 现在渲染 R3D 的推荐路线

1. 用 **REDCINE-X Pro** 导出：
   - ProRes 4444 / 4444 XQ
   - 色彩空间 **REDWideGamutRGB**
   - Gamma **Log3G10**
   - 保留音频和 timecode
2. 把解码烘进 LUT：

```powershell
.\build\spektra-roll-cli.exe lut bake `
  --ocio-config data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio `
  --ocio-input "Log3G10 REDWideGamutRGB" `
  --scan negative --exposure 0.7 --out out\red-log3g10.cube
```

3. 渲染导出的 ProRes：

```powershell
.\build\spektra-roll-cli.exe video --lut out\red-log3g10.cube `
  --in clip-raw.mov --out out\graded.mov --codec prores
```

内置 ACES 配置已经包含 `Log3G10 REDWideGamutRGB`，所以不需要额外装 OCIO 配置。

---

## 7. 常见问题

**CMake 报 `SR_WITH_REDSDK=ON requires -DSR_REDSDK_ROOT`**
：没设路径。用 `-SdkRoot` 或 `SR_REDSDK_ROOT`。

**CMake 报 `R3DSDK.h or the R3DSDK library was not found`**
：SDK 目录结构不同。先用 `-Check` 看三项路径；必要时手动指定 `-DSR_REDSDK_ROOT` 到真正的前缀目录。

**`red-info` 显示 built but not initialized**
：动态库没找到或版本不匹配。设置 `SPEKTRA_RED_SDK` 指向动态库目录；确认 SDK 版本支持你的 R3D NE 文件。

**`red-info --in` 报 `not an R3D file` / `unsupported clip format`**
：SDK 太旧，或文件不是 R3D NE。用 REDCINE-X 确认；必要时升级 SDK。

**官方 `build\` 里没有 RED**
：这是设计。只有 `build-red\` 是你自己编的。

---

## 8. 分发提醒

- `build-red\` 只适合自己用。
- 不要把 SDK 的头文件、静态库、动态库复制进仓库。
- 不要把带 RED 的二进制当作 SpektraRoll 官方包发布。
- 如果确实要发布带 RED 的版本，先让律师看 RED EULA 和 GPLv3 的组合，或单独做一个非 GPL 的发行版。
