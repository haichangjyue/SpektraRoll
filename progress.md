# SpektraRoll · 进度交接书

> **这份文件的用途**：让你在新开一个对话时，把这一份丢给 AI，它就能接着干，不需要你重新解释背景。
>
> **最后更新**：2026-11-05（D081：光谱 GPU 接 OCIO + OCIO 线性 EXR；Windows + Mac 已验，CI run 153 双绿）
> **当前里程碑**：**M5 进行中 · macOS UI v1.1 完成（D075–D080），Windows Qt 冻结**（Qt 6 三栏界面 + 渲染队列 + 光谱/GPU 引擎 + GPU 质感 + 物理点扩散核 + 常驻预览 + 融合提交 + 管道/并行 + 预览代理 + float/half/linear/ZIP EXR + Metal 颜色与质感 + S-Log3/C-Log3/V-Log/LogC3/LogC4/BMD/DaVinci + 静态 OCIO + 界面 OCIO 下拉 + Vision3/2383/2393 + 反转片直扫 + BYO-SDK R3D 插件/解码 + `video --in clip.R3D` + R3D 音频/UI/segment + 光谱 GPU 的 OCIO（D041–D081）
> **项目**：SpektraRoll —— 面向**动态影像（视频）**的开源胶片与印片模拟器
> **仓库**：https://github.com/haichangjyue/SpektraRoll （公开，GPL-3.0-or-later）
> **本地目录**：`X:\agent\SpektraRoll`

---

## 本次对话交接摘要（D075–D081，2026-11-05 晚）

> **新开对话先读这一节**；下面是逐轮细节（D081 → D075，越往后越早）。这一轮从“Windows Qt 冻结，README 换 macOS 截图”开始，随后完成 macOS UI v1.1 P0/P1、D077 反馈、D078 画幅/APP 版本、D079 配光、D080 静态 OCIO、D081 光谱 GPU 的 OCIO + 线性 EXR。

### 一、已完成的事项

| 编号 | 交付 | 关键结果 |
|---|---|---|
| **README** | 去掉 Windows Qt Desktop UI 截图，换 macOS 最新截图 | `docs/ui-macos.png`；Desktop UI 改为 macOS 参考 + Windows Qt 冻结；`537ad1f` |
| **D075** | macOS UI v1.1 P0 | 关闭胶片效果显示 sRGB 静帧；直方图/宽容度/裁切；滚轮/双指缩放、拖拽、适合/100%、双击；导出按钮跳 OUTPUT；参数 180 ms 防抖刷新 |
| **D076** | 播放代理 P1 | CLI `video --display` + `--max-side`；960 px 显示/胶片代理；AVPlayer 保留时间切换；参数 hash 缓存；Mac 播放日志确认 `block film proxy` → `switch film` → `unblock` |
| **D077** | 验收反馈 4 点 | ADVANCED 预览代理：默认 5 GB 上限（0 = 不限制）、自定义目录、当前占用、清理、超限删最旧；生成胶片代理时暂停播放并记住播放请求；进度胶囊移到标题行左下；INFO 新增 Mac 系统面板 |
| **D078** | 电影画幅 + APP 版本 | 新增 Super 8；16mm 10.26 / Super 16 12.52 / Super 35 24.89 / 65mm 52.63 对齐 SpektraLab 相机负片长边；35mm 36.0、IMAX 70.0；全部标 CINE；INFO 引擎区加 APP 行 |
| **D079** | 配光按钮 | 放在 `导出` 右侧；暂停、抽取当前帧、解到 sRGB、按亮度中位数把 18% 线性中灰落位；`C0113.MP4` 实测 **+0.66 EV**；中性 C/M/Y 由胶片/相纸数据库自动查表 |
| **D080** | macOS 静态 OCIO 2.6.0 | 新增 `scripts/build-ocio-macos.sh` / `.command`；静态依赖装到 `~/Documents/ocio-2.6.0-static`；`build-macos-ui.sh` 自动检测；bundle `ocio.available=true, 2.6.0`；`otool -L` 无 Homebrew OCIO/Imath/yaml/pystring dylib；INFO 显示“可用 2.6.0” |
| **D081** | 光谱 GPU 接 OCIO + 线性 EXR | GPU 后端上传前用 OCIO CPU 处理器批量解码成线性 Rec.709；D3D11 / Metal 着色器新增 `inputMode=10`，并保留原始编码缓冲保证 `--strength` 语义；`convert --ocio-display ... --exr-linear` 改为直接写显示前线性 Rec.709；移除 `lut apply` / `video` / `preview` 的 CPU 回退；Windows + Mac 本机 `ctest` **11/11**，Metal OCIO worst **0**、CPU vs Metal **113.70 dB**；CI run **153** 双绿 |

**顺手修复**：片库按路径去重（调试自动导入/重复导入不再出现两个同名素材）；`AppVersion.testTag` 升到 **D081**。

### 二、关键决策

| 编号 | 决策 | 结论 |
|---|---|---|
| D075 | P0 范围 | 先做低风险 4 项 + 参数防抖；播放实时预览留 P1 |
| D076 | 实时预览方向 | 不复制颜色逻辑；CLI 生成 960 px 显示/胶片代理，AVPlayer 切换，参数 hash 缓存 |
| D077 | 代理缓存 | 默认 5 GB 软上限，可改目录/手动清理；超限删最旧，保留当前显示/胶片代理 |
| D077 | 生成时播放 | 生成胶片代理期间暂停播放，完成后自动续播；按空格可取消/恢复待播放 |
| D078 | 画幅 | 采用 SpektraLab 相机负片长边；所有当前画幅标 CINE；35mm 仍为 36 mm 全画幅，IMAX 70 mm |
| D078 | APP 版本 | 未发布统一 `内部测试（测试号） · 版本`；每轮更新 `AppVersion.testTag`，发布后 `isReleased = true` |
| D079 | 配光 | 中性滤色片已由数据库自动查表；按钮只补自动曝光（中位数 → 18% 线性中灰），不做 Y/M 偏移 |
| D080 | macOS OCIO | 不走 Homebrew 包（macOS 14 会拉 `python@3.15` 失败）；自建静态 OCIO 2.6.0，不编 Python 绑定；纯静态依赖 |
| D081 | GPU OCIO | 不把 OCIO 塞进着色器：CPU 批量解码后进现有光谱 GPU；额外保留原始编码帧做 `--strength` 混合 |
| D081 | 线性 EXR | OCIO 显示变换的逆没有定义；`--exr-linear` 直接写显示前线性 Rec.709，`--exr-trc` 不再参与 |

### 三、当前状态（截至最后一次操作）

- 最新功能代码：`6cb8055`（D081；其后的 `5f2be7f` 是 D081 验收文档 + help 的 `%` 转义）
- CI：D080 功能代码 run **148** Windows + macOS 双绿（含 `swift build`）；D081 run **153** Windows + macOS 双绿（含 Metal `gpu_film` 与 OCIO 步骤）
- Mac 仓库：`/Users/wenyutao/Documents/SpektraRoll`，已同步到 `6cb8055` 并本机验证（`build-macos-tests` 11/11）
- Mac app：已正常启动并验收（用 `open build-ui-macos/SpektraRoll.app` 可重新打开）
- Mac OCIO 前缀：`~/Documents/ocio-2.6.0-static`（OCIO 2.6.0，纯静态）
- Mac 代理缓存：`~/Library/Caches/SpektraRoll/preview-cache/`，上限 5 GB，最后约 9 MB / 4 个代理
- Mac 测试素材：`~/Movies/C0113.MP4`（3840×2160 / 50p / 312 帧 / 00:06）
- 当前 app state（最后一次检查）：`ocioEnabled=true`、`ocioInput=S-Log3 S-Gamut3.Cine`、`exposure≈0.664`、`filmID=kodak_portra_400`、`paperID=kodak_endura_premier`

**Mac 远程访问**

```powershell
$key = "$env:TEMP\spektra-mac-ed25519"
ssh -i $key wenyutao@192.168.5.142
```

- 非交互 SSH 的 PATH 只有 `/usr/bin:/bin`，脚本里要显式：
  ```bash
  export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin
  ```
- 如果 key 被临时目录清理，需要重新生成并让用户在 Mac 上加入 `authorized_keys`，或让用户本机操作。

**Mac 构建/运行**

```bash
cd /Users/wenyutao/Documents/SpektraRoll
export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin

# 第一次（或 OCIO 版本变化时）
scripts/build-ocio-macos.sh

# 之后
scripts/build-macos-ui.sh --no-open
open build-ui-macos/SpektraRoll.app

# D081：带测试的构建（含 Metal gpu_film / OCIO 对拍）
cmake -S . -B build-macos-tests -DCMAKE_BUILD_TYPE=Release -DSR_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH="$HOME/Documents/ocio-2.6.0-static" \
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON
cmake --build build-macos-tests -j "$(sysctl -n hw.ncpu)"
ctest --test-dir build-macos-tests --output-on-failure
```

**调试环境变量（仅 `open -F --env` 生效）**

| 变量 | 作用 |
|---|---|
| `SPEKTRA_AUTOLOAD=<path>` | 启动后自动导入并选中视频 |
| `SPEKTRA_AUTOPREVIEW=1` | 自动打开胶片效果（暂停单帧） |
| `SPEKTRA_AUTOPLAY=1` | 启动 3.5 s 后自动播放 |
| `SPEKTRA_AUTOGRADE=1` | 启动后自动执行一次配光 |
| `SPEKTRA_PLAYER_DEBUG=1` | 播放源切换写入 `/tmp/spektra-player.log` |
| `SPEKTRA_TAB=info / film / print / advanced / output` | 启动时打开指定检查器 tab |
| `SPEKTRA_PROXY_LIMIT_GB=<GB>` | 本次启动临时覆盖代理缓存上限 |
| `SPEKTRA_OCIO=1` | 自动启用 OCIO 并选 `S-Log3 S-Gamut3.Cine` |
| `SPEKTRA_CAPTURE=1` | app 自截图到 `/tmp/spektra-self-0.png`（触发：`touch /tmp/spektra-capture-now`） |
| `SPEKTRA_ALL_SPACES=1` | 窗口所有 Space 可见、定时前置（远程验收用） |

### 四、未完成的待办（建议顺序）

**P0 · OCIO 深度**
- [x] `--engine spectral-gpu` + OCIO：D081 已接通（CPU 批量解码 → 线性 Rec.709 → 光谱 GPU）；Windows 已验证，Mac/CI 待跑。
- [x] `--exr-linear` 的 OCIO 显示变换：D081 改为直接写显示前线性 Rec.709。

**P1 · 实时预览/性能**
- [ ] 4K50 6 秒素材的胶片代理生成在 M1 + spectral-gpu 约 **20–30 秒**；可考虑降到 720 px、代理固定走 LUT 引擎，或只预生成前几秒。
- [ ] 代理是整段预渲染；超长片段的首次播放等待需要优化。
- [ ] 切换缓存目录不迁移旧目录文件（当前只影响新代理）。

**P1 · 功能补齐**
- [ ] 4K50 `C0113.MP4` 整段 312 帧渲染验收（音频/timecode/ProRes）。
- [ ] 时间线拖动/逐帧预览、渲染队列、区间输入（macOS v1 仍没有）。
- [ ] SpektraLab 式 Y/M 滤色片偏移控件 + 独立 `solve --filter-pack`；当前配光只做自动曝光。
- [ ] 严格 35mm 电影画幅（Academy 21.95 mm）与 135 全画幅拆分；现在 35mm = 36 mm 全画幅并标 CINE。

**P2 · 交付/平台**
- [ ] macOS 公证/DMG；`.app` 仍未签名分发。
- [ ] ffmpeg/ffprobe 仍是 Homebrew 动态依赖，分发前要静态链接或捆绑；OCIO 已静态。
- [ ] R3D macOS（单独立项）。
- [ ] 印片机 Y/M/预闪/光源、扫描仪 unsharp、空间物理模型、PIZ/DWA。
- [ ] Windows Qt UI v3：等 macOS 设计冻结后按同一规格重做（当前只修 bug、冻结新功能）。

### 五、新对话第一步

```powershell
# 本机（Windows）
git -C X:\agent\SpektraRoll status
git -C X:\agent\SpektraRoll log --oneline -8
```

```bash
# Mac（如 SSH 可达）
cd /Users/wenyutao/Documents/SpektraRoll
export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin
git reset --hard origin/main
scripts/build-macos-ui.sh --no-open
open build-ui-macos/SpektraRoll.app
```

检查：

```bash
build-ui-macos/SpektraRoll.app/Contents/Helpers/spektra-roll-cli capabilities --json
# 应看到 "ocio": {"available": true, "version": "2.6.0"}
```

---

## 最新：D081 · 光谱 GPU 接 OCIO + 线性 EXR（2026-11-05 晚；Windows + Mac 已验，CI 待跑）

> 解决 D080 剩下的两个 P0 欠账：`--engine spectral-gpu` + `--ocio-input` 不再回退 CPU 光谱；`convert --ocio-display ... --exr-linear` 不再用 sRGB/PQ/HLG 猜显示变换的逆。

### 一、这次做了什么

- **GPU 的 OCIO 路径**（D3D11 + Metal）：
  - 不在着色器里实现 OCIO，也不让 GPU 链接 OCIO。`GpuFilm::processToDevice` 在上传前把输入 RGB 复制一份，调用 `OcioProcessor::apply(float*, pixels)` 批量解码成线性 Rec.709，再打包成 GPU 的 `float4`。
  - 着色器新增 `inputMode = 10`：表示输入已经是线性 Rec.709，只做 `max(0)` 钳制，不再跑内置 Log 曲线。
  - 为保持 `--strength < 1` 与 CPU 的 `sr::mix(原始编码值, 胶片结果, strength)` 一致，OCIO 路径额外上传一份原始编码帧；着色器混合时用它，而不是线性值。
  - `GpuFilm::configure` 复制 `OcioProcessor`（共享句柄），调用方的 recipe 可以先释放；重新 configure 会正确释放/重建编码缓冲。
- **移除回退**：`lut apply`、`video`、`preview` 里三处 `warning: OpenColorIO input is CPU-only for now` 全部删除；GPU 配置失败仍按原逻辑回退 CPU 光谱。
- **清掉 Qt 旧文案**：删除 `src/ui-qt/main.cpp` 里“OCIO 输入只能用 CPU 光谱”的翻译分支、渲染/队列开始前“会自动回退 CPU”的预提示，以及不再使用的 `argsUseOcioWithGpu()`；通用 `warning:` → “提示：”仍保留。
- **线性 EXR**：`convert --exr-linear` 遇到 `--ocio-display/--ocio-view` 时跳过显示变换，直接写显示前的线性 Rec.709。任意 view 的逆变换没有定义，之前按 sRGB 反推会得到错误的“线性”。`--exr-trc` 在这种组合下不参与。
- **测试/CI**：`test_gpu_film` 新增 `ocio logc3 / negative +gamut` 对拍；macOS CI 的 OCIO 步骤改为断言 `spectral-gpu` 日志里没有 `warning:`，并新增 OCIO 线性 EXR 冒烟。
- **UI/文档**：`AppVersion.testTag` 升到 **D081**；`docs/macos-dev-notes.md`、`docs/ui-parity.md`、`RUN.md`、`ui-macos/README.md` 更新为“已接通”。

### 二、Windows 本机验证

| 项 | 结果 |
|---|---|
| 构建 | Ninja Release 通过（`gpu_film.cpp` + `main.cpp`） |
| `ctest` | **11/11**；`gpu_film` 新增 OCIO 用例 worst **1e-6** |
| `lut apply` OCIO，CPU vs spectral-gpu | 320×180 / 16-bit，**109.43 dB** |
| 同上 `--strength 0.5` | **inf**（16-bit 读回逐位一致） |
| `convert --ocio-display ... --exr-linear` vs 普通线性 EXR | **inf** |
| `video --engine spectral-gpu --ocio-input "S-Log3 S-Gamut3.Cine"` | 2 帧 320×180 H.264 正常出片，无回退 warning |
| Mac `scripts/build-macos-ui.sh --no-open` | 通过（CLI + SwiftUI + `.app`） |
| Mac `ctest`（`build-macos-tests`） | **11/11**；`gpu_film` 真跑 Metal |
| Mac `test_gpu_film` OCIO 用例 | worst **0.000000** |
| Mac OCIO CPU vs Metal | 320×180 / 16-bit，**113.70 dB** |
| Mac `convert --ocio-display ... --exr-linear` | 与普通线性 EXR **inf** |
| Mac `video --engine spectral-gpu --ocio-input` | 2 帧 320×180 H.264 正常出片，无 warning |
| Qt UI | `build-ui.ps1 -NoLaunch` 通过；`--stocks-selftest` → `films=20 papers=8 ... slide=1` |
| CI run **153** | Windows + macOS **双绿**（macOS 真跑 Metal `gpu_film` / OCIO 步骤） |

### 三、仍未做 / 待验收

- ~~**UI 提示**：Qt 里“OCIO 输入只能用 CPU 光谱”的旧翻译还留着。~~ → 已清掉：Qt 的旧翻译分支和渲染/队列里的“会自动回退 CPU”预提示都删了。
- **性能**：OCIO 的 CPU 批量解码仍在每帧上传前执行；4K 有成本但远低于回退 CPU 光谱。后续可把 OCIO 变换烘成 3D LUT 上 GPU。

---

## 最新：D080 · macOS 静态 OCIO 2.6.0（2026-11-05 晚；Mac 已验收）

> 解决 D073 起 macOS 上 OCIO 不可用的问题。Homebrew 的 `opencolorio` 在 macOS 14 会尝试构建 `python@3.15`，clang tail-call 报错；改为自建静态 OCIO 核心（不编 Python 绑定）。

### 一、这次做了什么

- **新脚本** `scripts/build-ocio-macos.sh`（+ 双击 `build-ocio-macos.command`）：
  - 从本机 Homebrew 缓存或 GitHub 取 OCIO **2.6.0**；
  - `OCIO_BUILD_PYTHON=OFF`、APPS/TESTS/DOCS 全关，`BUILD_SHARED_LIBS=OFF`；
  - `OCIO_INSTALL_EXT_PACKAGES=ALL` 编静态依赖；
  - 安装到 `~/Documents/ocio-2.6.0-static`；
  - 补 `pystring` 的 CMake config、移除指向缺失 dylib 的 zlib config，让 `CMAKE_PREFIX_PATH` 能用纯静态依赖。
- **`scripts/build-macos-ui.sh`** 自动检测：
  - 默认找 `$HOME/Documents/ocio-2.6.0-static`；
  - `OCIO_PREFIX=...` 可覆盖；
  - 找到静态前缀就用 `CMAKE_FIND_PACKAGE_PREFER_CONFIG=ON`；否则退回 Homebrew 包或 stub。
- **结果**：app bundle 的 CLI `capabilities` 报 `ocio.available=true, version=2.6.0`；`otool -L` 不再依赖 Homebrew 的 OCIO/Imath/yaml-cpp/pystring/minizip dylib。
- **UI**：INFO 引擎区 OCIO 行显示“可用 2.6.0”；ADVANCED 的 OCIO 输入启用，显示 `S-Log3 S-Gamut3.Cine`；显示代理的 CLI 参数已带 `--ocio-config/--ocio-input`。APP 版本升到 **D080**。
- **调试**：`SPEKTRA_OCIO=1` 启动时自动启用 OCIO 并选 `S-Log3 S-Gamut3.Cine`。

### 二、验证

| 项 | 结果 |
|---|---|
| OCIO 静态构建 | 成功，`libOpenColorIO.a` 13 MB + Imath/expat/minizip-ng/pystring/yaml-cpp/zlib 静态库 |
| `capabilities --json` | `ocio.available=true`, `version=2.6.0` |
| `ocio-info` | 找到 `Linear Rec.709 (sRGB)` 和 S-Log3 系列空间 |
| `lut bake --ocio-input "S-Log3 S-Gamut3.Cine"` | 成功，17³ 表 |
| `otool -L` | 无 OCIO/Imath/yaml/pystring/minizip Homebrew dylib |
| UI 截图 | INFO 显示 OCIO 可用 2.6.0；ADVANCED OCIO 输入启用 |
| APP | `内部测试（D080） · 0.1.0` |

### 三、仍未做

- ~~**光谱 GPU 的 OCIO**：仍按 D062 回退 CPU 光谱。~~ → **D081 已完成**。
- ~~**`--exr-linear` 的 OCIO 显示变换**：仍只认 sRGB/PQ/HLG。~~ → **D081 已完成**（写显示前线性 Rec.709）。
- OCIO 代理生成/渲染速度：GPU 路径现在只多了每帧 CPU 批量解码，已远快于回退 CPU 光谱。

---


## 最新：D079 · 配光按钮（2026-11-05 晚；Mac 已验收）

> 产品负责人问 SpektraLab 的「配光」按钮怎么工作，并要求在 `导出` 右边也加一个。

### 一、SpektraLab 的配光做了什么

- 顶栏按钮中文「配光」，英文「Process」；调用引擎 `solve(target:"both")`。
- **测光**：对当前帧跑 meter（balanced / center / protect highlights / protect shadows），返回 `exposure_compensation_ev`，自动曝光节点在底片前应用。
- **滤色片**：用 `neutral_print_filters_from_database` + `digest` 解出当前胶片/相纸的中性 C/M/Y 放大机滤色片。
- UI 把用户的 Y/M 偏移清零（偏移是相对解出的中性值），再 `reprint` 当前帧。它是用户说“印这一帧”的动作，不是打开文件时自动做。

### 二、SpektraRoll 的配光按钮

- 位置：播放器控制条 `导出` 右边、`胶片效果` 左边。
- 中性滤色片：SpektraRoll 已经通过 `data/profiles/neutral_print_filters.json` 按胶片/相纸自动查表并烘进模型，UI 没有 Y/M 偏移控件，所以这一步不需要按钮再解。
- 缺少的是 **自动曝光**：按钮现在会
  1. 暂停播放，抽取当前帧；
  2. 用 `convert --from <input>` 解到显示 sRGB；
  3. 取画面亮度中位数，算出让 18% 线性中灰落在目标上的 EV（`FrameMeter`）；
  4. 写入 `settings.exposure`，打开 `胶片效果`，重渲当前帧；
  5. 状态栏显示例如 `配光：+0.66 EV（中性滤色片自动）`。
- 实测 `C0113.MP4`：显示帧中位数 0.371 sRGB → 线性 0.1137 → **+0.66 EV**；ADVANCED 曝光滑块同步到 `+0.66 EV`，胶片效果刷新。
- 仍未做：SpektraLab 那样的 Y/M 偏移控件和独立 `solve --filter-pack`；如果以后要严格对齐，可以给 CLI 加 `solve` 命令并暴露 C/M/Y 偏移。

### 三、APP 版本

- `AppVersion.testTag` 升到 **D079**；INFO 引擎区显示 `内部测试（D079） · 0.1.0`。

### 四、调试开关

- `SPEKTRA_AUTOGRADE=1`：启动后自动执行一次配光（配合 `SPEKTRA_AUTOLOAD` 验收）。

---


## 最新：D078 · 电影画幅 CINE 标识 + APP 版本（2026-11-05 晚；Mac 已验收）

> 产品负责人问 FILM 画幅的实现路径，并要求电影画幅加 CINE 标识、INFO 引擎区加 APP 版本。

### 一、画幅功能怎么实现

- **SpektraRoll**：`FrameGauge`（SwiftUI）只保存一个 **`widthMM`（长边）**；`CLIArguments` 把它写成 `--frame-width-mm`；CLI 的 `parseTexture` 存进 `sr::TextureParams.spread.frameWidthMm`；质感核按 `像素 sigma = µm × 画面宽度 / frameWidthMm` 换算。它**只决定颗粒/光晕/眩光/扩散的物理尺度**，不裁切画面、不改变宽高比。
- **SpektraLab**：`FilmFrame` 有 `short/long` 两个边长、`isCine`、`sideLengthMM` 和 Custom；选择画幅后 `derivedFilmFormatMM` 取长边，再算 `pixel_size_um = long_mm × 1000 / long_edge`。思路相同（物理宽度 → 像素尺度），但它还支持短边/长边、静态/电影、Custom。
- **结论**：路径同一原则，但不是逐行同构。本轮把电影长边对齐 SpektraLab 的相机负片数据，并补了 Super 8：
  - Super 8 **5.79 mm**
  - 16mm **10.26 mm**（原 12.5）
  - Super 16 **12.52 mm**
  - 35mm **36.00 mm**（保留 135 全画幅宽度）
  - Super 35 **24.89 mm**（原 24.9）
  - 65mm **52.63 mm**（原 52.5）
  - IMAX **70.00 mm**
  - 全部标 **CINE**；下拉菜单和当前画幅行都显示 CINE，旁边显示物理宽度。
- **仍未做**：短边/画幅比例、Custom 自定义宽度、按画幅裁切。SpektraLab 的 cine 列表没有 35mm 和 IMAX（36mm 在它那里是静态 135）；如果你想严格区分，可以再拆成「35mm 电影（Academy 21.95）」+「135 全画幅」两个选项。

### 二、APP 版本

- 新增 `AppVersion`：市场版本读 `CFBundleShortVersionString`（0.1.0），内部测试号 `testTag = "D078"`，`isReleased = false`。
- INFO → 引擎面板第一行 **APP**：`内部测试（D078） · 0.1.0`。发布后把 `isReleased = true`，测试号自动消失。
- 每轮内测更新 `testTag`。

### 三、验收

- FILM 截图：画幅行显示 `CINE` + `36.00 mm`，下拉当前项 `35mm · CINE`。
- INFO 截图：APP 行 `内部测试（D078） · 0.1.0`。
- Mac 本机 Swift build 通过；`data/ui-schema.json` 的 frameWidth 下限从 8.0 降到 5.0，允许 Super 8。
- CI 待本轮提交后跑。

---


## 最新：D077 · macOS UI v1.1 反馈（2026-11-05 晚；Mac 已验收）

> 产品负责人在 D076 播放代理上提出 4 点，已全部实现并在 Mac 上截图/日志验收。

### 一、4 点反馈与实现

| # | 反馈 | 实现 |
|---|---|---|
| 1 | 代理缓存体积上限 5 GB、ADVANCED 可填、可自定义目录、超限主动清理 | ADVANCED 新增「预览代理」：磁盘上限（默认 **5 GB**，0 = 不限制）、缓存位置（`NSOpenPanel` 选择，持久化）、当前占用、手动清理；每次生成后/启动时/改上限时清理最旧代理，保留正在播放的显示/胶片代理。 |
| 2 | 生成胶片代理时不要播放视频 | 开始生成胶片代理时立即暂停播放，记住“用户想播放”；生成完成后自动切到胶片代理并继续播放。按空格可取消/恢复待播放。 |
| 3 | 进度胶囊放到视频标题栏左下 | 从底部移到标题信息行下方、左对齐。 |
| 4 | INFO 追加电脑配置（Mac/PC、系统、屏幕、CPU、GPU、RAM、存储、HDR、屏幕色彩空间） | INFO 底部新增 **Mac** 面板（`#if os(macOS)` 标题 Mac，否则 PC）；读取 `NSScreen` / Metal / `sysctl` / `FileManager`。 |

顺手修掉：调试自动导入/重复导入同一路径会在片库里出现两个同名素材；现在按路径去重。

### 二、关键文件

- `AppState.swift`：代理上限/目录、按大小清理、`isGeneratingFilmProxy`、系统信息无关。
- `PlayerController.swift`：`blockPlaybackForFilmProxy` / `unblockAfterFilmProxy`，暂停期间记住播放请求。
- `PlayerView.swift`：生成胶片代理时拦截播放、进度胶囊位置、代理切换。
- `AdvancedTab.swift`：预览代理设置。
- `InfoTab.swift` + `Models/SystemProfile.swift`：Mac/PC 配置信息。
- `Package.swift`：链接 Metal 框架。

### 三、Mac 验收结果

- INFO 截图：Mac 面板显示 macOS 14.7.5、Built-in Retina Display 2880×1800 2x、Apple M1 / Apple M1、8 GB、30.67 GB 可用 / 245.11 GB、HDR 开启（EDR 2.00x）、Display P3。
- 播放日志：`block film proxy playing=true resume=true` → `switch film ... play=false` → `unblock film proxy play=true`；期间没有 `switch display`，确认生成时没有播放未上胶片的画面。
- 进度胶囊：截图确认在标题行左下。
- 缓存上限：临时放入 3 个 2 MB 假代理 + 2.3 MB 真实代理，限制设为 5 MB；启动后 3 个假代理全部被删，保留 2 个真实代理（4.6 MB）。
- 片库去重：重启后只显示 1 个 `C0113.MP4`。

### 四、调试开关（新增）

- `SPEKTRA_PROXY_LIMIT_GB=<GB>`：本次启动临时覆盖缓存上限（测试用）。
- `SPEKTRA_TAB=info|film|print|advanced|output`：启动时直接打开指定检查器 tab。
- `SPEKTRA_AUTOPLAY=1` / `SPEKTRA_PLAYER_DEBUG=1`：见 D076。

### 五、待做

- 最终 CI 回归（代码提交后）。
- 代理生成速度：4K50 6 秒素材、spectral-gpu 960 px 在 M1 上约 20–30 秒；可考虑 720 px 或 LUT 代理。
- 缓存目录切换后旧目录文件不会自动迁移（只影响新代理）。

---


## 最新：D076 · macOS UI v1.1 P1（播放代理，2026-11-05 晚；已在 D077 验收）

> D074 #3：播放时不再只显示 AVPlayer 的原始 Log 画面，也不再只有暂停时的单帧胶片效果。UI 会生成低分辨率 **显示代理** 和 **胶片代理**，按需切换 AVPlayer，并缓存到本地。CI run **142** Windows + macOS 双绿（含 `swift build`）；Mac 本机构建通过，显示代理/胶片代理生成与 `switch film` 已在 D077 播放日志中确认。

### 一、这次做了什么

**CLI（`video`）**
- `--max-side N`：解码后、调色前按长边等比缩小（偶数尺寸，只缩小不放大）。用于 960 px 播放代理。
- `--display`：只做输入色彩空间 → linear Rec.709 → sRGB，不上胶片/LUT；OCIO 输入走 `--ocio-config/--ocio-input`。事件里的 `engine` 是 `"display"`。
- 两个代理都保留源音频和 timecode；R3D 暂不支持 `--max-side`。

**macOS UI**
- 打开素材后后台生成 **显示代理**（`--display --max-side 960`）；按播放时如果胶片效果开着，再生成 **胶片代理**（当前 LUT/光谱/质感参数）。
- 缓存目录：`~/Library/Caches/SpektraRoll/preview-cache/`；文件名按素材（路径/大小/修改时间）+ 输入/OCIO + 参数 hash 计算。旧缓存 7 天清理，中断的 `.part.mov` 24 小时清理。
- AVPlayer 在“原始 / 显示代理 / 胶片代理”之间切换，保留当前播放位置；切换期间抑制暂停事件，避免误触发静帧渲染。
- 胶片效果关闭时播放显示代理；开启时优先生成/播放胶片代理，未就绪则先用显示代理。
- 参数变化 500 ms 防抖后重建胶片代理；播放器继续播旧代理，新的就绪后自动切换。
- 生成期间显示“生成原片代理 / 生成胶片代理”进度胶囊；最终渲染开始时取消代理生成，避免抢 CPU。
- 调试钩子：`SPEKTRA_AUTOPLAY=1` 启动 3.5 s 后自动播放；`SPEKTRA_PLAYER_DEBUG=1` 把播放源切换写入 `/tmp/spektra-player.log`。

### 二、已验证

| 项 | 结果 |
|---|---|
| CLI `video --display --max-side 960` | 960×540 / H.264 / 50 帧，**59.2 ms/帧**；源音频和 timecode 保留 |
| CLI 胶片代理 `--max-side 960` | 960×540 / H.264 / 50 帧，**52.7 ms/帧**（LUT 路径） |
| CLI OCIO 显示代理 | 480×270 / 10 帧，OCIO 解码正常 |
| `ctest` | **11/11** |
| Swift build（Mac 本机） | **通过** |
| CI | run **142** Windows + macOS 双绿 |
| Mac 运行时 | 显示代理已生成（`1af621bb04342595.mov`，2.3 MB）；播放切换待验收 |

### 三、待做

- 在 Mac 上实际播放验收：显示代理播放、胶片代理生成后切换、暂停/继续、参数变化后自动更新。
- 代理生成速度：6 秒 4K50 素材在 M1 上预计数秒到十几秒；如果偏慢，可降到 720 px 或先只生成前几秒。
- 代理播放期间的音画同步/最后一帧切换细节。

---


## 最新：D075 · macOS UI v1.1 P0（2026-11-05 晚；P1 已在 D076 实现）

> 按 D074 的 P0 顺序完成 #4 / #6 / #1 / #2，并顺手做了 #5 的参数即时刷新；播放时的实时胶片预览（#3）留到 P1。CI run **140** Windows + macOS 双绿（含 `swift build`），但 Mac 上的实际观感还没验收。

### 已完成

| 反馈 | 实现 |
|---|---|
| **#4 关闭胶片效果显示 sRGB** | 暂停时用 `convert --from <input>`（OCIO 则 `--ocio-input`）把当前帧解码到 sRGB，作为“原片”显示，不再直接显示 AVPlayer 的 Log 画面。播放中仍走 AVPlayer；**D076 已用显示代理替换**。 |
| **#6 导出按钮** | 播放器控制条 `胶片效果` 左侧新增“导出”，点击切到 `OUTPUT` tab。 |
| **#1 直方图 + 宽容度** | 视频下方、控制条上方；RGB/亮度直方图 + 宽容度（0.1%–99.9% 线性光档数）、亮度范围、高光/暗部裁切百分比。从当前静帧 720 px 降采样计算。 |
| **#2 缩放/平移** | 滚轮/双指缩放、拖拽平移、`适合` / `100%` 按钮、双击切换；100% = 1 源像素对 1 显示点。 |
| **#5 参数即时刷新** | 胶片效果开启且暂停时，参数变化 180 ms 防抖后自动重渲；渲染中再改参数会标记 dirty，完成后自动补最新一帧。 |

### 关键文件

- `ui-macos/Sources/SpektraRoll/Features/Player/PlayerView.swift`：缩放/平移、导出、布局。
- `.../FrameStats.swift`、`.../HistogramView.swift`：直方图与宽容度/裁切数据。
- `App/AppState.swift`：display 预览、防抖、dirty 队列。
- `Engine/EngineClient.swift`：`convertToDisplay`。
- `Design/Components.swift`：小号胶囊按钮。

### 验收（Mac）

1. `scripts/build-macos-ui.sh --no-open` → `open build-ui-macos/SpektraRoll.app`
2. 双击 `C0113.MP4` → 暂停时应显示解码后的 sRGB 画面（不是 Log 灰片），下方有直方图。
3. 滚轮缩放、拖拽平移、`适合` / `100%`、双击切换。
4. 开 `胶片效果`，拖动 FILM / PRINT / ADVANCED 任意滑块，画面自动刷新。
5. `导出` 按钮跳到 OUTPUT。

### 仍未做（P1）

- **#3 播放时实时胶片预览**：按参数 hash 生成低分辨率显示代理 + 胶片代理，AVPlayer 切换，缓存失效/清理。**✅ 已在 D076 实现**。
- 播放中关闭胶片效果时仍走 AVPlayer 的 Log 画面；**D076 已改为显示代理**。

---

## 最新：D074 · macOS UI v1 验收反馈（2026-11-05 晚；P0 已在 D075 实现）

> 产品负责人已在 Mac 上实际验收 D073：**效果比预期好**。以下 6 点是下一轮（macOS UI v1.1）要改的；**P0 的 #4/#6/#1/#2 和 #5 已在 D075 实现，#3 已在 D076 实现**。

### 一、这轮已完成的事

**远程接管**
- 通过局域网 SSH 连上 Mac：`wenyutao@192.168.5.142`，macOS 14.7.5 / Xcode 16.2 / Swift 6.0.3。
- 克隆仓库到 `/Users/wenyutao/Documents/SpektraRoll`；非交互 SSH 的 PATH 只有 `/usr/bin:/bin`，所有脚本显式加 `/opt/homebrew/bin`。
- Mac 上直接编译、打包、启动、验收，不再依赖 GitHub macOS runner 排队。

**构建与打包**
- `scripts/build-macos-ui.sh` 成功：CLI + SwiftUI + `.app`（约 167 MB），包含 `spektra-roll-cli`、ffmpeg/ffprobe 9.0.2、`data/profiles`、`data/ocio`、Noto Sans CJK SC Regular/Bold。
- `scripts/fetch-macos-fonts.sh` 下载并打包字体；ad-hoc 签名通过。
- 修复：`AppState.inspectorTab` 缺失（CI #125/#126 失败原因）、播放器时长 00:00、`video` 未传 `--ffmpeg` 绝对路径、深色标题栏、白色 `+` 按钮、导入按钮白色药丸。
- 增加**仅环境变量生效**的调试钩子：`SPEKTRA_ALL_SPACES` / `SPEKTRA_CAPTURE` / `SPEKTRA_AUTOLOAD` / `SPEKTRA_AUTOPREVIEW` / `SPEKTRA_AUTORENDER`；正常启动不受影响。

**验收结果**
- 三栏、字体、白色药丸、缩略图、AVPlayer 播放、空格、INFO、胶片效果、UI 渲染均正常。
- 真实 `C0113.MP4`：3840×2160 / 50p / 312 帧 / 00:06；缩略图、播放、INFO、4K 单帧胶片预览成功（`~/Library/Caches/SpektraRoll/preview/graded.png`）。
- 合成 3 秒测试片：UI 触发渲染 → 640×360 / 3.00 s / H.264 / yuv420p。
- 远程截图坑：SSH 在 `Background` launchd domain，`screencapture` 抓不到 GUI 窗口；用 app 内 `SPEKTRA_CAPTURE` 自截图。Stage Manager 临时关闭后已恢复。

**GitHub**
- 最新两个 run 的 macOS job 是 **cancelled**（runner 容量/取消），不是编译红。
- #125/#126 确实失败于缺少 `inspectorTab`，已修；**#128 `swift build` 通过**。之后以 Mac 本机编译为准。

**环境改动 / 已知限制**
- OCIO **当前不可用**：macOS 14 上 Homebrew 无 OCIO bottle，源码构建 `python@3.15` 被 clang tail-call 错误挡住。
- 为装 OCIO，把原来的手动 cmake 4.4.4 备份到 `/opt/homebrew/opt/cmake-manual-4.4.4`，切到 Homebrew cmake 4.4.5（正常可用）。
- 首版用内置 10 种 Log 输入；R3D 在 macOS 仍禁用。
- `.app` 未公证；ffmpeg/OCIO 仍是 Homebrew 动态依赖，分发前要静态链接/捆绑。

### 二、你这次提出的 6 点反馈（全部待做）

1. **直方图 + 宽容度等数据参考**：放在中间视频画面下方、`胶片效果` 按钮那一排的上方。
2. **视频画面缩放**：能放大/平移仔细看；至少支持适合窗口 / 100% / 滚轮缩放。
3. **播放时实时胶片预览**：现在只有暂停时单帧；希望在按下播放时做**预渲染缓存**，然后实时播放胶片效果。
4. **关闭胶片效果时的“原片”**：应该显示**对应色彩空间解码到标准显示（sRGB）**的画面，而不是 Log 灰片；否则对比没有意义。
5. **参数即时刷新**：胶片效果开启时，任何参数变化都应立即反映到画面，不需要用户重新开关。
6. **导出按钮**：放在 `胶片效果` 按钮左侧；按下直接跳到 **OUTPUT** tab。

### 三、下一轮关键决策（D074）

| 决策 | 结论 |
|---|---|
| 优先级 | 先做 UI v1.1 的 6 点反馈；**OCIO 单独一轮**，不混进来 |
| 实时预览方向 | **播放时预渲染缓存**：按当前参数生成低分辨率/代理预览片段，缓存后交给播放器；参数变化使缓存失效 |
| 关闭胶片效果 | 不能直接放 AVPlayer 原片；要显示“输入色彩空间 → 标准显示 sRGB”的解码结果 |
| 参数变化 | 防抖 + 使预览缓存失效 + 自动刷新当前帧（胶片效果开启时） |
| 导出按钮 | 播放器控制条增加“导出”白色药丸，点击 `inspectorTab = .output` |
| Windows Qt | 继续冻结（只修 bug，不加功能），等 macOS v1.1 设计稳定 |

### 四、未完成待办（按建议顺序）

**P0 · 低风险、先做**
- [ ] #4 关闭胶片效果时显示“输入解码到 sRGB”的显示画面（不是 Log 灰片）。
- [ ] #6 播放器增加“导出”按钮，跳 OUTPUT。
- [ ] #1 视频下方加直方图 + 宽容度/亮度范围/高光暗部裁切参考。
- [ ] #2 画面缩放/平移/适合/100%。

**P1 · 实时预览（最大块）**
- [ ] #3 播放时预渲染缓存：按参数 hash 生成低分辨率显示代理 + 胶片代理；AVPlayer 切换；缓存失效/清理。
- [ ] #5 参数变化自动刷新（防抖 + 缓存失效 + 当前帧重渲染）。

**P2 · 后续**
- [ ] OCIO：MacPorts / 自建静态 OCIO / 升级 macOS / 固定 python 版本，四选一。
- [ ] 真实 4K50 `C0113.MP4` 的整段渲染（音频/timecode/ProRes）验收。
- [ ] 时间线拖动/逐帧预览、渲染队列、区间输入。
- [ ] macOS 公证/DMG、ffmpeg/OCIO 静态依赖。
- [ ] R3D macOS（单独立项）。

### 五、实现提示（给下一个 AI）

- **直方图/宽容度**：可先在 Swift 里从预览 RGBA 计算 RGB/luma 直方图、min/max、0.1%/99.9% 分位、高光/暗部裁切百分比；若要和 CLI 完全一致，再加 `preview` 协议的 `histogram` 字段。
- **缩放**：播放器与静帧共用 `zoom`/`offset`；双击切适合/100%；AVPlayerView 可用 `NSScrollView` 包一层或对 SwiftUI 层做 scale/offset。
- **预渲染缓存**：建议新增 CLI 侧“预览片段”能力（低分辨率代理 + 参数 hash），不要让 UI 复制颜色逻辑；缓存目录 `~/Library/Caches/SpektraRoll/preview-cache/`；播放前先生成，生成期间显示进度/源片。
- **关闭胶片效果**：用同一套代理管线生成“display”版本（`convert --from <input>` 或等价 LUT），与“film”版本成对缓存。
- **参数变化**：`LookSettings` 变更时 `objectWillChange`/`onChange` 触发防抖 150–250 ms，使缓存失效并刷新当前帧。

---

## 最新：D073 · macOS SwiftUI 三栏初版

> **状态**：Mac 上已构建并验收（2026-11-05）；产品负责人提出 6 点改进，见上方 **D074**（下一轮做）。
>
> 新开对话先读 D074；D072 是引擎契约，D071 及更早是 CLI/引擎历史。

### 一、这次做了什么

- 按产品负责人的设计输入，建立 macOS SwiftUI 初版：
  - 三栏：左「底片」220 / 中播放 / 右检查器 380。
  - 深色近黑；白色药丸按钮 + 黑字；思源黑体，标题加粗。
  - 左侧双击空白导入视频/项目目录；单列缩略图；双击缩略图载入播放器。
  - 导入弹窗：画面类型 / OCIO 色彩空间；OCIO 配置后台自动选择。
  - 中间 AVPlayer 播放；空格播放/暂停；暂停时可开「胶片效果」单帧预览。
  - 右侧 INFO / FILM / PRINT / ADVANCED / OUTPUT。
- 关键文件：
  - `ui-macos/Package.swift` + `ui-macos/Sources/SpektraRoll/**`（SwiftUI）。
  - `scripts/build-macos-ui.sh`：编 CLI → 编 SwiftUI → 组装 `.app` → 拷 data/ffmpeg/字体 → ad-hoc 签名。
  - `scripts/fetch-macos-fonts.sh`：下载 Noto Sans CJK SC（OFL）。
  - `docs/ui-design-macos.md`：布局/交互/参数映射/验收。
  - `design/tokens.json` 升到 v2：暗色品牌、白色药丸、思源黑体、三栏尺寸。
  - CI macOS job 增加 `swift build --package-path ui-macos -c release`。

### 二、当前状态（诚实）

| 项 | 状态 |
|---|---|
| SwiftUI 源码 | 已写；Windows 本机不能编译 SwiftUI，但 macOS 本机 `swift build` 通过 |
| macOS 编译 | **✅ CI #128 + Mac 本机 `swift build` 通过**；#125/#126 因缺 `inspectorTab` 失败，已修 |
| `.app` 双击验收 | **✅ 已在 Mac 上构建并启动**（Stage Manager 暂时关闭用于截屏，已恢复） |
| 导入/缩略图 | **✅** 3 秒测试片 + 真实 `C0113.MP4`（3840×2160 / 50p / 312 帧）均正常 |
| 播放/空格 | **✅** AVPlayer 播放；真实素材时长 00:06；空格由菜单快捷键处理 |
| INFO / 胶片效果 | **✅** 4K50 INFO 元数据正常；胶片效果单帧渲染成功（4K graded.png） |
| 渲染 | **✅** UI 触发渲染 → 640×360 / 3.00 s / H.264 / yuv420p，输出到 `~/Movies/SpektraRoll` |
| 字体 | **✅** Noto Sans CJK SC Regular/Bold 已下载并打包（OFL；思源黑体同设计） |
| ffmpeg | **✅** Homebrew ffmpeg/ffprobe 9.0.2 已打包 |
| OCIO | **❌ 当前不可用**：macOS 14 上 Homebrew 无 bottle，源码构建 `python@3.15` 失败；cmake 已切到 Homebrew 4.4.5。首版用内置 10 种 Log 输入 |
| R3D | macOS v1 明确禁用 |

### 三、Mac 上验收（2026-11-05 已完成）

```bash
export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin
cd /Users/wenyutao/Documents/SpektraRoll
scripts/fetch-macos-fonts.sh
scripts/build-macos-ui.sh --no-open
open build-ui-macos/SpektraRoll.app
```

已验收：`.app` 启动 → 三栏 → 自动导入 3 秒测试片 → 缩略图 → 播放/时长 00:03 → INFO 元数据 → 胶片效果（Portra 400 负片观感）→ 渲染 H.264 640×360 / 3.00 s。

远程验收注意：SSH 会话在 `Background` launchd domain，`screencapture` 抓不到 GUI 窗口；用 app 内 `SPEKTRA_CAPTURE=1` 自截图。Stage Manager 会隐藏窗口，验收时临时关闭、结束后恢复。细节见 `docs/macos-dev-notes.md`。

### 四、下一步

1. **OCIO**：解决 `/opt/homebrew/opt/cmake` 冲突后安装 `opencolorio`，或在 Mac 上自建静态 OCIO。
2. 用真实 `C0113.MP4` 验收 4K50 / S-Log3 / 音频 / timecode 的导入、预览、渲染。
3. 视觉细节：按钮宽度、间距、右栏密度、空状态。
4. 之后才做时间线逐帧预览、渲染队列、R3D macOS、公证。

---

## 最新：D072 · macOS SwiftUI 决策 + 引擎契约（Phase 0）

> 新开对话先读这一节；下面“零之零”的 D067–D071 仍是上一轮摘要。

### 一、这次定了什么

| 编号 | 决策 | 结论 |
|---|---|---|
| D072 | macOS UI | **SwiftUI 壳 + 现有 CLI**（不直接链接 C++ 核心） |
| D072 | 分发 | 先做**未签名 `.app` 本地验收**；公证/Developer ID 后补 |
| D072 | v1 范围 | **与 Qt 功能对齐**，不是精简 MVP |
| D072 | 双 UI 策略 | macOS 是**参考实现**，`design/` 是设计源；macOS v1 + 设计冻结前，Windows Qt 只修 bug；之后 M6 按规格重做 Windows UI v3 |
| D072 | 防漂移 | `data/ui-schema.json` + `docs/ui-parity.md` + golden argv + CI 双 UI 编译 |
| D072 | macOS R3D | **本期不做**；UI 明确显示“R3D 仅 Windows” |

### 二、Phase 0 已交付（CLI + 契约，不碰 UI）

- `capabilities --json`：版本、平台、OCIO/RED/GPU 能力、输入/编码/预览协议；UI 用它决定控件可用性。
- `catalog --json`：直接输出 UI 应显示的 **20 底片 / 8 相纸印片**，含 `targetPrint`、`positive`、`printFilm`；SwiftUI 不再重写过滤规则。
- `video --progress json`：输出 `start` / `progress` / `done` 单行 JSON；并行分段有互斥锁，行不会写坏。
- `preview` 协议升到 **2**，新增 `hello` 握手；`frame`/`raw` 仍是 8-bit RGBA 文件传输。
- `data/ui-schema.json`：42 个参数的标签、范围、默认值、CLI flag、显示条件（Qt 与 SwiftUI 共用）。
- `design/tokens.json`：v0 基线（当前 Qt 配色/字号/间距），等 macOS 设计替换。
- `docs/engine-api.md`：两个 UI 共同遵守的引擎契约。
- `docs/ui-parity.md`：功能矩阵、v1 范围、防漂移五道闸。
- `scripts\verify-d072.cmd`：双击验收（capabilities / catalog / preview hello / ctest / JSON 进度）。
- 顺手修掉 CLI `help` 里“R3D 无音频、不支持 segment”的过期说明。

### 三、验证

| 项 | 结果 |
|---|---|
| `capabilities --json` | 合法 JSON；Windows/d3d11/OCIO 2.4.2/RED stub 正确 |
| `catalog --json` | 合法 JSON；**20 films / 8 papers**；约 213 ms |
| `preview hello` | `protocol: 2` |
| `video --progress json` | 48 帧：`start` + 2×`progress` + `done`；`--segment 1 --jobs 2` 下 JSON 行完整 |
| `ctest` | **11/11**（含 GPU / OCIO / RED stub） |
| `scripts\verify-d072.cmd` | **5/5 OK**：capabilities / catalog / preview hello / ctest / JSON 进度 |
| Qt UI | 未改；本轮不碰 `src/ui-qt/main.cpp` |

### 四、下一步（Phase 1）

1. 在 Mac 上装 **Xcode + command line tools**。
2. 建 `ui-macos/`（建议 XcodeGen，不要手改 `.pbxproj`）。
3. `scripts/build-macos-ui.sh`：CMake 编 CLI → Xcode 编 app → 把 CLI/ffmpeg/data 打进 `.app` → ad-hoc 签名。
4. 最小验收：双击 `.app` → 选 `C0113.MP4` → 显示首帧。
5. 之后才是素材/片库/烘焙/预览/渲染/队列的逐项对齐。

### 五、当前限制

- Phase 0 只做了 CLI/文档；macOS UI 还是一行代码都没写。
- 设计输入还没给：需要产品负责人提供参考图/布局思路，并明确“macOS 原生感”还是“品牌统一专业工具感”。
- macOS 上 OCIO 仍是 Homebrew 动态库；`.app` 交付前必须静态链接或捆绑 dylib + 签名。
- ffmpeg/ffprobe 还没打包；R3D 插件 macOS 仍不可用。
- Windows Qt 已进入冻结：只修 bug，不加新功能，避免两遍开发。

---

## 零、给下一个 AI 的开场白（可以直接复制）

```
你接手一个叫 SpektraRoll 的项目，仓库在 X:\agent\SpektraRoll
（远程 https://github.com/haichangjyue/SpektraRoll）。

先读这三份文件，再开始干活：
  1. progress.md        ← 就是本文件，全局状态
  2. DECISIONS.md       ← 决策台账，不要推翻已定的事
  3. STATUS.md          ← 一页纸人话进度

我的角色：产品负责人，没有编程基础，只做决策和验收。
你的角色：工程负责人 + 全部代码实现。

工作方式（已经约定好的，不要改）：
  - 每次会话最多问我 3 个问题，且都是"选 A 还是 B"，不问技术细节
  - 每次交付必须有一个我能双击验证的东西
  - 所有决定写进 DECISIONS.md
  - 每次结束时更新 progress.md 和 STATUS.md

测试素材固定用 X:\agent\footage\C0113.MP4
（Sony FX30，S-Log3 / S-Gamut3.Cine，4K50，全范围，PCM 音频，timecode 00:13:37:46）。
不要再用旧的 A001_11011538_C001.mov；视频文件不进仓库。

R3D 测试（D068–D071）：
  - 样片：X:\agent\footage\ZR_RED_Color_Science_Sunflower.R3D
    （Nikon ZR R3D NE，6048×3402，249 帧，23.976，TC 11:09:14:03，0 音频）
  - SDK：X:\agent\footage\R3DSDKv9_2_1（不进仓库）
  - 本机插件：build-red\spektra-red.dll（MSVC + 用户 SDK 本地编译）
  - 官方 build\ 永远不带 RED；只有 build-red\ 能解码 R3D。
```

---

## 零之零、本次对话（D067–D071）交接摘要

> **新开对话先读这一节**；更早的 D063–D066 在下一节，细节按决策编号往下（最新在前）。
>
> 本次对话从 D067 开始：先做反转片直扫，然后接入 RED R3D / R3D NE（D068 骨架 → D069 解码 → D070 直读视频 → D071 音频/UI/segment）。

### 这次完成了什么

| 编号 | 交付 | 关键结果 |
|---|---|---|
| **D067** | 反转片直扫 | Ektachrome 100 / Velvia 100 / Provia 100F / Kodachrome 64；`--scan slide` 保留片基、按“清澈片基 → 最深染料”黑白平衡、中灰按通道中和；正片自动直扫，错误组合明确报错；CPU / D3D11 / Metal 同步；4K 单帧 CPU 2685.6 ms vs GPU 261.3 ms（**119.09 dB**）；界面片库 **20 支底片** |
| **D068** | RED R3D BYO-SDK 骨架 | `red-info [--in] [--json]`、`.r3d` 识别、`scripts\build-red` + `docs/red-sdk.md`；官方构建只带 loader。**D068 的静态 `SR_WITH_REDSDK` 路径已在 D069 被插件架构取代，新对话不要再用它。** |
| **D069** | R3D NE 帧解码 | Windows SDK 是 MSVC-only → 核心 GPL CLI + 本地 **MSVC 插件** `spektra-red.dll`（C ABI）；`red-decode` 解单帧/序列，默认 **REDWideGamutRGB + Log3G10**，也支持 linear/sRGB、full/half/quarter；真实 Nikon ZR 样片：6048×3402 / 249 帧 / 23.976 / TC 11:09:14:03；half 约 3.8 s/帧、full 6K 约 14 s/帧（CPU，含 PNG 写盘） |
| **D070** | R3D 直读视频 | `video --in clip.R3D` 直接出片；LUT 引擎解 **Log3G10/RWG**，光谱引擎解 **sRGB**；`--red-scale full/half/quarter`；timecode/帧数/帧率来自插件；默认 ProRes 422 HQ；half LUT **2.0 s/帧**、half 光谱 GPU **2.29 s/帧**、quarter **0.69 s/帧** |
| **D071** | R3D 收尾 | 插件音频（AU → PCM/AAC）、`--segment/--resume/--jobs`、UI `.r3d` 预览/单条渲染；`--red-selftest` ≈ **7.9 s**（3024×1701）、`--red-render-selftest` 2 帧 ProRes；**真实 R3D 音频、批量 R3D、GPU/并行解码仍未验证** |

- 本机：`ctest` **11/11**；`verify-m1` **88.07 dB**；UI `stocks selftest: ok films=20 papers=8 ... slide=1`。
- CI：run **121** Windows + macOS 双绿（官方构建无插件，只走“plugin not found”路径）。
- 最新代码提交：`d6adb83`（D071 代码）；之后只有文档更新（DECISIONS / STATUS / RUN / README / docs/red-sdk.md / 本文件）。
- 双击验收：
  - `scripts\demo-slide.cmd`（D067 四支反转片）
  - `scripts\demo-cine.cmd`（D065 电影片库）
  - `scripts\demo-red.cmd`（5 步：R3D 解码 → Log3G10 → Vision3 250D + 2383 → 3 帧 ProRes）
  - `build-ui\spektra-roll-ui.exe --red-selftest` / `--red-render-selftest`

### 关键决策

| 编号 | 决策 | 结论 |
|---|---|---|
| D067 | 反转片直扫 | `ScanMode::Positive`；保留片基；扫描仪黑白平衡 + 中灰中性；正片默认直扫；CPU/GPU 三态 `scanMode` |
| D068 | RED R3D 依赖策略 | BYO SDK；官方包/CI 永远不带 SDK；**D069 起改为 loader + 本地 MSVC 插件，`SR_WITH_REDSDK` 已移除** |
| D069 | R3D 解码架构 | Windows SDK 是 MSVC-only → 不 MinGW 直链；插件 C ABI；输出 Log3G10/RWG 16-bit；官方包不带 SDK |
| D070 | R3D 直读视频 | `VideoRender.red` + `RedReader` 帧源；LUT=Log3G10/RWG，光谱=sRGB；默认 ProRes；timecode 跟 `--start` |
| D071 | R3D 收尾 | 插件写 AU + ffmpeg 封装；segment 每段独立 RedReader；UI 强制 Log3G10/RWG LUT + red-decode 预览；选 R3D 自动 ProRes |

### 未完成的待办（按建议顺序）

1. **真实 R3D 音频样片验证**：代码已接通，但 `ZR_RED_Color_Science_Sunflower.R3D` 是 0 音频通道；目前只验证了无音频路径和合成 AU 封装。
2. **UI 批量 R3D 入队**：单条预览/渲染已可用；批量拖入仍明确提示先用单条或 `red-decode --out-dir`。
3. **GPU / 并行 R3D 解码**：目前 CPU；6K 全分辨率约 14 s/帧（含 PNG），成片性能还需优化。
4. **印片机控制**：Y/M 二向色滤色片步进、预闪、光源选择；扫描仪 unsharp。
5. **空间物理模型**：染料云颗粒、逐通道 halation/scatter、具名扩散、镜头/扫描仪 blur。
6. **光谱 GPU 的 OCIO**、**PIZ / DWA**、**代理清理 / 4K 预览**、**macOS Qt 打包**。
7. **`--exr-linear` 的 OCIO 显示变换**：现在只认 sRGB/PQ/HLG。

### 新对话第一步

1. `ctest --test-dir build --output-on-failure`（11/11）
2. `scripts\verify-m1.cmd -NoOpen`（应仍是 **88.07 dB**）
3. `scripts\demo-slide.cmd -NoOpen`、`scripts\demo-cine.cmd -NoOpen`（D067/D065 不回归）
4. `scripts\demo-red.cmd -NoOpen`（5 步；需要 `build-red\`、SDK 和 ZR 样片）
5. `build-ui\spektra-roll-ui.exe --red-selftest` / `--red-render-selftest`
6. `build-red\spektra-roll-cli.exe video --in <R3D> --lut ... --frames 4 --segment 0.1 --red-scale half --jobs 2`（分段/续渲）
7. Mac：`scripts\verify-macos.command` 与 `scripts\bench-macos.command`

### 当前环境 / 已知限制

- 本机已装 **VS Build Tools 2022**（`X:\agent\tools\vsbuild`），用于本地编 `spektra-red.dll`；MinGW 不能直链 RED SDK。
- RED SDK 在 `X:\agent\footage\R3DSDKv9_2_1`；R3D 样片在 `X:\agent\footage\ZR_RED_Color_Science_Sunflower.R3D`。两者都**不进仓库**。
- 官方 `build\` / CI 永远只有 loader，不带 RED；只有 `build-red\` + 用户自己的 SDK 能解码。
- `build-red\` 含 RED Redistributable DLL 和带 SDK 的插件，**只限本地自用**，不要当官方包分发。
- `build-red.ps1` 会复制当前 `build\spektra-roll-cli.exe` 到 `build-red\`；改完核心代码后要重新跑一次 `scripts\build-red.cmd`。
- R3D 音频：插件按 SDK 的 `AudioBlockCountAndSize` + `DecodeAudioBlock` 写 AU，`video` 再封装；**没有有声 R3D 样片，真实音频未测**。
- UI 预览 R3D 走 `red-decode` 单帧（约 4–8 s/帧），拖动只在松手后更新；不是常驻预览缓存。
- half 3024×1701 高度是奇数：选 R3D 自动切 ProRes；手动改 H.264/HEVC 会被 CLI 拒绝。
- R3D 解码是 CPU；`full` 6K 很慢，预览/批量先用 `half`/`quarter`。
- 旧 D068 文档里的 `SR_WITH_REDSDK` 已不存在；当前构建不需要任何 CMake RED 开关。

---

## 零之一、上一轮（D063–D066）交接摘要

> 上一轮（D063–D066）的摘要；新对话优先读上面的 D067–D071。

### 这次完成了什么

| 编号 | 交付 | 关键结果 |
|---|---|---|
| **D063** | UI 的 OCIO 输入 | 左栏“色彩管理（OCIO）”：配置 + 色彩空间下拉（名字来自 `ocio-info --json`）；烘焙 / 光谱预览 / 渲染 / “按住看原片”都走 `--ocio-input`；UI 烘的 LUT 与 CLI **逐位相同（inf）**；顺手修掉 `preview` 的 spectral-gpu 静默忽略 OCIO（21.34 dB） |
| **D064** | 测试素材换成 FX30 S-Log3 | 统一 `X:\agent\footage\C0113.MP4`（4K50、10-bit、全范围）；UI 默认 + 9 个 demo 改 `s-log3` / `--in-matrix bt709` / `in_range=auto`；OCIO 演示改 `S-Log3 S-Gamut3.Cine`；旧素材从未进 git |
| **D065** | 电影片库 | Vision3 50D/250D/200T/500T + Verita + 2383/2393 印片；T 参考光 / K75P 观看光 / CAT02；CPU + D3D11 + Metal 同步；与上游 **88.85 dB（max 4.48e-4）**；界面可用 profile **17 → 24** |
| **D066** | 状态栏回退提示 | OCIO + 光谱 GPU 只打一条明确 warning（不再有 `warning: ;`）；UI 显示“提示：”而不是像报错；队列开始先说明会回退 CPU |

- CI：D063 的 run **102**、D064/D065 的 run **104**、D066 的 run **107**，Windows + macOS 全绿。
- 本机：`ctest` **10/10**；`verify-m1` **88.07 dB**；`--stocks-selftest` **16 films / 8 papers**。
- 最新代码提交：`a869e14`（D066）；之后只有文档更新（DECISIONS / STATUS / 本文件）。

### 关键决策

| 编号 | 决策 | 结论 |
|---|---|---|
| D063 | UI 的 OCIO | UI 不链库；CLI 提供名字（`--json`）与解码；LUT 路径烘进表里；`raw` 也走 OCIO；GPU 光谱明确回退 CPU |
| D064 | 测试素材 | 统一 FX30 S-Log3 `C0113.MP4`；无标签容器矩阵显式 `bt709`；解码 `in_range=auto`；视频文件不进仓库 |
| D065 | 电影片库 | 第二层乳剂可以是相纸或印片；T/K75P 进模型；D50 保持严格单位矩阵；GPU 常量缓冲同步 |
| D066 | 回退提示 | OCIO + 光谱 GPU 是“提示”不是失败；CLI 只打一条；UI 映射成中文提示 |

### 未完成的待办（按建议顺序）

1. [x] ~~**反转片直扫**：Ektachrome / Velvia / Provia / Kodachrome~~ → D067 已完成（`--scan slide`）
2. **光谱 GPU 的 OCIO**：现在 `--ocio-input` 会回退 CPU 光谱（4K 约 3.5 s/帧）
3. **印片机控制**：Y/M 二向色滤色片步进、预闪、光源选择；扫描仪白/黑校正 + unsharp
4. **空间物理模型**：染料云颗粒、逐通道 halation/scatter、具名扩散滤镜、镜头/扫描仪 blur
5. **PIZ / DWA 压缩**、**代理清理 / 4K 预览**、**macOS Qt 打包**
6. **`--exr-linear` 的 OCIO 显示变换**：现在只认 sRGB/PQ/HLG

> 我的建议顺序：反转片直扫已在 D067 完成；下一步做**空间物理**（染料云颗粒 + 逐通道光晕，2–3 个 D），然后是**印片机/扫描仪控制**；光谱 GPU 的 OCIO 可以等。

### 新对话第一步

1. `ctest --test-dir build --output-on-failure`（10/10；`ocio` 在没有 OCIO 的机器上 skip）
2. `scripts\verify-m1.cmd -NoOpen`（应仍是 **88.07 dB**）
3. `scripts\demo-cine.cmd -NoOpen`（Vision3 500T + 2383 / 负片 / Portra 400 三张）
4. `scripts\demo-ocio-ui.cmd -NoOpen`（UI 自检 52 个空间；UI LUT vs CLI **inf**）
5. `scripts\demo-ocio.cmd -NoOpen`（S-Log3 双实现 61.4 dB；ARRI LogC3 LUT 63.9 dB）
6. `scripts\demo-log-inputs.cmd -NoOpen`（9 种输入；GPU/CPU 118–120 dB）
7. `scripts\demo-exr.cmd -NoOpen`（EXR 回归；读回 PSNR = inf）
8. Mac：`scripts\verify-macos.command` 与 `scripts\bench-macos.command`

### 当前已知限制 / 坑

- **OCIO + 光谱 GPU 会回退 CPU**（D062/D066）；UI 会显示“提示：OCIO 输入只能用 CPU 光谱…”，不是失败。想快就用 LUT 引擎或关掉 OCIO。
- 4K50 素材的预览会走 **960 px 代理**；光谱 GPU 在 4K 上约 **765 ms/帧**（含上传/回读 + GPU 质感），LUT 路径快得多。
- FX30 的 XAVC **没有颜色标签**：CLI 的 `--in-matrix` 要显式 `bt709`；UI 已按输入类型自动用 bt709。
- UI 的 OCIO 能力取决于它调用的 CLI：完整构建 `build\spektra-roll-cli.exe` 有；只有 `build-ui\` 的包没有，可用 `SPEKTRA_CLI` 覆盖。
- 仓库里 **没有视频文件**；素材在 `X:\agent\footage\`（git 外）。

---

## 零之二、更早的阶段摘要：D056–D062（Metal → 浮点 → 相机 Log → OCIO）

> 这里是上一次交接的摘要，保留给需要回溯 D056–D062 的对话。

### 已完成（按交付）

| 编号 | 内容 | 关键结果 |
|---|---|---|
| **D056** | macOS Metal 光谱 | 2.6 MP 单帧 **424.3 → 129.0 ms（3.3×）**；M1 上 `MTLCreateSystemDefaultDevice` 返回 nil，`MTLCopyAllDevices` 兜底；新增 `gpu-info` 诊断 |
| **D057** | macOS Metal 质感 | 14 个 pass / 一个 command buffer / `setBytes` 内联；真机九组 **逐位相同**，融合 **134.80 dB**；质感本身约 125 → 48 ms |
| **D058** | 线性 EXR | `--exr-linear [--exr-trc srgb\|pq\|hlg] [--peak]`；1.0 = 白/峰值；同帧 display 0.016..0.887 → linear 0.0012..0.762 |
| **D059** | EXR ZIP | 默认 ZIP、`--exr-raw` 关闭；已调色帧 −28%、half −38%、带颗粒 ≈0%；ffmpeg 交叉验证 92.6 dB |
| **D060** | 多相机 Log | S-Log3 / C-Log3 / V-Log；曲线 101 点 vs colour-science **完全一致**；CPU vs D3D11 **110.67 dB**；UI 下拉可选 |
| **D061** | 更多相机 Log | LogC3 / LogC4 / BMD Gen5 / DaVinci / S-Gamut3；101 点曲线 **0 差**；CPU vs D3D11 **118.06–119.97 dB**；`--in-matrix auto` 按素材标签解码 |
| **D062** | OpenColorIO | 可选依赖（`SR_WITH_OCIO`）；MinGW 自建静态 OCIO 2.4.2；`ocio-info` / `lut bake --ocio-input` / `convert --ocio-*`；OCIO sRGB vs 内置 **118.24 dB**、Apple Log **93.60 dB** |

- CI：D057–D060 的最终提交（run **85–91**）Windows + macOS 全绿；macOS 上 `gpu_film` / `gpu_texture` / `gpu_chain` 都是真跑，不是 skip（run 84 是 D057 开发中的一次编译失败）。
- CI：本轮 D061 的 run **93 / 95**、D062 的 run **97** Windows + macOS 全绿；macOS 从 run 97 起用 Homebrew 的 OpenColorIO 真跑 `test_ocio`，Windows 跑 stub 路径。
- 真机：MacBook Air M1（macOS 14.7.5，arm64）三轮截图验收（verify ×2 + bench）；Windows 本机 **10/10** 测试。

### 关键决策（D056–D062）

| 编号 | 决策 | 结论 |
|---|---|---|
| D056 | macOS GPU 颜色 | Metal 移植光谱链；**fast math 必须关**（否则 NaN 保护被优化掉）；默认设备 nil 时用设备列表兜底 |
| D057 | macOS GPU 质感 | 14 pass 一个 command buffer；参数/PSF taps 用 `setBytes` 内联；融合路径借用 film 的 `MTLBuffer` |
| D058 | 线性浮点交付 | 可选 `--exr-linear`；1.0 = 白/峰值；sRGB 解码不钳制；不改默认 |
| D059 | EXR 压缩 | 默认 ZIP、可 `--exr-raw`；压不动逐块回退；只用 stb zlib，无新依赖 |
| D060 | 多相机 Log | 官方曲线 + 原生 gamut 矩阵；CPU/GPU 同一套 `inputMode`；手动选输入 |
| D061 | 更多 Log + 容器矩阵 | 追加 5 种输入（枚举只能往末尾加）；未知 `inputMode` 回退 sRGB；`--in-matrix auto` 读素材 `color_space` |
| D062 | OpenColorIO | 可选依赖 + stub；胶片模型只吃线性 Rec.709；解码烘进 LUT；GPU 回退 CPU |

> 本次对话（D063–D066）的完成、决策和待办见前面的「零之一」；下面是更早阶段的记录。

---

## 会话 D071 · R3D 收尾：音频 + UI 直连 + segment

会话目标：D070 已经能 `video --in clip.R3D`；产品负责人选 A，补齐音频、UI 和 segment。

### 一、已完成的事项

**音频**
- `red_plugin_api.h` ABI 升到 2；增加 `sr_red_has_audio` / `sr_red_write_audio`。
- 插件用 `AudioBlockCountAndSize` + `DecodeAudioBlock` 写 **32-bit PCM Sun AU**（对齐 512 字节），和 SDK `ExtractAudioSample.cpp` 一致。
- `RedReader::hasAudio/writeAudio`；`video` 渲染前写临时 `.au`，最后 ffmpeg 封装：ProRes → PCM 24-bit，H.264/HEVC → AAC 192k；`--start/--frames` 同步裁剪。
- 样片 0 音频通道，所以无音频路径和合成 AU 封装已验证；真实 R3D 音频未测。

**segment / resume / jobs**
- 移除 R3D 的 segment 限制；`renderRange` 每段新建 `RedReader`；`finishRedVideo` 用 concat + AU 一次性封装。
- `--resume` 签名包含 red/scale/format；`--jobs` 的 worker 各自打开 clip。

**UI 直连**
- 左栏“R3D 解码：full/half/quarter”；`probeInput` 用 ffprobe 元数据 + `red-info` 判断插件。
- LUT：R3D 强制 `--ocio-input "Log3G10 REDWideGamutRGB"`，`lookSignature` 带 `r3d:<scale>`。
- 光谱：`lookArgs` 对 R3D 不传 `--input/--ocio-*`。
- 预览：`previewFromServer` 对 R3D 返回 false；`previewRedLegacy` 用 `red-decode` + `lut apply`/光谱；拖动只在松手后渲染。
- 渲染：`buildVideoArgs` 加 `--red-scale`；选 R3D 自动切 ProRes；单条渲染/入队可用。
- 新增 `--red-selftest`、`--red-render-selftest`；完成弹窗在 selftest 里抑制。

### 二、一条关键决策（详见 DECISIONS D071）

| 编号 | 决策 | 结论 |
|---|---|---|
| D071 | R3D 收尾 | AU + ffmpeg 封装；每段独立 RedReader；UI 强制 Log3G10/RWG LUT + red-decode 预览；R3D 自动 ProRes |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| `--red-selftest` | `ok: 预览：3024 × 1701 @ 1.92 s · 7993 ms` |
| `--red-render-selftest` | `ok: 渲染完成`，3024×1701 / 2 帧 ProRes 422 HQ |
| segment/jobs | 4 帧 / 2 段 / 2 workers；删第 2 段后 `--resume` 只补缺失段 |
| 音频封装 | 合成 AU + ProRes → PCM 24-bit 成功；真实 R3D 音频未测（样片 0 通道） |
| `ctest` | 11/11；`verify-m1` 88.07 dB |

### 四、怎么用

```powershell
# CLI：R3D 直读 + 分段
.\build-red\spektra-roll-cli.exe video --lut out\red-vision3-2383.cube `
  --in "X:\agent\footage\ZR_RED_Color_Science_Sunflower.R3D" `
  --out out\red-clip.mov --frames 4 --segment 0.1 --red-scale half --jobs 2

# UI 自检
.\build-ui\spektra-roll-ui.exe --red-selftest
.\build-ui\spektra-roll-ui.exe --red-render-selftest
```

UI：选 `.r3d` → 左栏“R3D 解码”选 half → 预览/渲染；R3D 会自动切 ProRes，LUT 用 Log3G10/RWG 烘。

### 五、未完成的待办

- [ ] 真实 R3D 音频样片验证
- [ ] UI 批量 R3D 入队
- [ ] GPU/并行解码、6K 性能
- [ ] 印片机/扫描仪控制、空间物理模型、光谱 GPU 的 OCIO

---

## 会话 D070 · R3D NE 直读视频（`video --in clip.R3D`）

会话目标：D069 已经能 `red-decode` + `lut apply`；这一轮让普通 `video` 命令直接吃 `.R3D`。

### 一、已完成的事项

**D070 · 直读视频**
- `VideoRender` 增加 `red/redScale/redFormat`；`renderRange` 在 R3D 模式下改用 `sr::RedReader` 取帧，不再开 ffmpeg 解码管道。
- LUT 引擎：R3D → **Log3G10/REDWideGamutRGB** → LUT；光谱引擎：R3D → **sRGB Rec.709** → `FilmModel`。
- 元数据：帧数/帧率/timecode 来自插件；`--start` 正确偏移输出 timecode；`--frames` 超长会截断并 warning。
- 默认编码 **ProRes 422 HQ**（half 3024×1701 高度奇数；H.264/HEVC 4:2:0 不接受）。
- `--red-scale full|half|quarter`。
- R3D 分支暂时拒绝 `--segment/--resume`；输出视频-only（不封装音频）。
- `demo-red.cmd` 增加 5/5：直接 `video --in clip.R3D` 出 3 帧 `out\red-clip.mov`。
- UI 仍不直连 `.r3d`，但提示改成“CLI 已可直接 video --in clip.R3D”。

### 二、一条关键决策（详见 DECISIONS D070）

| 编号 | 决策 | 结论 |
|---|---|---|
| D070 | R3D 直读视频 | `VideoRender.red` + `RedReader` 帧源；LUT=Log3G10/RWG，光谱=sRGB；默认 ProRes；视频-only、无 segment |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| LUT，half，3 帧 | 3024×1701 ProRes 422 HQ，**2.0 s/帧**（CPU R3D 解码 + LUT + 编码） |
| 光谱 GPU，half，2 帧 | Vision3 250D + 2383，**2.29 s/帧**，输出正常 |
| LUT，quarter，1 帧 | 1512×850，**0.69 s/帧** |
| `--start 1 --frames 1` | timecode `11:09:14:03 → 11:09:15:03` |
| `demo-red.cmd` | 4 张 PNG + 3 帧 `red-clip.mov` 正常 |
| `ctest` | **11/11**；`verify-m1` 88.07 dB |

### 四、怎么用

```powershell
# LUT 路径（LUT 必须是 Log3G10/RWG 烘的）
.\build-red\spektra-roll-cli.exe video --lut out\red-vision3-2383.cube `
  --in "X:\agent\footage\ZR_RED_Color_Science_Sunflower.R3D" `
  --out out\red-clip.mov --frames 12 --red-scale half --codec prores

# 光谱 GPU 路径（R3D 解 sRGB；不要传 --input/--ocio-*）
.\build-red\spektra-roll-cli.exe video --engine spectral-gpu `
  --film data\profiles\kodak_vision3_250d.json --paper data\profiles\kodak_2383.json `
  --scan print --exposure 0.7 --in "...\ZR_RED_Color_Science_Sunflower.R3D" `
  --out out\red-spec.mov --frames 3 --red-scale half --codec prores

# 不点鼠标
scripts\demo-red.cmd
```

### 五、未完成的待办

- [ ] R3D 音频封装（插件音频 API + ffmpeg）；样片 0 音频
- [ ] R3D 的 `--segment/--resume/--jobs`
- [ ] UI 预览/渲染直连 `.R3D`
- [ ] GPU/并行解码

---

## 会话 D069 · RED R3D NE 帧解码（MSVC 插件）

会话目标：用户提供了 Nikon ZR 的 R3D NE 样片和 RED R3D SDK 9.2.1，要求继续把 D068 骨架变成真正可解码。

### 一、关键发现

- Windows SDK 9.2.1 只有 **MSVC 静态库**（`R3DSDK-2017MD.lib` 等），没有 MinGW 可用的 import lib；主程序是 MinGW，C++ ABI 不兼容。
- 直接链进 GPL 二进制也不合适（闭源 SDK + GPL 发布）。
- 结论：**核心 loader + 本地 MSVC 插件（C ABI）**。插件是唯一包含 `R3DSDK.h` 的文件；官方 `build\` 不含 RED 代码。

### 二、已完成的事项

**D069 · R3D 插件 + 解码**
- `src/red-plugin/red_plugin_api.h`：纯 C ABI（open/decode/close/info）。
- `src/red-plugin/red_plugin_msvc.cpp`：MSVC 编译；`InitializeSdk(..., OPTION_RED_NONE)`；`Clip::DecodeVideoFrame`；16-bit interleaved RGB。
- `src/core/src/red_plugin.cpp`：MinGW loader；`LoadLibrary`/`dlopen`；找 `spektra-red.dll`（或 `SPEKTRA_RED_PLUGIN`）；把 16-bit 转 float。
- `sr::RedReader`：一次 open、多帧 decode、复用 SDK 缓冲。
- CLI `red-decode`：单帧 `--out` / 序列 `--out-dir`；`--format log3g10|linear|srgb`；`--scale full|half|quarter`；PNG/EXR。
- 默认 `Primary_Development_Only` → **REDWideGamutRGB + Log3G10**（`lut bake --ocio-input "Log3G10 REDWideGamutRGB"` 的输入域）；`Full_Graded` → Linear Rec.709 / sRGB。
- `scripts/build-red.ps1` 重写：用 MSVC 编插件、复制普通 CLI + SDK Redistributable DLL 到 `build-red\`；`-Check` 检查 header/lib/runtime/MSVC。
- `scripts/demo-red.ps1` / `.cmd`：真样片 → sRGB 原图 + Log3G10 中间帧 → Vision3 250D + 2383 → 成片。
- `red-info` 现在报告插件路径/SDK 版本/R3D 元数据；`video`/`preview` 仍拒绝 `.r3d`，但指向 `red-decode`。

### 三、一条关键决策（详见 DECISIONS D069）

| 编号 | 决策 | 结论 |
|---|---|---|
| D069 | R3D 解码架构 | MSVC-only SDK → 内核 loader + 插件 C ABI；输出 Log3G10/RWG；官方包不带 SDK |

### 四、实测 / 验证

| 项 | 结果 |
|---|---|
| SDK / 样片 | RED R3D SDK **9.2.1**；Nikon ZR R3D NE 6048×3402 / 249 帧 / 23.976 / TC 11:09:14:03 / 0 音频通道 |
| `red-info --in` | 元数据正确；插件 `available=true` |
| half 解码 | 3024×1701；约 **3.8 s/帧**（含 SDK 初始化 + 30 MB PNG） |
| full 6K 解码 | 6048×3402；约 **14 s/帧**（含 100 MB PNG） |
| sRGB / Log3G10 | 两种输出都正常；Log3G10 经 OCIO 显示变换颜色正常 |
| 胶片 | `lut bake --ocio-input "Log3G10 REDWideGamutRGB"` + Vision3 250D + 2383 → 成片正常 |
| 序列 | `red-decode --out-dir` 3 帧 + `lut apply --in-dir` 正常；`red-to-prores` 出 3 帧 ProRes 422 HQ（3024×1701，无音频） |
| `ctest` | **11/11**（官方构建无插件，`test_red` 走 stub 分支） |
| 回归 | `verify-m1` 88.07 dB；`stocks selftest` 正常 |

### 五、怎么用

```powershell
$env:SR_REDSDK_ROOT = "X:\agent\footage\R3DSDKv9_2_1"
.\scripts\build-red.cmd -Check
.\scripts\build-red.cmd

.\build-red\spektra-roll-cli.exe red-info --in "X:\agent\footage\ZR_RED_Color_Science_Sunflower.R3D" --json
.\build-red\spektra-roll-cli.exe red-decode --in "X:\agent\footage\ZR_RED_Color_Science_Sunflower.R3D" `
  --frame 0 --scale half --format log3g10 --out out\red-log3g10.png
.\build-red\spektra-roll-cli.exe lut bake --ocio-config data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio `
  --ocio-input "Log3G10 REDWideGamutRGB" --film data\profiles\kodak_vision3_250d.json `
  --paper data\profiles\kodak_2383.json --scan print --exposure 0.7 --out out\red-vision3-2383.cube
.\build-red\spektra-roll-cli.exe lut apply --lut out\red-vision3-2383.cube `
  --in out\red-log3g10.png --out out\red-graded.png
```

双击：`scripts\demo-red.cmd`。

### 六、未完成的待办

- [ ] `video --in clip.R3D` / UI 预览/渲染直连（帧源抽象、音频、timecode）
- [ ] GPU 解码（CUDA/OpenCL/Metal）、并行解码
- [ ] 印片机/扫描仪控制、空间物理模型、光谱 GPU 的 OCIO

---

## 会话 D068 · RED R3D / R3D NE 的 BYO-SDK 骨架

会话目标：产品负责人问能否支持 Nikon ZR 的 **R3D NE**，并选择 **A：先做 BYO SDK 骨架**。R3D 是私有 RAW，`ffmpeg` 主线不能解；RED SDK 闭源、EULA 不允许分发头文件/静态库，和 GPL 官方发布冲突。

### 一、已完成的事项

**D068 · 可选 RED SDK 骨架**
- `sr/red.hpp` 纯接口；`red_stub.cpp` 默认构建；`red_sdk.cpp` 只在 `-DSR_WITH_REDSDK=ON` 时编译。
- CMake：`SR_WITH_REDSDK` 默认 **OFF**；ON 时要求 `SR_REDSDK_ROOT`，找不到 `R3DSDK.h` / 库就配置阶段报错并指向 `docs/red-sdk.md`。
- CLI：`red-info [--in <clip.r3d>] [--json]` 报告 built/available、SDK 版本、运行时库目录；带 SDK 时读尺寸/帧数/帧率/音频/timecode。
- `.r3d` 识别（大小写不敏感）在**所有构建**都有效；`video` / `preview open` 遇到 `.r3d` 给 BYO SDK / REDCINE-X 指引，不再掉进 ffprobe。
- UI：文件选择/拖放接受 `.r3d`；`probeInput` 调 `red-info --json`，没有 SDK 显示指引，有 SDK 显示元数据 + “解码待接入（D068）”；`requireInput` 拦住单条预览/渲染，`enqueueBatch` 拦住批量。
- `scripts\build-red.ps1` / `.cmd`：`-Check` 只查 header/library/runtime；构建到独立 `build-red\`，不碰官方 `build\`。
- `docs/red-sdk.md`：BYO SDK 步骤、`SPEKTRA_RED_SDK`、许可证边界、REDCINE-X ProRes 4444 现行路线。
- `test_red`：默认构建断言 `.r3d` 识别 + stub 行为；CI 增加 `red-info` 必须 `built:false`，以及 `video --in sample.r3d` 必须返回 RED 指引。

### 二、一条关键决策（详见 DECISIONS D068）

| 编号 | 决策 | 结论 |
|---|---|---|
| D068 | RED R3D | BYO SDK；官方包/CI 永远 stub；D068 只读元数据；渲染先用 REDCINE-X ProRes 4444（RWG/Log3G10）+ OCIO LUT |

### 三、验证

| 项 | 结果 |
|---|---|
| `ctest` | **11/11**（新增 `test_red`） |
| `red-info`（默认构建） | `not built`，message 指向 `docs/red-sdk.md` |
| `red-info --in sample.r3d --json` | `clipError` 明确说明没有 SDK；不会崩 |
| `video --in sample.r3d` | 返回 RED 指引，退出非零；不会去跑 ffprobe |
| `CMake -DSR_WITH_REDSDK=ON` 无 SDK | 配置阶段报错，提示 `SR_REDSDK_ROOT` / docs |
| `red_sdk.cpp` 语法检查 | 用最小假 `R3DSDK.h` 通过 `-Wall -Wextra -Wpedantic`；真实 SDK 仍需用户机器验证 |
| 假 SDK 全链路 | 用假 `libR3DSDK.a` 走通 `-DSR_WITH_REDSDK=ON` 配置 + 编译 + `red-info --in`（4096×2160 / 120 帧 / 25 fps） |
| 回归 | `verify-m1` **88.07 dB**；UI `stocks selftest` 正常 |

### 四、怎么用

```powershell
# 官方构建：没有 RED，但会识别 .r3d 并给出下一步
.\build\spektra-roll-cli.exe red-info
.\build\spektra-roll-cli.exe red-info --in D:\footage\A001.R3D --json

# 你自己的 SDK（本地自用，官方包不带）
$env:SR_REDSDK_ROOT = "D:\RED_SDK"
.\scripts\build-red.cmd -Check
.\scripts\build-red.cmd
.\build-red\spektra-roll-cli.exe red-info --in D:\footage\A001.R3D --json
```

界面：选择/拖入 `.r3d` 后，素材信息条会显示 SDK 状态；没有 SDK 会提示用 REDCINE-X 导出 ProRes 4444（RWG/Log3G10）。

### 五、未完成的待办

- [ ] **R3D 帧解码**：`red_sdk.cpp` 接解码；`video` 帧源抽象（ffmpeg / RED）；音频、timecode、预览
- [ ] 印片机 Y/M 微调、预闪、光源选择；扫描仪 unsharp
- [ ] 空间物理模型、光谱 GPU 的 OCIO、PIZ/DWA、代理清理 / 4K 预览、macOS Qt 打包

---

## 会话 D067 · 反转片直扫（Ektachrome / Velvia / Provia / Kodachrome）

会话目标：D065 之后片库里只剩 4 支反转片没接。反转片显影后本身就是正像，不能印到负片相纸，也不该走“负片倒相”；这一轮把“正片直扫”路径接通。

### 一、已完成的事项

**D067 · 正片直扫**
- `ScanMode` 追加 `Positive`；CLI 新增 `--scan slide`（也接受 `positive` / `reversal`）。片库里的 `type=positive` 胶片在不写 `--scan` 时自动直扫。
- 负片扫描会去掉片基（橙 mask 随之消失）；反转片直扫**保留片基**，扫描后再按“清澈片基 → 最深染料”做黑白平衡，并用 18% 中灰按通道 power 中和染料串色。
- `--contrast` 仍以中灰为支点；反转片默认 **1.0**（负片仍 1.15，印片仍 1.0）。
- 错误组合明确报错：正片 + `--scan print` / `--scan negative`，负片 + `--scan slide`。
- CPU / D3D11 / Metal 同步：常量缓冲新增 `positiveWhite/Black/Gamma` 与 `positiveGreyLuma`，`scanNegative` 改成三态 `scanMode`；两个着色器都实现“带片基扫描”。
- UI：片库纳入 4 支反转片（下拉显示“（反转片）”）；选反转片自动切“扫描反转片（直扫）”、停用相纸；切回负片恢复。`--stocks-selftest` 增加 `slide`。
- 新增 `scripts\demo-slide.cmd` / `.ps1`：同一帧出四张 4K 反转片。

### 二、一条关键决策（详见 DECISIONS D067）

| 编号 | 决策 | 结论 |
|---|---|---|
| D067 | 反转片直扫 | `ScanMode::Positive`；保留片基；扫描仪黑白平衡 + 中灰中性；正片默认直扫；CPU/GPU 三态 `scanMode` |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| 4K50 FX30 单帧（Ektachrome） | 光谱 CPU **2685.6 ms** vs 光谱 GPU **261.3 ms** |
| CPU vs D3D11 | ffmpeg PSNR **119.09 dB**（r 118.33 / g 119.29 / b 119.78） |
| `test_gpu_film` | 新增 Ektachrome + slide + couplers + gamut，worst **6e-6** |
| `test_film_model` | 黑白端点、中灰中性、单调性、0.5 / 1.0 锚点；另外三支反转片都能配置并到白 |
| 角色反转错误 | 正片 + print/negative、负片 + slide 都返回明确错误 |
| 界面片库 | `stocks selftest: ok films=20 papers=8 vision3=1 verita=1 print=1/1 portra=1 slide=1` |
| `demo-slide` | 4 支反转片烘表 33³ 约 100–110 ms，4K 输出正常 |
| 回归 | `ctest` **10/10**；`verify-m1` **88.07 dB**（旧路径数值未变） |

### 四、怎么用

```powershell
# 单支反转片（不写 --scan 也会自动直扫）
.\build\spektra-roll-cli.exe lut bake --film data\profiles\kodak_ektachrome_100.json `
  --input s-log3 --scan slide --exposure 0.7 --out out\ektachrome.cube

# 视频
.\build\spektra-roll-cli.exe video --engine spectral-gpu --film data\profiles\fujifilm_velvia_100.json `
  --scan slide --input s-log3 --in-matrix bt709 --in X:\agent\footage\C0113.MP4 `
  --out out\slide-velvia.mov --duration 2 --codec prores

# 双击验收
scripts\demo-slide.cmd
```

界面：左栏“胶片”选带“（反转片）”的项，扫描方式会自动切到“扫描反转片（直扫）”，相纸下拉停用。

### 五、未完成的待办

- [ ] 印片机 Y/M 微调、预闪、光源选择；扫描仪 unsharp
- [ ] 空间物理模型（染料云颗粒、逐通道 halation、具名扩散、镜头/扫描仪 blur）
- [ ] 光谱 GPU 的 OCIO、PIZ/DWA、代理清理 / 4K 预览、macOS Qt 打包

---

## 会话 D066 · 状态栏的“warning: ;”

会话目标：产品负责人截图，UI 渲染队列（偶尔预览一帧）状态栏出现 `warning: ; falling back to the CPU spectral engine`，看起来像报错。

### 一、原因

- 选 **OCIO 输入 + 光谱 GPU** 时，CLI 先打 `warning: OpenColorIO input is CPU-only ...`，把 `gpu` 置 false；通用回退分支又用**空的 `error`** 打了一条 `warning: %s; ...`。
- UI 的队列/渲染输出处理把**每一行 stderr** 都写进状态栏，所以最后留下的是那条空 warning。
- 任务本身没有失败：它按设计退回 CPU 光谱（D062 的限制），只是慢。

### 二、修复

- CLI：OCIO 分支不再触发第二条警告；`error` 为空时用明确文字。
- UI：`warning:` → “提示：”；OCIO 那句翻译成“OCIO 输入只能用 CPU 光谱，已自动回退（渲染会变慢）”；日志保留英文原文。
- 队列/渲染开始：参数里同时有 `--ocio-input` 和 `--engine spectral-gpu` 时，先写一条中文提示。
- CI：macOS 的 OCIO 步骤加断言，输出里不能再出现 `warning: ;`。

### 三、验证

| 项 | 结果 |
|---|---|
| `lut apply --engine spectral-gpu --ocio-*` | 只剩一条明确的 OCIO 回退 warning |
| `ctest` | 10/10 |
| UI 构建 / `--stocks-selftest` | 正常（16 films / 8 papers） |
| CI | run **107** Windows + macOS 全绿（macOS 断言不再出现 `warning: ;`） |
| 实际速度 | OCIO + CPU 光谱在 4K 上约 **3.5 s/帧**；想快就用 LUT 引擎或关掉 OCIO |

---

## 会话 D064–D065 · 测试素材换成 FX30 S-Log3 + 电影片库

会话目标：产品负责人选 **A（电影片库）**，并要求之后的测试素材统一用 `X:\agent\footage\C0113.MP4`（Sony FX30，S-Log3），旧的 iPhone 素材不再使用。

### 一、已完成的事项

**D064 · 测试素材**
- 新素材：3840×2160、50p、10-bit 4:2:2、6.24 s / 312 帧；**没有 color_space / transfer / primaries 标签，但 `color_range` 标成全范围（pc）**；有 PCM 48 kHz 立体声，相机 timecode **00:13:37:46** 在 rtmd 数据流里。
- UI：默认素材 → `C0113.MP4`，默认“画面类型” → `S-Log3（S-Gamut3.Cine）`；老安装里如果保存的是旧默认路径，载入时一次性迁移。
- 9 个 demo 脚本：`$Footage`、抽帧矩阵（bt2020 → **bt709**）、`--input/--from`（apple-log → **s-log3**）、产物名（`*-applelog*` → `*-slog3*`）全部同步；`video` 调用显式 `--in-matrix bt709`。
- `in_range` 从硬编码 `tv` 改成 **`auto`**（CLI 的 `video` / `preview` 和 UI 回退路径）：这个文件是全范围，硬编码 tv 会把 S-Log3 码值压暗约 1%；无标签素材仍等价 tv。
- OCIO 演示和 UI 自检改用 `S-Log3 S-Gamut3.Cine`。
- `demo-m4` 保留相机 timecode（00:13:37:46）和 PCM 音频，不再写死 `--timecode`。
- 仓库从未跟踪视频文件，所以没有二进制要删；素材仍在 `X:\agent\footage\`（git 外）。

**D065 · 电影片库**
- 第二层乳剂可以是相纸，也可以是 `support=film + stage=printing` 的电影印片（2383/2393）。
- 参考光支持 D55 / **T**；印片观看光支持 D50 / **K75P**；负片用胶片自己的 viewing、印片用第二层乳剂的。
- K75P → D50 用 CAT02；D50 保持严格单位矩阵，旧片库数值逐位不变。
- CPU / D3D11 / Metal 三个后端同步；常量缓冲新增印片光源、归一化和两套 3×3 矩阵（顺手补 16 字节对齐）。
- UI 片库：**16 支负片**（11 支静态 + 4 支 Vision3 + Verita）+ 6 相纸 + 2 电影印片；选 Vision3 自动选中 2383；印片下拉标注“（电影印片）”。

### 二、两条关键决策（详见 DECISIONS D064/D065）

| 编号 | 决策 | 结论 |
|---|---|---|
| D064 | 测试素材 | 统一 FX30 S-Log3；无标签容器矩阵显式 bt709；视频不进 git |
| D065 | 电影片库 | 第二层可以是印片；T/K75P 进模型；D50 单位矩阵；GPU 同步 |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| 片库覆盖率 | 界面可用 profile **17 → 24**（16 负片 + 6 相纸 + 2 印片）；只剩 4 支反转片 |
| Vision3 500T + 2383 vs 上游 reference | 17³ **88.85 dB**，max |diff| **4.48e-4** |
| 印片曝光因子 | **1.720784698861**（上游 1.720784698861） |
| `test_film_model` | 新增片库 stage、8 个锚点；250D + 2393 中灰中性 |
| `test_gpu_film` | Vision3 print + S-Log3 negative，worst **1e-6**；**10/10** |
| 其他片库 | 200T + 2393、250D + 2383、Verita + 2383 都能烘表 |
| 界面片库 | `--stocks-selftest`：**films=16 / papers=8**，Vision3 / Verita / 2383 / 2393 都在 |
| `demo-ocio`（新素材） | 内置 S-Log3 vs OCIO **61.44 dB**；ARRI LogC3 LUT 63.89 dB |
| `demo-ocio-ui` | UI 自检 52 个空间（S-Log3）；UI LUT vs CLI **inf** |
| `demo-cine` | Vision3 500T + 2383 / Vision3 负片 / Portra 400 三张正常出图 |
| `demo-m4`（新素材） | 100 帧 / 2 段，续渲只补缺失段；timecode **00:13:37:46**、音频 PCM 正常 |
| 范围 | `in_range=auto` 与 `in_range=pc` 逐位相同、与 `tv` 不同；无标签素材 auto == tv |
| 4K50 光谱 GPU（Vision3） | 4 帧 ProRes，**764.6 ms/帧**（含 4K 上传/回读 + GPU 质感） |
| CI | run **104** Windows + macOS 全绿（macOS 真跑 Metal + OCIO） |

### 四、怎么用

```powershell
# 电影负片 + 印片
.\build\spektra-roll-cli.exe lut bake --film data\profiles\kodak_vision3_500t.json `
  --paper data\profiles\kodak_2383.json --scan print --input s-log3 `
  --exposure 0.7 --out out\vision3-2383.cube

# 视频（无标签的 FX30 XAVC 记得 bt709）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input s-log3 `
  --film data\profiles\kodak_vision3_500t.json --paper data\profiles\kodak_2383.json `
  --scan print --exposure 0.7 --in-matrix bt709 `
  --in X:\agent\footage\C0113.MP4 --out out\cine.mov --duration 2 --codec prores
```

界面：左栏“胶片”选 Vision3 500T / 250D 等，“相纸”会自动切到 2383（标注“电影印片”）。

### 五、未完成的待办

- [x] ~~**反转片直扫**：Ektachrome / Velvia / Provia / Kodachrome~~ → D067
- [ ] 印片机 Y/M 微调、预闪、光源选择；扫描仪 unsharp
- [ ] 空间物理模型（染料云颗粒、逐通道 halation、具名扩散、镜头/扫描仪 blur）
- [ ] 光谱 GPU 的 OCIO、PIZ/DWA、代理清理 / 4K 预览、macOS Qt 打包

---

## 会话 D063 · UI 的 OpenColorIO 输入

会话目标：按 D062 待办第一条，把 CLI 已经能用的 OCIO 暴露到 Qt 界面里——不是给界面加第二套颜色管线，而是让界面驱动同一条 CLI。

### 一、已完成的事项

**D063 · 界面里的 OCIO**
- CLI 新增 `ocio-info --json`：`version / config / configFile / linearSpace / colorSpaces / displays`，配置名做转义；UI 不解析缩进文本。
- 左栏新增“色彩管理（OCIO）”：勾选 / 配置文件 / `读取` / 色彩空间下拉 / 一行状态（版本、空间数、实际线性空间）。勾选后“画面类型”变灰，两者互斥。
- 烘焙 LUT、光谱预览、渲染视频都改传 `--ocio-config/--ocio-input`，不再传 `--input`；LUT 路径仍把解码烘进表里。
- 预览协议：LUT 引擎的 `look` 也接受 OCIO，服务端在 `configureLook` 里为 `raw`（按住看原片）编一个独立解码器。
- UI 仍不链接 OCIO：`findCli()` 优先用完整构建 `build\spektra-roll-cli.exe`，`SPEKTRA_CLI` 可覆盖；只有 build-ui 的包会提示“这个构建没有 OpenColorIO”。
- 新增 `--ocio-selftest`：固定 look（Portra 400 / Endura、0.70 EV、negative、1.15、DIR、OkLCh、sRGB）烘一张 OCIO LUT，供脚本验收（D063 时是 Apple Log，D064 起改用 S-Log3 S-Gamut3.Cine）。
- 顺手修掉：`preview` 的 `spectral-gpu` 在 OCIO 下**静默忽略解码**（和 CPU 差 21.34 dB），现在与 `lut apply`/`video` 一样 warning + 回退 CPU。

### 二、一条关键决策（详见 DECISIONS D063）

| 编号 | 决策 | 结论 |
|---|---|---|
| D063 | UI 的 OCIO | UI 不链库；CLI 提供名字与解码；烘焙/光谱/渲染同一条 CLI；`raw` 也走 OCIO |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| UI 自检 | **52 个色彩空间**，线性空间 `Linear Rec.709 (sRGB)`，`out/ui-ocio-selftest.cube` 正常写出 |
| UI LUT vs CLI LUT（同参数） | **PSNR = inf，max |diff| = 0**（35937 点） |
| `raw` 的 OCIO 解码 vs `convert --ocio-input` | **49.58 dB**（8-bit + 缩放）；不带 OCIO 只有 **19.72 dB** |
| 同一个预览进程里关掉 OCIO | 之后的 `raw` 与“从没开过 OCIO”的基线 **逐位相同（哈希一致）** |
| 光谱预览 + OCIO | `ok look spectral`；1600×898 一帧 **537 ms**，`raw` **95.8 ms** |
| `spectral-gpu` + OCIO | `ok look spectral`（回退 CPU），一帧 **529.9 ms**；修复前 34 ms 但颜色错 21 dB |
| 回归 | `ctest` **10/10**；`demo-ocio` 的 93.6 / 63.9 dB 不变 |
| CI | run **102** Windows + macOS 全绿；macOS 真跑 `ocio-info --json` + OCIO LUT，Windows 跑 stub |
| 双击验收 | `scripts\demo-ocio-ui.cmd` |

### 四、怎么用

```powershell
# 不点鼠标的验收：界面读配置 + 烘 S-Log3 LUT + 和 CLI 逐位比较
scripts\demo-ocio-ui.cmd -NoOpen

# 直接跑界面自检
.\build-ui\spektra-roll-ui.exe --ocio-selftest

# 界面里：左栏“色彩管理（OCIO）” → 勾选 → 读取 → 选 S-Log3 S-Gamut3.Cine / ARRI LogC3 / ACEScg
```

界面用的 CLI 可以显式指定（便携包 / 多套构建时）：

```powershell
$env:SPEKTRA_CLI = "X:\agent\SpektraRoll\build\spektra-roll-cli.exe"
.\build-ui\spektra-roll-ui.exe
```

### 五、未完成的待办

- [ ] **光谱 GPU 的 OCIO**：现在明确回退 CPU 光谱（界面状态栏已写明）
- [ ] `--exr-linear` 不认 OCIO 的显示变换
- [ ] PIZ/DWA、代理清理 / 4K 预览、macOS Qt 打包

---

## 会话 D062 · OpenColorIO 接入

会话目标：产品负责人选 **A：引入 OpenColorIO**。先解决“能不能真的编出来”，再谈功能。

### 一、已完成的事项

**D062 · 可选的 OpenColorIO 依赖**
- `sr/ocio.hpp` 是纯 C++ 接口（pimpl），只有 `src/core/src/ocio.cpp` 包含 OCIO 头；没有 OCIO 时编 `ocio_stub.cpp`，API 一致、所有 `--ocio-*` 报同一句“这个构建没有 OpenColorIO”。
- CMake：`-DSR_WITH_OCIO=ON`（默认）→ 先 `find_package(OpenColorIO CONFIG)`，再找自建前缀（`$OCIO_ROOT` 或 `X:/agent/tools/ocio`）。
- Windows 自建：`tools\ocio\build-ocio.cmd` 用 MinGW 编 **OCIO 2.4.2**，产物是自包含前缀（`libOpenColorIO.a` + expat / Imath / minizip-ng / pystring / yaml-cpp / zlib）。两个 MinGW 补丁：`tools/ocio/mingw.patch`（`SystemMonitor_windows.cpp` 的窄 tstring）+ 关掉 `OCIO_USE_WINDOWS_UNICODE` + 给 yaml-cpp 0.7.0 补 `<cstdint>`。
- 契约：胶片模型永远吃**线性 Rec.709**；`--ocio-linear` 不填时自动找（5 个常见名字，再退化成“唯一 Linear+Rec.709”）。OCIO 解码后**负值钳 0**，与内置解码一致。
- CLI：`ocio-info`、`lut bake --ocio-config/--ocio-input/--ocio-linear`、`convert --ocio-input` / `--ocio-display --ocio-view`。**LUT 把解码烘进表里**，所以 `lut apply`/`video` 完全不需要 OCIO。
- GPU：`--engine spectral-gpu` 没有 OCIO，明确 warning 并回退 CPU 光谱；LUT 引擎下给 `--ocio-*` 直接报错并提示改用 `lut bake`。
- 分段签名带 OCIO 输入名。UI 构建 `-DSR_WITH_OCIO=OFF`（Qt 的 GCC 13.1 与 OCIO 的 GCC 16.2 静态库符号不兼容，且 UI 不直接调 OCIO）。
- 仓库内置 ACES 2.0 **studio** 配置（Apache-2.0，`data/ocio/`，74 KB），离线可用。

### 二、一条关键决策（详见 DECISIONS D062）

| 编号 | 决策 | 结论 |
|---|---|---|
| D062 | OpenColorIO | 可选依赖 + stub；胶片模型只吃线性 Rec.709；解码烘进 LUT；GPU 回退 CPU |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| 测试 | **10/10**（新增 `test_ocio`；无 OCIO 时 exit 77 skip） |
| OCIO `sRGB Encoded Rec.709 (sRGB)` vs 内置 `srgbDecode` | 2.6 MP **118.24 dB**（max 1.5e-5 = 16-bit 1 LSB） |
| OCIO `Apple Log` vs 内置 Apple Log | **93.60 dB**（两条独立实现互证） |
| OCIO `ARRI LogC3 (EI800)` LUT vs 内置 `--input logc3` | **63.89 dB**，max 1.1e-2（ACES 的 D60→D65 适应） |
| ACES 2.0 显示器变换 | `sRGB - Display` / `ACES 2.0 - SDR 100 nits (Rec.709)` 正常出图 |
| `ocio-info` | 自动选中 `Linear Rec.709 (sRGB)`；内置配置列出 **52 个颜色空间 / 8 个显示器** |
| CI | run **97** Windows + macOS 双绿；macOS 用 Homebrew 的 OCIO 真跑 `test_ocio`，Windows 跑 stub 并断言那句报错 |
| 回归 | M1 parity 88.07 dB、9 个原测试全过、UI 第二次预览 48 ms |

### 四、怎么用

```powershell
# 配置里有什么（先看名字）
.\build\spektra-roll-cli.exe ocio-info --ocio-config data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio

# 用 ACES 配置里的 ARRI LogC3 烘一张负片 LUT（之后 lut apply / video 不需要 OCIO）
.\build\spektra-roll-cli.exe lut bake --ocio-config data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio `
  --ocio-input "ARRI LogC3 (EI800)" --scan negative --out out\logc3-ocio.cube

# 只解码/看显示器变换
.\build\spektra-roll-cli.exe convert --ocio-config <cfg> --ocio-input "Apple Log" `
  --ocio-display "sRGB - Display" --ocio-view "ACES 2.0 - SDR 100 nits (Rec.709)" `
  --in frame.png --out frame-aces2.png

# 光谱视频（CPU）也能直接用 OCIO 输入
.\build\spektra-roll-cli.exe video --engine spectral --ocio-config <cfg> `
  --ocio-input "S-Log3 S-Gamut3.Cine" --scan negative --in clip.mov --out out\graded.mov
```

### 五、未完成的待办

- [x] ~~**UI 的下拉**：界面还没暴露 OCIO（构建也故意不链 OCIO）~~ → D063 已完成（左栏“色彩管理（OCIO）”，用 `ocio-info --json` 填名字）
- [ ] **光谱 GPU 的 OCIO**：现在回退 CPU 光谱
- [ ] `--exr-linear` 还不认 OCIO 的显示变换

> 现状：Windows CI 不装 OCIO（跑 stub 路径并断言那句报错），macOS CI 用 Homebrew 的 OCIO 真跑 `test_ocio`——两边都覆盖到了。

---

## 会话 D061 · 更多相机 Log + 容器矩阵

会话目标：按产品负责人选择，**一次补齐主流机型**——ARRI LogC3/LogC4、Blackmagic Film Gen 5、DaVinci Intermediate、Sony S-Gamut3（非 Cine）。

### 一、已完成的事项

**D061 · 五种新输入**
- `InputTransfer` 追加 `SLog3SGamut3 / ArriLogC3 / ArriLogC4 / BmdFilmGen5 / DaVinciIntermediate`；新值只往枚举末尾加，D060 的 `inputMode` 0–4 编号不变。
- 曲线全部来自 colour-science 0.4.7：LogC3（SUP 3.x / EI 800）、LogC4、Blackmagic Film Gen 5、DaVinci Intermediate；S-Gamut3 复用 S-Log3 曲线。
- 色域矩阵：ARRI Wide Gamut 3 / 4、Blackmagic Wide Gamut（CAT02 适应，因为它的白点数字比 D65 更精确）、DaVinci Wide Gamut、S-Gamut3。
- `--input` 解析改成表驱动，大小写 / 连字符 / 下划线都接受（`LogC3`、`arri_logc4`、`s_gamut3`），别名 `s-gamut3`、`bmd-film-gen5`、`blackmagic-film`、`davinci-intermediate`。
- HLSL 与 MSL 同步加 5 个 `inputMode` 分支；**未知 mode 回退 sRGB**（D060 的最后一段默认落到 V-Log，加输入后必须改）。
- UI“画面类型”补齐 9 项；“按住看原片”对任意 Log 输入都传 `--from <输入>`。

**顺带修掉的两件事**
- `--in-matrix auto` 成为默认：读素材 `color_space` 标签决定 YUV→RGB（`bt2020nc` → bt2020，`bt709`/`smpte170m`/`bt470bg`/`smpte240m` 原样，未标注回退 bt2020）。UI 不再按输入类型猜。
- 分段签名加入 `input` 与 `matrix`：以前 `--engine spectral` 下换输入再 `--resume` 会混用旧分段。

### 二、一条关键决策（详见 DECISIONS D061）

| 编号 | 决策 | 结论 |
|---|---|---|
| D061 | 更多 Log + 容器矩阵 | 追加 5 种输入；未知 mode 回退 sRGB；容器矩阵跟素材标签走 |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| 101 点曲线扫描 vs colour-science | S-Log3 / LogC3 / BMD **相对差 0**；LogC4 / DaVinci **≤ 2.3e-16** |
| 2.6 MP CPU vs D3D11 | logc3 **119.63** / logc4 **118.06** / bmd-film **119.60** / davinci **119.64** / s-log3-sgamut3 **119.97 dB** |
| `test_gpu_film` 新用例 | worst ≤ **1.0e-6** |
| `lut bake` 五种输入 | 113–133 ms 全部正常出表 |
| `--in-matrix auto` vs 显式 bt2020 | **逐位相同（inf）**；vs bt709 **44.02 dB**（矩阵确实在起作用） |
| `demo-log-inputs` 九种输入 | 烘表 112–133 ms；GPU/CPU **118.08–119.52 dB** |
| 回归 | 9/9 测试（D061 当时；D062 起 10/10）、M1 parity **88.07 dB**、`demo-m3` 239.7 ms/帧、`demo-m4` 续渲正常、`demo-exr` 读回 inf、UI 第二次预览 **57 ms** |
| CI | run **93 / 95** Windows + macOS 双绿；新增一步“八种输入各烘一张 LUT + 两次光谱出图” |

### 四、怎么用

```powershell
# 直接烘 ARRI / Blackmagic / DaVinci 的负片 LUT
.\build\spektra-roll-cli.exe lut bake --input logc3 --scan negative --out out\logc3.cube
.\build\spektra-roll-cli.exe lut bake --input bmd-film --scan negative --out out\bmd.cube
.\build\spektra-roll-cli.exe lut bake --input davinci --scan negative --out out\dwg.cube

# 视频（容器矩阵现在自动跟素材的 color_space 标签）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input logc4 \
  --scan negative --in clip.mov --out out\graded.mov

# 标签写错了才手动覆盖
.\build\spektra-roll-cli.exe video --input v-log --in-matrix bt709 ...
```

### 五、未完成的待办

- [ ] **OCIO**：仍是"官方曲线 + 固定矩阵"，不是可配置的色彩管理
- [ ] **LogC3 的其他 EI / SUP 2.x、ACEScct**：目前只 EI 800、SUP 3.x
- [ ] **自动识别 Log 格式**：仍要手动选
- [ ] **PIZ/DWA**、**代理清理 / 4K 预览**、**macOS Qt 打包**

---

## 会话 D060 · 多相机 Log 输入

会话目标：按产品负责人选择做 **A · 多相机 Log 输入**（S-Log3 / C-Log3 / V-Log），不再要求用户先把素材转成 sRGB / Apple Log。

### 一、已完成的事项

**D060 · 三种新 Log 输入**
- `InputTransfer` 增加 `SLog3 / CLog3 / VLog`；`--input s-log3|c-log3|v-log`（也接受 `slog3` 等写法）。
- 曲线与色域全部取自 colour-science 0.4.7：S-Log3、Canon Log 3 v1.2、V-Log 的官方公式 + S-Gamut3.Cine / Cinema Gamut / V-Gamut → 线性 Rec.709 矩阵。
- CPU 统一走 `sr::decodeInputToLinearSrgb()`（曲线 → 矩阵 → 负值钳 0）；`FilmModel`、`convert`、预览服务端都改用它。
- GPU 把 `appleLog` 布尔换成 `inputMode`，HLSL 和 MSL 都加同一组曲线/矩阵；`--engine spectral-gpu` 对新输入同样有效。
- UI“画面类型”新增三项；选任何 Log 输入自动勾上 DIR 与 OkLCh。
- 测试：101 点曲线扫描与 colour-science **完全一致**；`test_transfer` 加参考值；`test_gpu_film` 加三组 parity。

### 二、一条关键决策（详见 DECISIONS D060）

| 编号 | 决策 | 结论 |
|---|---|---|
| D060 | 多相机 Log | 官方曲线 + 原生 gamut 矩阵；CPU/GPU 同一套；手动选输入 |

### 三、实测 / 验证

| 项 | 结果 |
|---|---|
| 曲线 | 101 点扫描 vs colour-science **max |diff| = 0** |
| CPU vs D3D11（S-Log3，320×180） | **110.67 dB** |
| LUT 烘焙 | `--input c-log3` 正常出表 |
| Windows 本机 | 9/9 测试通过（含三组新 parity 用例） |

### 四、怎么用

```powershell
# 直接烘 Sony S-Log3 的负片 LUT
.\build\spektra-roll-cli.exe lut bake --input s-log3 --scan negative --out out\slog3.cube
.\build\spektra-roll-cli.exe lut apply --lut out\slog3.cube --in out\frame.png --out out\graded.png

# 或者走物理引擎
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input v-log \
  --scan negative --in clip.mov --out out\graded.mov
```

### 五、未完成的待办

- [ ] **自动识别 Log 格式**：目前要手动选；曲线无法从像素可靠推断
- [ ] **更多 gamut 选项**：例如 S-Gamut3（非 Cine）、ARRI LogC3
- [ ] **OCIO**：仍未引入（先定依赖策略）
- [ ] **PIZ/DWA**、**代理清理 / 4K 预览**、**macOS Qt 打包**

---

## 会话 D059 · EXR ZIP 压缩

会话目标：D058 之后按“先降体积、OCIO 单开一轮”的顺序做 **EXR ZIP**。

### 一、已完成的事项

**D059 · ZIP 压缩成为默认**
- EXR 默认 ZIP（16 scanline / chunk），`--exr-raw` 回到不压缩；读取器同时支持 NONE 和 ZIP。
- 压缩用 stb 的 zlib（16-bit PNG 已在用），没有新依赖；压不动的 chunk 按规范回退成原始块。
- 预处理与 OpenEXR `ImfZip` 对齐：偶数字节前半、奇数字节后半，再做逐字节差分。
- 新增 `scripts\bench-macos.command`：在 Mac 上跑 2.6 MP 的 CPU vs Metal（颜色 / 质感）四组计时。

### 二、一条关键决策（详见 DECISIONS D059）

| 编号 | 决策 | 结论 |
|---|---|---|
| D059 | EXR 压缩 | 默认 ZIP、可 `--exr-raw`；逐块回退；不引入新依赖 |

### 三、踩过并修掉的坑

1. **偶数/奇数拆分方向写反**：自己读自己对称，测试全过，但 ffmpeg 解码只有 **7.2 dB**；修正后 91.5 dB。教训：格式实现必须用第二个解码器交叉验证。
2. 压不动时必须存原始块（`packedSize == rawSize`），否则读取端会去 inflate 一份没压缩的数据。

### 四、实测（2160×1214）

| 内容 | 未压缩 | ZIP | 变化 |
|---|---|---|---|
| 已调色帧（identity LUT） | 31.49 MB | **22.73 MB** | −28% |
| 带颗粒的成片 | 31.49 MB | **31.47 MB** | ≈0% |
| 半精度（同一成片） | 15.74 MB | **9.82 MB** | −38% |
| 640×360 合成图表 | 2.77 MB | **38.7 KB** | −98.6% |

写盘耗时 16.0 → 22.6 ms/帧（含 PNG 读、颜色、写盘）；ffmpeg 解码 92.6 dB。**float 的颗粒会吃掉 zlib 的压缩率**，half 明显更好；再想小需要 PIZ/DWA。

### 五、为什么没接着做 OCIO

OCIO 需要引入 OpenColorIO 库和配置文件，Windows / macOS CI 都要装；没装它的环境里写出来的代码无法真跑。这一轮先把不依赖外部库、能立即验证的 ZIP 做完；OCIO 需要单独一轮先定“引入依赖还是只做 OCIO-lite”。

### 六、未完成的待办

- [ ] **Mac 2.6 MP 实测**：双击 `scripts\bench-macos.command`，把四组 ms 发回来
- [ ] **OCIO**：先定依赖策略
- [ ] **PIZ / DWA 压缩**：颗粒重的素材可再小 2–3 倍
- [ ] **代理清理 / 4K 预览**、**macOS Qt 打包**

---

## 会话 D058 · 线性 EXR

会话目标：D057 之后按产品负责人的选择做 **A · 线性 EXR**。上一版 EXR 是 display-encoded，进调色软件容易多套一次传输函数。

### 一、已完成的事项

**D058 · `--exr-linear`**
- `--exr-linear` + `--exr-trc srgb|pq|hlg` + `--peak`：写 EXR 前把显示编码值反解成线性光，**1.0 = 参考白**（sRGB/HLG 是显示白，PQ 是 `--peak` 尼特）。
- 核心 `sr::linearizeImage`：sRGB 解码**不钳制**（超白保留）；PQ 用 `pqDecode/peak`；HLG 用 `hlgDecode`。
- 接入 `lut apply`（单帧 + `--in-dir` 序列）、`convert`、`gen`；输出不是 `.exr` 时直接报错。
- 写盘前打印 `EXR: linear light (...)`，范围统计随之变成线性值。
- 测试：sRGB 0.5 → 0.214、1.5 的码值 → 2.53（超白保留）、PQ 1000 nits → 1.0、HLG 0.25 往返、非法 transfer 报错。
- `scripts\demo-exr.cmd` 新增 `out\exr-frame-linear.exr` 与对应说明。

### 二、一条关键决策（详见 DECISIONS D058）

| 编号 | 决策 | 结论 |
|---|---|---|
| D058 | 线性浮点交付 | 可选 `--exr-linear`；1.0 = 白/峰值；不钳制；不改默认 |

### 三、实测（2160×1214）

| 项 | 结果 |
|---|---|
| 同一帧 range | display 0.0160..0.8870 → linear **0.0012..0.7619** |
| `convert --exposure 3` | display max 5.34 → linear max **50.3**，仍不钳制 |
| 体积 / 耗时 | 与普通 EXR 相同（float 30 MB / half 15 MB） |

### 四、怎么用

```powershell
# 线性 sRGB EXR
.\build\spektra-roll-cli.exe lut apply --lut out\m3-applelog-sdr.cube `
  --in out\exr-frame.png --out out\frame-linear.exr --exr-linear

# HDR PQ（LUT 用 --hdr pq 烘的）：1.0 = 1000 nits
.\build\spektra-roll-cli.exe lut apply --lut out\hdr.cube `
  --in out\frame.png --out out\frame-linear-pq.exr --exr-linear --exr-trc pq --peak 1000

# 序列
.\build\spektra-roll-cli.exe lut apply --lut out\x.cube --in-dir frames `
  --out-dir frames_linear --out-ext exr --exr-linear
```

### 五、未完成的待办

- [ ] **OCIO**：线性 EXR 只是“按线性解读”，还没有 ACES/相机矩阵/显示变换
- [ ] **EXR 压缩**（ZIP/PIZ）
- [ ] **代理清理 / 4K 预览**、**macOS Qt 打包**

---

## 会话 D057 · macOS Metal 质感

会话目标：D056 让 macOS 的颜色上了 Metal，但质感还在 CPU（约 160 ms/帧），融合提交在 Mac 上用不了。这一轮把质感链也搬到 Metal。

### 一、已完成的事项

**D057 · macOS 质感走 Metal**
- 新增 `src/cli/gpu_texture_metal.mm`（Objective-C++，仅 Apple 编译），实现同一个 `GpuTexture` 接口；CMake 在 Apple 上用它替换 `gpu_texture.cpp`。
- 14 个 compute pass 逐行翻成 MSL：光晕/亮度、盒子模糊（一行/一列一个线程、累加顺序不变）、物理 PSF 卷积、降采样/升采样、颗粒场/叠加、三个 composite、copy。
- **整条链一个 command buffer**，最后只等一次；参数与 PSF taps 都用 `setBytes` 内联，避免共享缓冲在链内被覆盖。
- 融合路径直接借用 film 后端的 `MTLBuffer` 当第一遍输入（不拷贝 42 MB），之后 ping-pong 回自己的缓冲，绝不覆盖 film 结果。
- `fastMathEnabled = NO` 与 film 保持一致；无 Metal 设备时回退 CPU。
- `scripts/verify-macos.command` 第 5 步增加 `test_gpu_texture` 的输出（真机上可直接看到每组 parity 的 dB）。

### 二、一条关键决策（详见 DECISIONS D057）

| 编号 | 决策 | 结论 |
|---|---|---|
| D057 | macOS GPU 质感 | 14 pass 全上 Metal；一个 command buffer + `setBytes`；融合提交在 Mac 重新启用 |

### 三、踩过并修掉的坑

1. **`dispatchThreads` 收的是总线程数**，不是 D3D 的 group 数；照搬会把线程数缩小 64 倍。改用 `dispatchThreadgroups:` 并显式给 `(1,64)` / `(64,1)`。
2. PSF taps 放共享缓冲会被同一 command buffer 里的下一个效果覆盖 → 改成 `setBytes` 内联。
3. 融合路径第一遍读 film 的缓冲，交换后不能把它当写入目标，否则覆盖 film 结果。
4. ARC 下不能对 `id` 数组用非 const 引用遍历（编译错误）→ 索引循环。
5. `fastMathEnabled` 在 macOS 15 SDK 标记弃用（仅警告），继续用于兼容旧系统。

### 四、实测 / 状态

| 项 | 结果 |
|---|---|
| macOS CI（run 85） | **9/9 全过**：`gpu_texture` **2.11 s**、`gpu_chain` **2.10 s**（之前 skip） |
| Windows 本机 | 9/9 全过，D3D11 路径未受影响 |
| 真机 M1 | `test_gpu_texture` 九组 **逐位相同**（worst 0.0）；融合 vs CPU **134.80 dB**；320×180 单帧 CPU 11.5 → Metal **5.6 ms** |
| 真机 M1 · 2.6 MP | 颜色都用 spectral-gpu、含 PNG 读写：质感 CPU **253.6 ms** → Metal **176.6 ms**；质感本身约 **125 → 48 ms（2.6×）** |

### 五、未完成的待办

- [ ] **Mac 2.6 MP 实测**：`test_gpu_texture` 的逐组 dB、`video` 的 ms/帧（现在颜色 + 质感都在 GPU）
- [ ] **线性 / OCIO 交付**、**EXR 压缩**、**代理清理 / 4K 预览**

### 六、复现 / 验收（在 Mac 上）

```bash
# 质感 parity 的逐组 dB
./build/test_gpu_texture

# 融合提交 parity
./build/test_gpu_chain

# 速度：真实素材 2 秒
SPEKTRA_VIDEO_DEBUG=1 ./build/spektra-roll-cli video --engine spectral-gpu \
  --input apple-log --exposure 0.7 --scan negative --contrast 1.15 \
  --couplers on --gamut oklch --grain 0.4 --halation 0.45 --glare 0.2 \
  --diffusion 0.1 --in clip.mov --out out/mac-gpu.mov --duration 2 --codec prores

# 设备与两条链
./build/spektra-roll-cli gpu-info
```

---

## 会话 D056 · macOS Metal 光谱

会话目标：产品负责人指出 macOS 的 GPU 进度落后，要求先做 Metal。macOS 目前只有 CPU 光谱（2.6 MP 约 1.08 s/帧），Windows 已经是 50 ms。

### 一、已完成的事项

**D056 · macOS 颜色走 Metal**
- 新增 `src/cli/gpu_film_metal.mm`（Objective-C++，仅 Apple 编译），实现同一个 `GpuFilm` 接口；CLI / 预览 / UI / 测试的调用方式不变。
- 着色器是 D3D11 版 HLSL 的逐行 MSL 翻译，**运行时 `newLibraryWithSource:` 编译**，没有 `.metallib` 步骤、没有新依赖。
- 常量缓冲用和 C++ 完全相同的标量数组布局；输入/输出/常量都是 `MTLResourceStorageModeShared`，`dispatchThreads` + `waitUntilCompleted` 后直接 memcpy。
- CMake：Apple 上启用 OBJCXX、按平台选源文件、链接 Metal + Foundation、`.mm` 用 ARC。
- 无 Metal 设备时 `available()` 返回 false，自动回退 CPU 光谱。
- **`MTLCreateSystemDefaultDevice()` 返回 nil 时**用 `MTLCopyAllDevices()` 兜底取第一块设备；两者都没有才回退。新增 `spektra-roll-cli gpu-info`（macOS 版本/架构、设备列表、默认设备、两条 GPU 链是否可用）。
- 测试：`gpu_film` 在 macOS CI 上**真跑**（17 个样本 vs CPU 参考）；`gpu_chain` 先查 `GpuTexture::available()`，没有 Metal 质感就 skip。
- `scripts/verify-macos.command` 新增第 5 步：先打 `gpu-info` 和 `test_gpu_film` 的输出，再做 CPU 光谱 vs Metal 光谱出图并打 PSNR。

### 二、一条关键决策（详见 DECISIONS D056）

| 编号 | 决策 | 结论 |
|---|---|---|
| D056 | macOS GPU 颜色 | Metal 移植光谱链；fast math 必须关；质感留到 D057 |

### 三、踩过并修掉的坑（重点）

1. **Metal 默认开 fast math**，编译器按“没有 NaN”优化，把 `density != density` 的保护删掉。光谱表里有几条波长密度是 NaN，于是整帧 NaN：普通路径直接把 NaN 输出，负片路径被 `clamp` 成 0。修法：`MTLCompileOptions.fastMathEnabled = NO`。
2. 这个坑 Windows 复现不了，只能靠 macOS CI 的失败输出定位；`test_gpu_film` 现在失败时会打印逐样本 GPU/CPU 值并显式检查 NaN。
3. 融合提交测试在没有 Metal 质感后端时会 attach 失败；先探测 `GpuTexture::available()` 再决定 skip。
4. **真机 M1 上 `MTLCreateSystemDefaultDevice()` 返回 nil**（Metal 3 明明可用）：默认设备 API 在 macOS 上不是“有 GPU 就给”。加 `MTLCopyAllDevices()` 兜底 + `gpu-info` 诊断；如果仍拿不到，`gpu-info` 会说明是没枚举到、还是枚举到但没选上。

### 四、实测 / 状态

| 项 | 结果 |
|---|---|
| macOS CI（run 80） | `gpu_film` 通过；`gpu_texture` / `gpu_chain` skip；Windows 全绿 |
| Windows 本机 | 9/9 测试通过，GPU 测试仍真跑 |
| 真机 M1（macOS 14.7.5） | `MTLCopyAllDevices: 1 → Apple M1`（默认设备仍 nil）；六组 parity 最差 **2.99e-4**；320×180 CPU vs Metal **115.44 dB**；单帧 10.8 → **7.1 ms**（含 PNG I/O） |
| Mac 2.6 MP 速度 | 2160×1214 单帧（含 16-bit PNG 读写）CPU **424.3 ms** → Metal **129.0 ms（3.3×）** |

### 五、未完成的待办

- [x] ~~**D057：Metal 质感**~~ → 已完成（14 pass 全上 Metal，融合提交在 Mac 重新启用）
- [ ] **Mac 实测**：`verify-macos.command` 跑一遍，记录 Metal 光谱的 ms/帧与 PSNR
- [ ] **线性 / OCIO**、**EXR 压缩**、**代理清理 / 4K 预览**（沿用上一轮）

### 六、复现 / 验收（在 Mac 上）

```bash
# 一键：编译 + 测试 + LUT parity + 视频 + CPU vs Metal PSNR
scripts/verify-macos.command

# 单独量 Metal 光谱
./build/spektra-roll-cli lut apply --engine spectral-gpu --scan negative \
  --couplers on --gamut oklch --in out/frame.png --out out/metal.png
SPEKTRA_VIDEO_DEBUG=1 ./build/spektra-roll-cli video --engine spectral-gpu \
  --input apple-log --exposure 0.7 --scan negative --in clip.mov --out out/mac.mov --duration 2
```

---

## 会话 D055 · 浮点 / EXR

会话目标：D054 之后按清单做 **浮点 / EXR**（D017 欠账）。动手前先确认 CI：run **75**（D054 代码）和 **76**（文档）在 Windows + macOS 都绿。

### 一、已完成的事项

**D055 · OpenEXR 浮点交付**
- **自带最小 OpenEXR**（`image.cpp`）：scanline、NONE 压缩、RGB、FLOAT32 / HALF，读写都是自己拼，不引入 OpenImageIO 或新依赖；头里写 sRGB/Rec.709 + D65 的 `chromaticities`。
- **按扩展名决定格式**：`--out *.exr` 一律写 EXR（默认 float，`--exr-half` 写 half），`--bits` 不参与；`--bits 32` 配 `.png` 会明确报错，不静默降级。
- **EXR 也能读**：`loadImage` 认 `.exr`（NONE、FLOAT/HALF、任意通道顺序），`sourceBits` 报 32；32-bit 源写 PNG 时默认 16-bit。
- **序列**：`lut apply --in-dir ... --out-dir ... --out-ext exr` 把一整段 PNG 转成 float 序列。
- **不钳制**：`convert` 写 EXR 时不再把结果夹到 0..1（PNG 仍夹），所以 `--exposure 3` 的超白能留下来。
- **可见的交代**：写 EXR 时打印 `range: min .. max (x% below 0, y% above 1, written unclipped)`。
- 测试：`test_image` 覆盖 float 精确往返、half 精度、越界值保留、16-bit PNG 对照钳制、32-bit PNG 报错；CI 新增 EXR 冒烟（写 → 读回 → 出 PNG），双平台跑。
- 双击验收：`scripts\demo-exr.cmd`。

### 二、一条关键决策（详见 DECISIONS D055）

| 编号 | 决策 | 结论 |
|---|---|---|
| D055 | 浮点交付 | 自带最小 EXR（不压缩、float/half）；`.exr` 即浮点、不钳制；读回与 PNG16 逐位一致 |

### 三、踩过并记下的坑

1. **16-bit PNG 的 `toShort()` 会夹到 0..1**：浮点交付必须绕过量化，直接写 float 样本，否则 +3 EV 的高光全没了。
2. **EXR 的 chlist 每条是固定 16 字节 + 名字**（pixelType 4、pLinear+保留 4、x/ySampling 各 4），少写一个字段读取器就错位。
3. **通道按 B,G,R 存**是惯例，读取器必须按名字映射，不能假设顺序。
4. **不要静默把 `--bits 32` 降级成 16**：用户要的是浮点，宁可报错让他改扩展名。
5. ffmpeg 的 EXR 解码与我们的 16-bit PNG 对比是 **92.6 dB**，不是 inf——这是独立解码器的舍入差异，不是写坏了；自己读回才是 inf。

### 四、实测（2160×1214）

| 场景 | 结果 |
|---|---|
| 文件体积 | float EXR **30.0 MB** / half **15.0 MB** / 16-bit PNG 15.0 MB |
| 写盘耗时（同一帧，含颜色 + 质感） | EXR 295 ms vs PNG16 293 ms |
| EXR 读回 → PNG 与原 PNG | **PSNR = inf**（逐位相同） |
| ffmpeg 独立解码 EXR → PNG | float **92.6 dB**、half **82.8 dB** |
| `convert --exposure 3` | EXR 范围 **0.0000 .. 5.3438**，**32.973%** 像素 >1 保留；PNG 全切到 1 |

### 五、未完成的待办（按建议顺序）

- [ ] **线性 / OCIO**：EXR 现在是 display-encoded，不是场景线性；需要 `--exr-linear` 或接 OCIO。
- [ ] **EXR 压缩**：NONE 体积大，2.6 MP 30 MB/帧；ZIP/PIZ 能小 3–5 倍。
- [ ] **macOS 的 GPU 路线**：Metal；目前 macOS 只有 CPU 光谱 / LUT。
- [ ] **代理清理 / 4K 预览**：`out/ui-proxy/` 会按素材累积；4K 可考虑 1280 px 代理。

### 六、复现 / 验收

```powershell
scripts\demo-exr.cmd            # PNG16 / float EXR / half EXR + 读回比对 + 超曝光

# 单帧浮点
.\build\spektra-roll-cli.exe lut apply --lut out\m3-applelog-sdr.cube `
  --in out\m3-frame.png --out out\frame.exr --grain 0.4 --halation 0.45

# 序列转 EXR
.\build\spektra-roll-cli.exe lut apply --lut out\identity.cube `
  --in-dir out\seq-in --out-dir out\seq-out --out-ext exr

# 读回
.\build\spektra-roll-cli.exe lut apply --lut out\identity.cube `
  --in out\frame.exr --out out\back.png --bits 16
```

---

## 会话 D054 · 真实点扩散核

会话目标：按建议做 **A（真实点扩散核）**。动手前先跑完健康检查：9/9 测试、M1 parity 88.07 dB、`demo-m3` 220.8 ms/帧、`demo-m4` 续渲正常、Qt 界面能编译、预览第二次 45 ms。

### 一、已完成的事项

**D054 · 光晕 / 眩光 / 扩散换成物理点扩散核**
- 参数从"画幅比例"改成"底片微米"：`--halation-spread 225`、`--glare-spread 725`、`--diffusion-spread 122`（有效 σ，µm），`--frame-width-mm 36` 负责换算；1080p / 4K / 8K 渲染出来是同一个物理大小。
- 核形状：窄芯 + 宽裙的双高斯混合，按有效 σ 归一化（总展宽不变，只改形状）；截断在**最宽分量**的 3σ，3σ 之外的裙部能量是同等方差高斯的 **7–10×**。
- 成本：按核半径自动降采样 1–8×，核按同样比例缩小；结构仍是"降采样 → 分离卷积 → 双线性升采样"（D037 的思路）。
- **CPU / GPU 同一组权重**：核由 `sr::buildSpreadKernel` 在 CPU 生成，GPU 用 structured buffer `t3` 上传，HLSL 只做 `-r..+r` 顺序累加，和 `convolveSeparable` 完全同序。
- `--spread-model box` 保留 D033–D037 的"画幅比例 + 盒状模糊"，两条老路径都没删，旧观感随时可回。
- 预览协议 `texture` 后面新增 `model frameWidthMm` 两个数；渲染签名新增 `spread ...` 一行（换模型/尺度不会混着续渲）。
- 新增双击验收 `scripts\demo-spread.cmd`：同一帧 + 同一段 1 秒视频各渲 box / physical 两遍，打印 PSNR、ms/帧，并排对比。
- 测试：`test_texture` 新增物理尺度、归一化、对称、重尾与 box 回退检查；`test_gpu_texture` 九组配置全过。

### 二、一条关键决策（详见 DECISIONS D054）

| 编号 | 决策 | 结论 |
|---|---|---|
| D054 | 真实点扩散核 | 微米单位 + 双高斯重尾；CPU 生成核权重、GPU 只做卷积；box 模型保留 |

### 三、踩过并修掉的坑（下次别再踩）

1. **HLSL 常量缓冲里的数组是 16 字节/元素**，taps 按 4 字节排进去，GPU 读到的是错位权重，parity 直接掉到 **19–48 dB**；改成 `StructuredBuffer<float> tapsIn : register(t3)` 后 **149–178 dB**。
2. 第一版混合核按"有效 σ"归一化后，3σ 外的裙部反而比同方差高斯**更薄**；把截断改成按最宽分量 3σ，才有真正的重尾。
3. 单元测试里 64 px 的画面在 225 µm 下只有 0.4 px 的扩散，等于没效果；物理核的测试要按底片宽度选画面尺寸（640 px 对应 4 px σ）。

### 四、实测（本机，2160×1214）

| 场景 | 结果 |
|---|---|
| 默认值与旧观感的对应 | 225 / 725 / 122 µm ≈ 13.5 / 43.5 / 7.3 px 有效 σ，强度档位不用改 |
| 同帧 box vs physical | **57.03 dB**（差异集中在高光边缘的柔化与外晕） |
| GPU 视频（光谱 GPU + GPU 质感，ProRes） | **82.2 ms/帧**（D051 时 79.2，+3.8%） |
| LUT + CPU 质感视频 | box 231.8 → physical **251.4 ms/帧**（+8.5%） |
| `demo-m3`（LUT + CPU 质感，H.264） | 220.8 → **235.1 ms/帧**（+6.5%） |
| 单帧 `lut apply`（含 PNG I/O） | box 275.7 → physical **297.6 ms** |
| `demo-gpu-texture` | CPU 236.7 / GPU **82.6 ms/帧（2.87×）**，编码后 PSNR **85.6 dB** |
| GPU parity | `test_gpu_texture` **149.3–177.7 dB**（最差 2.98e-7） |
| 预览 | 第二次 **49 ms**（与之前 45 ms 持平） |

### 五、未完成的待办（按建议顺序）

- [x] ~~**浮点 / EXR**（D017 欠账）~~ → D055 已完成（自带 EXR 读写，float/half，不钳制）
- [ ] **macOS 的 GPU 路线**：Metal；目前 macOS 只有 CPU 光谱 / LUT。
- [ ] **代理清理策略 / 4K 预览**：`out/ui-proxy/` 会按素材累积；4K 素材可考虑 1280 px 代理。
- [ ] **核形状仍是近似**：双高斯是工程近似，没有按胶片层厚/折射率算；要更真需要实测的 halation/glare PSF。

### 六、复现 / 验收

```powershell
scripts\demo-spread.cmd            # 左旧 / 右新，打印 PSNR 与 ms/帧

# 同一帧两种模型
.\build\spektra-roll-cli.exe lut apply --lut out\m3-applelog-sdr.cube `
  --in out\m3-frame.png --out out\spread-physical.png `
  --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1
.\build\spektra-roll-cli.exe lut apply ... --spread-model box --out out\spread-box.png

# 自定义物理尺度（微米）
.\build\spektra-roll-cli.exe video --lut out\m3-applelog-sdr.cube --in X:\agent\footage\C0113.MP4 `
  --out out\spread-custom.mov --halation 0.45 --glare 0.2 --diffusion 0.1 `
  --halation-spread 120 --glare-spread 500 --diffusion-spread 80 --frame-width-mm 24.9
```

---

## 会话 D049–D053 · 融合提交 → 管道 → 并行 → 预览代理

会话目标：产品负责人依次选了 **B（融合提交）→ B2（视频管道）→ 并行分段 → 预览代理**。下面按交付编号整理，所有数字都是本机（i5-10600KF / Arc A750 / 32 GB）实测。

### 一、已完成的事项

**D049 · 颜色 + 质感合并成一次 GPU 提交**
- `GpuFilm::attach()` / `GpuTexture::attach()`：两个后端共用一个 D3D11 设备（所有权仍在 film）。
- `GpuFilm::processToDevice()` + `readback()`：中间结果不回 CPU。
- `GpuTexture::processBuffer()`：对 film 输出 buffer 建 SRV，GPU 内拷贝进 ping-pong 缓冲，只回读一次。
- `tests/test_gpu_chain.cpp`：融合 vs CPU 参考 **124.1 dB**，并检查确定性。
- 视频 **233.6 → 207.1 ms/帧**，与分段路径输出 **逐位相同**；预览 43 → 38 ms。

**D050 · 视频管道修复**
- 分段计时发现 **130 ms/帧** 花在 `float → uint16` 的 `std::lround`；改成乘加并并行两个整帧循环。
- 视频 **207.1 → 85–92 ms/帧**；`demo-m3` 358.7 → 245.5；输出逐位相同。
- 硬件解码测量后**不采用**：QSV+`hwdownload` 21.8 ms/帧 vs 软解 19.5 ms/帧；d3d11va 23 ms/帧。
- 编码器成本终于可见：ProRes 85、H.264+QSV **77**、H.264 软编 104、HEVC 软编 104 ms/帧。

**D051 · 解码/编码与处理重叠**
- `FramePipeReader`（双缓冲）+ `FramePipeWriter`（两槽位），读写线程在 `closePipe` 前 join。
- 视频 **85–92 → 79.2 ms/帧**；`demo-m3` 226.8；相对 D046 的 379 累计 **4.8×**；输出逐位相同。

**D052 · 并行分段**
- `--jobs N`（默认 `0` = `min(3, 核数/4, 段数)`；`1` 回到顺序）；每 worker 独立 `GpuFilm`/`GpuTexture`/颗粒缓存，原子计数器领段。
- 30 秒素材 / 6 段 × 5 秒 / ProRes：**LUT + CPU 质感 157.98 → 84.19 s（1.88×）**；光谱 GPU 53.17 → 48.42 s（1.10×，单卡串行）。
- 输出与顺序渲染 **逐位相同**；并行续渲正常（9 段删 1 补 1）。

**D053 · 长片预览代理**
- 整段进内存会把最长边压到 960 px 以下时，生成 960 px 全 I 帧代理（`out/ui-proxy/<hash>.mov`），`frame N` 按需 `-ss` 取帧；失败回退内存缓存。
- 30 秒素材：代理 **8.7 s / 30.6 MB**，预览 **960×540 · 100 ms/帧**（原先约 640 px）；重开 **48 ms**。
- 4 秒素材不变：内存缓存 1600×898 · 47 ms。
- `frame` 回复带实际宽高；`open` 的 total = 可预览总帧数。

### 二、五条关键决策（详见 DECISIONS D049–D053）

| 编号 | 决策 | 结论 |
|---|---|---|
| D049 | 融合提交 | 两个 GPU 后端共设备、一次上传/回读；失败回退分段 |
| D050 | 管道修复 | 去掉 `std::lround`；**不**做硬件解码（实测更慢） |
| D051 | 读写重叠 | 解码双缓冲 + 编码后台线程，不再阻塞 GPU |
| D052 | 并行分段 | 默认最多 3 个 worker；每个 worker 独立 GPU 对象 |
| D053 | 预览代理 | 只对“装不下/不够清晰”的长片生成代理，短片仍走内存 |

### 三、踩过并修掉的坑（下次别再踩）

1. film 输出 buffer 少了 `BIND_SHADER_RESOURCE`，质感建不了 SRV。
2. film dispatch 后没解绑 UAV，质感读同一 buffer 得到**全黑帧**。
3. `attach()` 后 `initialize()` 误判“已初始化”，着色器没编译 → 访问冲突。
4. 非 Windows 构建里 `#endif` 放进了函数体 → macOS 占位实现括号错位。
5. `std::filesystem::file_time_type::count()` 在 libc++ 上比 `long long` 宽，`std::to_string` 重载歧义 → 只有 macOS 编译失败，需 `static_cast<long long>`。
6. CI 的 macOS 失败先看 job 耗时：1–2 分钟才是真跑；“not acquired by Runner” / “Internal server error” 是 GitHub 的问题，空提交重跑即可。
7. workflow 现在会在构建失败时把 `error:` 行打成 annotation，直接在 run 页面可见。

### 四、未完成的待办（按建议顺序）

- [x] ~~**真实点扩散核**~~ → D054 已完成（微米 + 双高斯重尾核，box 模型保留对照）
- [x] ~~**浮点 / EXR**（D017 欠账）~~ → D055 已完成（自带 EXR，float/half，不钳制）
- [ ] **macOS 的 GPU 路线**：Metal；目前 macOS 只有 CPU 光谱 / LUT。
- [ ] **代理清理策略**：`out/ui-proxy/` 会按素材累积，可加 LRU 或“删除代理”按钮。
- [ ] **4K / 更长片**：预览代理固定 960 px，4K 素材可考虑 1280；分段并行的显存上限需要在更小显存的机器上验证。

### 五、复现 / 验收

```powershell
# 单帧/视频（应约 79 ms/帧，ProRes）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input apple-log --exposure 0.7 `
  --scan negative --contrast 1.15 --couplers on --gamut oklch --grain 0.4 --halation 0.45 `
  --glare 0.2 --diffusion 0.1 --in X:\agent\footage\C0113.MP4 `
  --duration 2 --codec prores --out out\check.mov

$env:SPEKTRA_VIDEO_DEBUG="1"   # 再跑一次，打印 wait/convert/process/pack

.\build\test_gpu_chain.exe                      # 融合 vs CPU，应 ≥60 dB（124 dB）
.\build-ui\spektra-roll-ui.exe --preview-selftest   # 第二次预览应 <100 ms
```

界面：`scripts\build-ui.cmd` → 拖时间线（短片 47 ms；长片自动建 960 px 代理，100 ms）。

---

## 会话 D048 · 预览实时化

会话目标：按建议先做 **A（预览实时化）**。先把旧预览链拆开量，再决定改法。

### 旧链的时间都花在哪（2.6 MP）

| 环节 | 耗时 |
|---|---|
| ffmpeg 抽 1 帧 → 16-bit PNG | ~1000 ms |
| CLI 启动 + 解析 `.cube`（35,937 行） | ~215 ms |
| 16-bit PNG 读 + 写 | ~600–700 ms（写 ~250 ms） |
| D3D11 设备 + 着色器编译（每次进程） | 150–700 ms |
| 真正的颜色 + 质感 | ~50 ms |

结论：不是算得慢，是**每次预览都在重建整个世界**。

### 已完成

1. **`spektra-roll-cli preview` 常驻进程**（D048）：行协议 `look / open / backend / texture / frame / raw / quit`。
2. **整段帧缓存**：`open` 一次性解码进内存（默认 1024 MB、最长边 1600 px），`frame N` 只做颜色 + 质感 + 8-bit RGBA 打包；换 look 不丢缓存。
3. **去掉了 PNG 往返**：预览输出 8-bit RGBA 原始帧，UI 用 `QImage::Format_RGBA8888` 直接接管。
4. **拖动合并**：时间线拖动每 90 ms 出一帧，松手补最后一帧；超缓存的帧在拖动时跳过，不触发慢路径。
5. **`ffprobe` 提速**：`probeGeometry` 改成一次 `-show_streams`；原来单独查 `side_data=rotation` 在这支素材上要 **3.8 s**，合并后 **42 ms**。
6. **旧路径保留为回退**：服务启动失败 / 超出缓存 / 无 ffmpeg 时仍然能出图。
7. **`--preview-selftest`**：启动界面后跑两次真实预览并打印状态行，脚本可验收。
8. **CI**：新增预览服务端协议冒烟（`look` / `open` / `frame` / `raw` + 输出尺寸），run **57** Windows + macOS 双绿（顺手修了 BSD `wc -c` 补齐空格导致 macOS 断言失败）。

### 实测

| 场景 | 旧 | 新 |
|---|---|---|
| LUT 预览（1600 × 898） | 1.7–2.2 s/帧 | **47–64 ms/帧** |
| 光谱 GPU 预览 | ~2.2 s/帧 | **43 ms/帧** |
| 首次打开素材 | — | 2–4 s（解码整段，一次性，取决于磁盘） |
| 常驻内存 | — | 99 帧 × 8.6 MB ≈ **853 MB** |

### 关键决策（详见 D048）

- 预览是**降分辨率 + 8-bit**，但颜色/质感仍走同一套 C++ 管线；状态栏写明尺寸与耗时。
- 帧缓存放在 CLI 进程里，UI 不碰解码细节；换 look 只重载 LUT/模型。
- 长片超出预算时回退单帧路径并明确提示，不做静默降级。
- `preview` 是内部命令，用户入口仍然是 UI 的预览按钮和 Ctrl+P。

### 未完成 / 下一步

- [ ] **颜色 + 质感合并成一次 GPU 提交**：视频仍是 245 ms/帧，空管道约 207 ms。
- [ ] **预览缓存放到磁盘 / 代理文件**：现在只有内存缓存，长片超出后回退单帧；可生成半分辨率代理。
- [x] ~~**真实点扩散核**~~ → D054 已完成（微米 + 双高斯重尾核）。
- [ ] **CI 上的真实 GPU 覆盖**：`gpu_film` / `gpu_texture` 在公有 runner 上会 skip。
- [ ] **macOS 的 GPU 路线**：Metal 或继续 CPU。
- [x] ~~浮点 / EXR（D017 欠账）~~ → D055 已完成。

### 复现命令

```powershell
# 预览服务端协议（CI 也跑这一段）
printf 'look --lut out/m3-applelog-sdr.cube\nopen "X:\agent\footage\C0113.MP4"\nframe 40 "out/p.rgba"\nquit\n' |
  .\build\spektra-roll-cli.exe preview --ffmpeg J:\ffmpeg\bin\ffmpeg.exe --in-matrix bt2020

# 界面里跑两次真实预览并打印耗时（无需手点）
.\build-ui\spektra-roll-ui.exe --preview-selftest

# 打开调试计时
$env:SPEKTRA_PREVIEW_DEBUG="1"; .\build-ui\spektra-roll-ui.exe --preview-selftest
```

界面验证：`scripts\build-ui.cmd` → 拖时间线，状态栏应显示 `47–64 ms · 整段缓存`。

---

## 会话 D047 · 空间质感上 GPU

会话目标：D046 之后颜色只剩约 50 ms/帧，视频却仍要约 380 ms/帧。先把时间花在哪量清楚，再决定要不要移植。

### 先量了一下（2.6 MP 单帧，`lut apply`，质感逐项开启）

| 配置 | 耗时 | 增量 |
|---|---|---|
| 只套 LUT | 14.7 ms | — |
| + 光晕 | 64.1 ms | +49 |
| + 眩光 | 57.3 ms | +43 |
| + 扩散 | 65.9 ms | +51 |
| + 颗粒（无缓存，两张场） | 138.6 ms | +124 |
| 全部 | 295.5 ms | +281 |

视频里颗粒有 `GrainFieldCache`，每帧约 1/3 个场，所以质感约 **160 ms/帧**；实测 GPU 颜色视频（无质感）207 ms/帧、全质感 366 ms/帧，与上表吻合。

### 已完成

1. **`sr::applyTexture()` 收敛成唯一 CPU 参考链**（`texture.hpp/texture.cpp`）：halation → glare → diffusion → grain，CLI 和 GPU parity 测试都调它，避免两处顺序漂移。
2. **`GpuTexture`（D047，`--texture-engine`）**：11 个 D3D11 compute pass，逐行翻译 CPU 实现。
   - 盒状模糊一行/一列一个线程，**浮点累加顺序与 CPU 完全一致**；
   - 宽模糊复用 CPU 的降采样 → 2 段模糊 → 双线性升采样（眩光 4×，光晕/扩散 2×），小画幅仍走全分辨率 3 段；
   - 颗粒用同一套 uint hash 在 GPU 上重建场，`--grain-rate` 的两场交叉淡化与归一化不变；
   - 无设备/编译失败/运行时错误 → stderr 一次 warning，整条序列退回 CPU。
3. **一致性**：`tests/test_gpu_texture.cpp` 九组配置（单项、raw noise、rate 1/3、demo-m3 全开、小画幅全分辨率分支）与 CPU 逐像素对比 **143–166 dB，最差 4.5e-7**；真实负片帧 **123.1 dB**。无显卡时 exit 77，ctest 记 skipped。
4. **实测**：
   - 视频全质感 **382.7 → 245.6 ms/帧（1.56×）**；两段 2 秒 demo **386.8 → 233.6 ms/帧（1.66×）**；
   - 质感本身 **约 160 → 约 38 ms/帧**；
   - 单帧 `lut apply`（含着色器编译）309.5 → 214.5 ms；
   - 两段编码后视频 PSNR 83.6 dB（差异来自编码器）。
5. **UI**：输出栏新增“GPU 质感（颗粒 / 光晕 / 眩光 / 扩散）”勾选框，默认开、随设置保存；预览、渲染、队列走同一条 `--texture-engine gpu`。
6. **双击验收**：`scripts\demo-gpu-texture.cmd` 用同一条命令渲两遍（CPU / GPU），打印两条 ms/帧、加速比和编码后 PSNR。
7. **CI**：新增 GPU 质感回退冒烟（`--texture-engine gpu` 在无显卡 runner 上必须自动退回 CPU），run **54** Windows + macOS 双绿。
8. 文档同步：`DECISIONS.md` D047、本文件、`STATUS.md`、`README.md`、`RUN.md`、CLI `--help`。

### 关键决策（详见 D047）

- `--texture-engine auto|cpu|gpu`：`auto` 只在 `--engine spectral-gpu` 时用 GPU 质感；`gpu` 也可配 LUT；**永不硬失败**。
- 空间质感仍然**不是** LUT 的一部分，顺序仍是 halation → glare → diffusion → grain。
- 盒状模糊的线程划分为了 **逐位对齐 CPU**，刻意不改成并行前缀和（换了顺序就过不了 parity）。
- GPU 质感不改变 `--grain-rate` 默认 3，也不改任何观感参数。

### 未完成 / 下一步

- [ ] **颜色 + 质感合并成一次 GPU 提交**：现在两段各自上传/回读 42 MB，空管道仍约 207 ms/帧；合并后预计再省 15–30 ms/帧，并消掉一次 16-bit 往返。
- [ ] **预览实时化**：GPU 颜色已够快，但 UI 预览仍是 “ffmpeg 抽帧 → 写 PNG → CLI 进程 → 读回”，整链 0.3–0.5 s；可做常驻进程或低分辨率直出。
- [x] ~~**真实点扩散核**~~ → D054 已完成（微米 + 双高斯重尾核，GPU 与 CPU 同一组权重）。
- [ ] **CI 上的真实 GPU 覆盖**：`gpu_film` 和 `gpu_texture` 在公有 runner 上会 skip。
- [ ] **macOS 的 GPU 路线**：Metal 或继续 CPU（当前选择）。
- [ ] **队列多任务/多段并行**：GPU 已释放 CPU。
- [x] ~~浮点 / EXR（D017 欠账）~~ → D055 已完成。

### 复现命令

```powershell
# 质感在 GPU 上（配合光谱 GPU 时 auto 自动开启）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input apple-log --exposure 0.7 `
  --scan negative --contrast 1.15 --couplers on --gamut oklch `
  --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1 `
  --in X:\agent\footage\C0113.MP4 --out out\gpu.mov --frames 24 --codec prores

# 强制 CPU 参考链做对比
.\build\spektra-roll-cli.exe video ... --texture-engine cpu

# parity 测试（无显卡时自动 skip）
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir build -R gpu_texture --output-on-failure
```

界面验证：`scripts\build-ui.cmd` → 勾上/取消“GPU 质感” → 预览一帧 / 渲染整段，比较速度。

---

## 会话 D044–D046 · 颜色改成物理计算并上 GPU

会话目标：按你的要求，**能物理算的地方不要再靠 LUT 采样**，并且用满本机（i5-10600KF / 32 GB / Arc A750）。

### 已完成

1. **可选 CPU 光谱引擎**（D044，`--engine spectral`）：每个像素直接跑 `FilmModel::transformFrom`，负片/印片/耦合剂/OkLCh/HDR/Apple Log/曝光/反差全部进物理链。2.6 MP 单帧 1081 ms。
2. **D3D11 探针**（D045，`tools/gpu-probe/`）：Arc A750 上 81 波长扫描 0.63 ms，确认值得移植。
3. **GPU 光谱引擎**（D046，`--engine spectral-gpu`）：HLSL 逐行翻译 CPU 参考链，常量缓冲 22.9 KB（曲线 / 密度 / CMF / 光源 / 矩阵 / 标量），structured buffer 进出，读回解包。
   - 2.6 MP 单帧 **1038–1158 ms → 47–50 ms**（约 21–23×，含 42 MB 上传 + 42 MB 回读）
   - 2.6 MP 视频（ProRes 422 HQ）**1427 → 379 ms/帧**（约 3.8×；瓶颈现在是纹理叠加与编码）
4. **一致性验证**：与 CPU 光谱逐像素比对，负片 119.1 dB、印片 inf（逐位相同）、PQ 82.9 dB、HLG 90.1 dB、`--strength 0.6` 121.3 dB、视频 86.1 dB。
5. **`tests/test_gpu_film.cpp`**：六组配置（sRGB/Apple Log、印片/负片、耦合剂、PQ、strength 混合）与 CPU 参考比对，最差偏差 3e-4；无 D3D11 设备时 exit 77，ctest 记为 skipped。
6. **强制回退**：无设备 / 着色器编译失败 / 运行时出错 → stderr 打 warning，自动退回 CPU 光谱（实测回退输出与 CPU 逐位相同）。
7. **UI 接入**：引擎下拉新增“光谱 GPU（物理，快）”，预览、批量渲染、队列、分段/续渲/时间码都走同一条 CLI；队列恢复能识别新引擎值。
8. **CI**：新增“物理引擎冒烟”（`spectral` + `spectral-gpu` 各跑一张 16-bit 小图，无显卡自动回退）；修正非 Windows 下的占位实现后 **Windows + macOS 双绿**（run 41440266503）。
9. 文档同步：`DECISIONS.md` D046、`README.md`、`STATUS.md`、本文件。

### 关键决策（详见 D044–D046）

- 默认仍然是 **LUT**；物理引擎是可选路径，界面上明确标注“慢（CPU）/ 快（GPU）”。
- **CPU 光谱是唯一参考**，GPU 必须先过 ≥60 dB 才允许接入（实际 82–119 dB）。
- GPU 只做**颜色**；颗粒/光晕/眩光/扩散仍在 CPU 按原顺序叠加。
- GPU 路径 **Windows-only（D3D11）**，其它平台/失败一律回退 CPU，不允许硬失败。
- 每个会话结束时更新 progress/STATUS/DECISIONS，且交付物必须能双击验收。

### 未完成 / 下一步

- [ ] **把空间质感搬到 GPU**：379 ms/帧里颜色只占约 50 ms，剩下是颗粒与模糊；这是目前最大的单点收益。
- [ ] **预览实时化**：GPU 后颜色已够快，但 UI 预览仍走 “ffmpeg 抽帧 → 写 PNG → CLI → 读回”，整链约 0.3–0.5 s；可做低分辨率直出/常驻进程来接近拖动实时。
- [ ] **CI 上的真实 GPU 覆盖**：公有 runner 一般没有可用 D3D11 设备，`gpu_film` 会被 skip；需要在有 GPU 的机器上跑一次（或加 WARP 检查）才能保证着色器编译不退化。
- [ ] **macOS 的 GPU 路线**：Metal 或继续 CPU 光谱（当前选择），若要 macOS 也快需要单独一层。
- [ ] 队列**多任务/多段并行**：GPU 已释放 CPU，可以并行渲染多个任务。
- [x] ~~真实点扩散核~~ → D054 已完成（微米 + 重尾核）。

### 复现命令

```powershell
# 单帧 GPU 光谱（约 50 ms）
.\build\spektra-roll-cli.exe lut apply --engine spectral-gpu --input apple-log --scan negative `
  --couplers on --gamut oklch --exposure 0.7 --in out\m3-frame.png --out out\gpu.png

# 视频（约 379 ms/帧，ProRes）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input apple-log --exposure 0.7 `
  --scan negative --contrast 1.15 --couplers on --gamut oklch `
  --in X:\agent\footage\C0113.MP4 --out out\gpu.mov --frames 3 --codec prores

# 一致性测试（无显卡时自动 skip）
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir build --output-on-failure
```

界面验证：`scripts\build-ui.cmd` → 引擎选“光谱 GPU（物理，快）”→ 预览一帧 / 渲染整段。

---

## M3 阶段（早期）

一次会话把项目从"M2 尾声"推到"M3 完成"，按时间顺序：

1. 修好 HDR 视频：原来 demo 是单帧循环 4 秒，改成 **99 帧整段 + 原声**
2. **M3 视频读写**（D022）：`spektra-roll-cli video` 直接吃 `.mov`，ffmpeg 解码 → 逐帧套 LUT → 重新编码，音画同步
3. **帧区间 + ProRes**（D031）：`--start/--frames/--duration`、`--codec prores`（422 HQ）
4. **macOS 修复**（issue #1）：POSIX `popen` 不接受 `"rb"/"wb"`，已修，CI 两个平台都跑视频冒烟
5. **多线程**（D032）：套 LUT 96 → 17 ms
6. **胶片质感**：颗粒 + 光晕（D033）→ 调轻（D034）→ 眩光 + 扩散（D035）→ 颗粒时间连续（D036）
7. **速度**：宽模糊降采样（D037）、`--crf/--preset`、`--encoder qsv`（D038）
8. **颗粒场缓存**（D039）：一个噪声场跨帧复用，全质感 **476.5 → 358.7 ms/帧**，整段 47.2 → 35.5 秒
9. **分段渲染 + 断点续渲 + 时间码**（D040）：`--segment` / `--resume`，中断后只补缺失段；输出保留相机 timecode
10. **Qt 6 桌面界面首版**（D009）：选素材 → 烘 LUT → 调质感 → 预览一帧 → 渲染整段；后续加上时间线、原片对比、拖放和**渲染队列**

**你亲自验收过的**：
- MacBook Air M1 上 `scripts\verify-macos.command` 全过（含视频路径）
- HDR 片段在 iPhone 16 Pro 上看过
- 负片观感、颗粒浓度（0.4 / 0.45）确认"可以了"
- 视频颗粒闪烁问题已反馈并修复（−44% 帧间差）

---

## 一、已完成的事项

### 1. 调研与方向确定

- [x] 分析上游 `andreavolpato/spektrafilm`（Python，静帧，GPLv3）
- [x] 分析 `SpektraLab`（macOS SwiftUI 商业产品，同一套引擎）
- [x] 分析 Filmulator（另一条技术路线，未采用）
- [x] 确认核心可行性判断：**光谱管线跑不动视频，必须把颜色烘焙成 3D LUT，空间效果留在 GPU pass**
- [x] 确认双平台策略：共享 C++ 引擎 + macOS SwiftUI / Windows WinUI 3 两套原生 UI

### 2. M0 可运行交付（已完成并验收）

| 项 | 状态 |
|---|---|
| 仓库骨架 + GPL-3.0 许可证 | ✅ |
| C++20 / CMake / Ninja 构建 | ✅ |
| `.cube` 解析器（TITLE / LUT_3D_SIZE / DOMAIN_MIN/MAX / 注释 / 错误行号） | ✅ |
| 四面体 + 三线性插值 + 越界钳制 | ✅ |
| PNG 读写（stb 单头文件）+ 内置测试图 | ✅ |
| 命令行 `gen` / `lut gen` / `lut info` / `lut apply --strength` | ✅ |
| 单元测试 6 组 | ✅ 6/6 通过 |
| GitHub Actions：Windows + macOS 编译 / 测试 / 冒烟 | ✅ **两个 job 都是绿的（你已确认）** |
| 免管理员权限的便携工具链 | ✅ |
| 双击即用的 `.cmd` 脚本 | ✅ |
| 文档：README / RUN / STATUS / DECISIONS / architecture / licensing | ✅ |

### 3. M1 真正的胶片 LUT（已完成并自测通过）

| 项 | 状态 |
|---|---|
| 导入上游 profile：Portra 400 / Portra Endura / 中性滤色片（CC BY-SA 4.0） | ✅ |
| 最小 JSON 解析器（支持 `null` → NaN） | ✅ |
| profile 加载 + 形状校验（81 点 / 380–780 nm / 5 nm） | ✅ |
| Mallett 2019 光谱上采样（81×3 基函数，内嵌） | ✅ |
| 胶片灵敏度积分 + 特征曲线 | ✅ |
| CMY → 光谱密度（含 NaN → 透过率 0） | ✅ |
| 放大机：3400 K + KG3 + 二向色中性滤色片 | ✅ |
| 相纸灵敏度 + 参数化特征曲线（normal CDF 叠加） | ✅ |
| 印片曝光因子（中灰几何平均归一）| ✅ |
| 扫描：D50 → XYZ → CAT02 → 线性 sRGB → 编码 | ✅ |
| `lut bake`（33³ float32 `.cube`，约 245 ms） | ✅ |
| `lut compare`（PSNR + CIE76 ΔE，逐点 35,937 个） | ✅ |
| 16-bit PNG 读 / 写（自己拼 IHDR/IDAT/CRC，复用 stb deflate） | ✅ |
| `gen --bits 16`、`lut apply` 自动保持位深 | ✅ |
| 单元测试：LUT / 胶片模型 / 图像 I/O / 传输函数，4 个程序全绿 | ✅ |
| 双击验收：`scripts\demo-m1.cmd`、`scripts\verify-m1.cmd` | ✅ |
| parity：PSNR **88.07 dB**，ΔE76 max **0.0065** | ✅ |
| CI：Windows + macOS 都跑 `lut bake` + `lut compare`（D019） | ✅ 两个 job 绿 |
| 冻结参考 LUT：`tests/reference/portra400_endura_33.cube` | ✅ |

### 4. M2 颜色正确（已完成并验收）

| 项 | 状态 |
|---|---|
| Apple Log 输入（LUT 定义域 = Apple Log 码值，内部解码到 BT.2020→sRGB） | ✅ 与 colour-science 差 ≤1 个 16-bit LSB |
| `convert --from apple-log`（只解码的对照图） | ✅ |
| 扫描负片（推荐路径）：反相 + 去片基 + 灰点中性化 | ✅ 天空 0.48–0.70，暗部死黑 7.7%→2.5% |
| `--contrast`，负片默认 1.15（你选的） | ✅ |
| 输出色域压缩（OkLCh，膝点 0.8/1.0/2.0） | ✅ Apple Log 印片负值 3,763→0 |
| DIR 耦合剂（与上游参考 PSNR **80.75 dB**） | ✅ |
| HDR 输出（PQ / HLG + `--peak`，16-bit PNG） | ✅ 1.0 → PQ 0.75183 |
| `scripts\demo-m2.cmd`（抽帧 + 三张图 + HDR 片段） | ✅ |

### 5. M3 视频与质感（已完成并验收）

| 项 | 状态 |
|---|---|
| `spektra-roll-cli video`：解码 → 套 LUT → 编码，带原声，自动处理 -90 旋转 | ✅ 99 帧约 26 秒（无质感） |
| `--start/--frames/--duration`（音画同步） | ✅ 24 帧片段验过 1.000 s / 1.021 s |
| `--codec prores`（422 HQ，HDR 时带 PQ 标记） | ✅ 120 MB / 4.1 s |
| 多线程（`--threads`，0=自动） | ✅ 套 LUT 96→17 ms |
| 颗粒 + 光晕（确定性、逐帧） | ✅ `--grain` / `--halation`，`--grain-rate` |
| 眩光 + 扩散 | ✅ `--glare` / `--diffusion` |
| 颗粒时间连续（两场交叉淡化） | ✅ 帧间差 −44%，你确认不再闪 |
| 宽模糊降采样（眩光 4×、光晕/扩散 2×） | ✅ 579→469 ms/帧，肉眼无差 |
| `--crf` / `--preset` / `--encoder qsv` | ✅ QSV 在 2.6 MP 下收益有限 |
| 颗粒场缓存（`GrainFieldCache`，LRU 两个场） | ✅ 476.5→**358.7 ms/帧**，整段 47.2→35.5 s |
| macOS `popen` 修复（issue #1） | ✅ CI 两平台都跑视频冒烟 |
| `scripts\demo-m3.cmd`（干净帧 / 质感帧 / 整段视频） | ✅ |

### 6. M4 分段渲染 + 断点续渲 + 时间码（已完成）

| 项 | 状态 |
|---|---|
| `--segment SEC`：拆成 `<out>.parts/part-NNNNN.mov` | ✅ 每段只写视频，边界用全片帧号保持颗粒连续 |
| `--resume`：跳过已完成的段 | ✅ 删掉第 4 段后重跑，只渲染 5 s 的 1 段 |
| 音频只从源文件复制一次 | ✅ 分段不再逐段编码 AAC，无接缝 |
| `--keep-parts` | ✅ 保留分段便于检查 |
| 时间码：读源 `timecode`，`--start` 时按帧偏移 | ✅ 15:38:35:00 → `--start 1` 得 15:38:36:00 |
| `--timecode` 覆盖 / `--no-timecode` 关闭 | ✅ |
| 分段 vs 整段逐帧对比 | ✅ PSNR 约 46 dB，边界无跳变 |
| CI 两平台跑分段 + 时间码冒烟 | ✅ Windows + macOS 绿（run 37） |
| `scripts\demo-m4.cmd`（渲染 → 模拟崩溃 → 续渲） | ✅ |

### 7. 改名与上线

- [x] 产品名定案 **SpektraRoll**，全仓库同步改名
  - 本地目录 `X:\agent\spektravideo` → `X:\agent\SpektraRoll`
  - CMake 工程名 `project(SpektraRoll)`
  - 二进制 `spektravideo-cli` → `spektra-roll-cli`
  - C++ 命名空间 `sv::` → `sr::`，头文件目录 `src/core/include/sr/`
  - CI、脚本、文档全部同步
- [x] GitHub 公开仓库建立并推送成功
- [x] GitHub 登录凭据问题解决（`scripts\push.cmd` 一键登录 + 推送）
- [x] GitHub Actions 双平台首次跑通，**Windows 和 macOS 都绿**
- [x] M0 历史 6 个 commit 已全部推送；M1 已提交并推送（`f493ee9`），CI 双平台绿
- [x] CI 现在在 Windows + macOS 都烘 M1 LUT 并和冻结参考对比（D019）

### 8. 实测数据（本机，真实跑出来的）

| 项 | 结果 |
|---|---|
| CPU | Intel i5-10600KF |
| GPU | Intel Arc A750（支持 D3D12 / Vulkan） |
| 内存 | 32 GB |
| 编译器 | MinGW-w64 GCC 16.2.0（便携版，无管理员权限） |
| CMake | 4.4.4（`C:\Program Files\CMake\bin\cmake.exe`） |
| Ninja | 1.13.2（`X:\agent\tools\ninja\ninja.exe`） |
| 单元测试 | **9/9** 程序通过（LUT、胶片模型、图像 I/O、Apple Log/传输、色域、质感、GPU 颜色 / 质感 / 融合 parity） |
| 33³ LUT 烘焙 | **约 108 ms**（多线程，含写文件）；单线程 198 ms |
| 2.6 MP 套 33³ LUT（16-bit） | **约 17 ms**（多线程）；单线程 96 ms |
| 视频渲染（2.6 MP，HEVC） | 无质感 **187 ms/帧**；全质感 **358.7 ms/帧**（场缓存前 476.5）；4.1 s 素材约 35.5 秒 |
| 光谱引擎（2.6 MP，12 线程） | 单帧 **1081 ms**；视频 **1258.9 ms/帧**；99 帧约 2 分钟 |
| 光谱 vs 33³ LUT（同一帧） | PSNR **62.19 dB**（R/G/B 63.5 / 62.9 / 60.7），LUT 采样误差很小 |
| GPU 扫描探针（Arc A750，D3D11） | 2.6 MP × 81 样本 **0.63 ms**（CPU 光谱整帧 1081 ms） |
| **GPU 光谱引擎（2.6 MP 单帧）** | **47–50 ms**（含 42 MB 上传 + 42 MB 回读）；CPU 光谱 1038–1158 ms |
| **GPU 光谱引擎（2.6 MP 视频）** | **379 ms/帧**（ProRes 422 HQ，含编码与管道）；CPU 光谱 1427 ms/帧 |
| **GPU 质感（2.6 MP 视频，全质感）** | **245.6 ms/帧**（CPU 质感 382.7）；`demo-gpu-texture` 233.6 vs 386.8（1.66×） |
| **GPU 质感本身** | 约 **38 ms/帧**（CPU 约 160）；单帧 `lut apply` 309.5 → 214.5 ms |
| **GPU 质感 vs CPU 参考** | 九组配置 **143–166 dB**（最差 4.5e-7）；真实负片帧 **123.1 dB** |
| **UI 预览（D048，1600×898）** | **47–64 ms/帧**（旧链 1.7–2.2 s，约 30×）；光谱 GPU 43 ms；首次打开解码 2–4 s |
| **融合提交（D049）** | 视频全质感 **207.1 ms/帧**（分段 233.6）；与分段输出 **逐位相同**；预览光谱 GPU **38 ms/帧** |
| **管道修复（D050）** | 视频 **207.1 → 85–92 ms/帧**（2.4×）；输出逐位相同；`pack` 从 130 → 5 ms |
| **读写重叠（D051）** | 视频 **85–92 → 79.2 ms/帧**；`demo-m3` 245.5 → 226.8；相对 D046 累计 **4.8×**；输出逐位相同 |
| **并行分段（D052）** | CPU 路径 **157.98 → 84.19 s（1.88×）**；GPU 路径 1.10×（单卡）；输出逐位相同；并行续渲正常 |
| **长片预览代理（D053）** | 30 秒素材代理 8.7 s / 30.6 MB；预览 **960×540 · 100 ms/帧**（原先约 640 px）；重开 48 ms；4 秒素材不变 |
| **物理点扩散核（D054）** | 同帧 box vs physical **57.03 dB**；GPU 视频 **82.2 ms/帧**（+3.8%）；LUT+CPU 质感 231.8 → 251.4 ms/帧；`demo-gpu-texture` CPU 236.7 / GPU **82.6（2.87×）**，编码后 85.6 dB；parity 149–178 dB |
| **浮点 EXR（D055）** | 2.6 MP：float **30.0 MB** / half **15.0 MB** / PNG16 15.0 MB；读写耗时与 PNG 相当；读回 **PSNR = inf**；ffmpeg 独立解码 92.6 dB（half 82.8）；+3 EV 时 **32.97%** 像素 >1（max 5.34）保留 |
| **Metal 光谱（D056）** | macOS CI run 80 双绿；真机 M1（macOS 14.7.5）：`MTLCopyAllDevices` 找到 Apple M1、默认设备仍 nil；六组 parity 最差 **2.99e-4**，320×180 CPU vs Metal **115.44 dB**；2.6 MP 单帧 **424.3 → 129.0 ms（3.3×）**（含 PNG I/O） |
| **Metal 质感（D057）** | macOS CI run 85 **9/9 全过**：`gpu_texture` 2.11 s、`gpu_chain` 2.10 s；真机 M1：九组 **逐位相同**（worst 0.0），融合 **134.80 dB**；2.6 MP 质感 **253.6 → 176.6 ms**（含颜色与 I/O，质感本身约 125 → 48 ms） |
| **线性 EXR（D058）** | 同帧 display 0.0160..0.8870 → linear **0.0012..0.7619**；+3 EV 线性最大 **50.3**；体积/耗时与普通 EXR 相同 |
| **EXR ZIP（D059）** | 已调色帧 31.49 → **22.73 MB（−28%）**；带颗粒 ≈0%；half **9.82 MB（−38%）**；合成图表 −98.6%；ffmpeg 解码 92.6 dB |
| **多相机 Log（D060）** | 三条曲线 101 点扫描 vs colour-science **max diff = 0**；S-Log3 CPU vs D3D11 **110.67 dB**；`c-log3` LUT 烘焙正常 |
| **更多 Log + 容器矩阵（D061）** | 五条新曲线 101 点：**0 差**（LogC4/DaVinci ≤ 2.3e-16）；2.6 MP CPU vs D3D11 **118.06–119.97 dB**；`--in-matrix auto` 与 bt2020 **逐位相同**、与 bt709 44.02 dB；九种输入烘表 112–133 ms |
| **OpenColorIO（D062）** | 可选依赖（`SR_WITH_OCIO`）+ 自建 MinGW 静态库；OCIO sRGB vs 内置 **118.24 dB**、Apple Log vs 内置 **93.60 dB**、ARRI LogC3 LUT vs 内置 **63.89 dB**；10/10 测试（`ocio` 无库时 skip） |
| **预览帧缓存** | 99 帧 × 8.6 MB ≈ **853 MB**（默认上限 1024 MB） |
| **GPU vs CPU 光谱 PSNR** | 负片 119.1 dB、印片 inf、PQ 82.9 dB、HLG 90.1 dB、strength 0.6 时 121.3 dB、视频 86.1 dB |
| 颗粒场缓存 | 每 3 帧只生成 1 个新场（原来每帧 2 个）；占用约 21 MB |
| 分段渲染 | 4 s 素材 / 1 s 段 = 5 段；合并后 99 帧 / 4.125 s / 原声 / timecode 15:38:35:00 |
| 断点续渲 | 删掉第 4 段后重跑：3 段跳过，只渲 1 段（约 5 s，整段约 35 s） |
| 分段 vs 整段 | 逐帧 PSNR 约 46 dB（差异来自强制关键帧），边界无跳变 |
| Qt UI 预览 | 2160×1214 @ 1.5 s，抽取 + 套 LUT + 质感约 1 s |
| Qt UI 整段渲染 | 99 帧 / 329.2 ms/帧 / 32.6 s（与 CLI 同一路径） |
| Qt UI 渲染队列 | 2 条任务顺序完成（48 帧 00:21、12 帧 00:08）；进度条与 ETA 正常；重启后队列恢复 |
| 颗粒时间稳定性 | 帧间差 0.0025（rate 1）→ **0.0014**（rate 3） |
| C++ vs Python 参考 | **PSNR 88.07 dB**，max \\|diff\\| 4.57e-4，ΔE76 max 0.0065 |
| Apple Log 解码 vs colour-science | 2.6 MP 帧，max \\|diff\\| **1.53e-5**（= 16-bit 1 LSB），mean 1.9e-8 |
| Apple Log 素材 | 2160x1214，24p，HEVC 10-bit 4:2:2，BT.2020 |
| 扫描负片 vs 印片（同一帧） | 天空 0.48–0.70 / 0.88–0.91；暗部 <0.10 由 7.7% 降到 2.5% |
| 负片反差 1.0 / 1.15 / 1.3 | 天空 p5–p95 0.48–0.70 / 0.49–0.75 / 0.49–0.79；暗部 2.5% / 6.8% / 13.1% |
| 印片曝光因子 | 0.837773013976（上游含一次 sRGB→sRGB 往返，0.837787256300） |
| 产物 | `build\spektra-roll-cli.exe`，静态链接，无 DLL 依赖 |

**重要推论**：套 LUT 已经压到 17 ms/2.6 MP；颗粒场缓存把质感从 290 ms 降到约 172 ms。现在整帧的剩余开销主要落在光晕/眩光/扩散三个宽模糊和管道/格式转换上，编码器仍不是瓶颈。

### 9. M5 Qt 6 桌面界面（首版，D009）

| 项 | 状态 |
|---|---|
| Qt 6.8.3 + MinGW 13.1 便携安装（`X:\agent\tools\qt`，不进仓库） | ✅ `aqtinstall` 安装 |
| `spektra-roll-ui` 目标（`-DSR_BUILD_QT_UI=ON`） | ✅ 与 CLI 共用 `sr_core` / 同一套命令 |
| 素材、LUT、曝光、负片/印片、反差、SDR/HDR 选择 | ✅ |
| 颗粒 / 光晕 / 眩光 / 扩散 / 颗粒稳定度 | ✅ |
| **预览一帧**（ffmpeg 抽帧 → 写 PNG → `lut apply` → 窗口显示） | ✅ 2160×1214 @ 1.5 s 实测（**已被 D048 取代**） |
| **预览常驻进程**（D048）：整段解码进内存，`frame` 只跑颜色 + 质感 | ✅ **47–64 ms/帧**（旧链 1.7–2.2 s）；首次打开 2–4 s |
| **时间线拖动**：拖到任意帧，松开刷新预览；显示 `mm:ss / 总长 · 帧 n/N` | ✅ |
| **素材信息条**：分辨率、帧率、时长、帧数、相机 timecode | ✅ 自动 ffprobe，含旋转 |
| **按住看原片**：按住显示未套胶片的解码画面，松开回到成片 | ✅ |
| **拖放素材**到窗口即可载入 | ✅ |
| 快捷键 `Ctrl+P` 预览 / `Ctrl+R` 渲染 / `Ctrl+B` 烘焙 | ✅ |
| **输出文件夹**按钮 | ✅ |
| **烘焙 LUT**（Apple Log/SDR，HDR 自动带 PQ/HLG） | ✅ |
| **渲染整段**（编码、起始、帧数、分段、续渲） | ✅ 日志、进度条、取消 |
| **渲染队列**：冻结参数入队、自动避免重名输出、逐条顺序渲染 | ✅ 每条显示 等待/渲染中/完成/失败/已取消 |
| 队列操作：开始、停止、移除、清空已完成、打开输出 | ✅ 停止后未完成任务保留为“等待” |
| 每行**进度条 + 用时/剩余时间** | ✅ 每秒刷新，按已完成帧数估算 ETA |
| 队列中每条任务使用 **LUT 快照**，之后改参数不会影响已入队任务 | ✅ `out/ui-queue/lut-*.cube` |
| **队列持久化**：`out/ui-queue/queue.json`，重启恢复；中断任务回到“等待” | ✅ 实测重启后 2 条任务恢复 |
| **多文件拖入 / 批量加入**：一次多个视频，按素材名自动生成任务 | ✅ |
| **界面 v2**（D041）：三栏 + 顶栏、SpektraLab 灰/琥珀令牌、Section + SliderRow | ✅ 截图更新 |
| 参数栏改成“标签 + 数值 + 细滑块”，队列切换时右栏自动加宽 | ✅ |
| **直方图叠加**（画布右上角 RGB） | ✅ |
| **适合 / 100% 缩放 + 滚轮缩放 + 拖拽平移**（Ctrl+0/1、双击） | ✅ |
| **冲印流程分组**（D042）：显影前（曝光/底片/DIR/颗粒/光晕）与显影后（相纸/扫描/反差/色域/眩光/扩散） | ✅ |
| 新增 `--couplers` / `--gamut` 开关并写进 LUT 配方签名 | ✅ |
| **胶片 / 相纸选择**（D043）：11 支彩色负片 × 6 种相纸，自动选 target_print | ✅ 实测 C200 + Crystal、Portra 800 + Endura |
| **可选物理光谱引擎**（D044）：`--engine spectral`，逐像素跑物理链，不查 LUT | ✅ 2.6 MP 单帧 1081 ms；视频 1259 ms/帧 |
| UI 输出栏“引擎：LUT / 光谱”，队列/分段两种都支持 | ✅ |
| **GPU 光谱探针**（D045）：D3D11 compute，2.6 MP × 81 样本 | ✅ **0.63 ms**（Arc A750，feature level 11_0） |
| **GPU 光谱引擎**（D046）：`--engine spectral-gpu` / UI“光谱 GPU（物理，快）” | ✅ 2.6 MP 单帧 **47–50 ms**、视频 **379 ms/帧**；与 CPU 参考 82–119 dB；无设备自动回退 |
| **GPU 质感**（D047）：`--texture-engine auto/cpu/gpu` / UI“GPU 质感”勾选框 | ✅ 视频 **382.7 → 245.6 ms/帧**；与 CPU 参考 143–166 dB；无设备自动回退 |
| 参考分析文档 `docs/ui-analysis-spektralab.md` | ✅ 含布局、令牌、组件与映射表 |
| 渲染完成询问是否打开文件夹 | ✅ |
| 关闭时保存路径/参数/窗口位置，下次恢复 | ✅ |
| `scripts\build-ui.cmd`：配置 → 编译 → windeployqt → 启动 | ✅ |
| CI 默认不建 UI（`SR_BUILD_QT_UI=OFF`），run 39 双平台绿 | ✅ |
| 截图 `docs/ui-preview.png`、`docs/ui-queue.png` | ✅ |

界面原则：**UI 不复制颜色/质感逻辑**，预览和渲染都调用 `spektra-roll-cli`；Qt 动态链接并随 exe 部署，符合 GPL/LGPL。

---

## 二、关键决策（完整台账见 `DECISIONS.md`）

| 编号 | 决策 | 结论 | 后果 / 影响 |
|---|---|---|---|
| **D001** | 第一个可运行成果 | **Windows 命令行先跑通** | Windows 先闭环；**后来确认有 MacBook Air M1**，macOS 本机验收可以做（`scripts\verify-macos.command`） |
| **D002** | 代码托管 | **现在建公开 GitHub 仓库** | 公开仓库的 macOS CI runner 免费，正好补上没有 Mac 的缺口 |
| **D003** | 许可证 | **GPL-3.0-or-later** | 不能上 Mac App Store；OFX 插件也必须 GPL；profile 数据仍 CC BY-SA 4.0 |
| **D004** | 图像 I/O（M0） | **stb_image / stb_image_write（8bit）** | M0 只能 8bit；M1 已按 D017 补到 16-bit PNG，EXR 仍欠着 |
| **D005** | 颜色变换实现形态 | **颜色烘成 3D LUT（33³ float32、四面体插值），空间效果走 GPU pass** | 视频能实时的前提；LUT 分辨率低于 33³ 会在暗部出色带 |
| **D006** | 构建系统 | **CMake ≥3.20 + Ninja + C++20** | Windows 用 MSVC，macOS 用 Clang，CI 两边一致 |
| **D007** | 产品名 / 仓库名 | **SpektraRoll**（你已拍板，正式生效） | 二进制 `spektra-roll-cli`，命名空间 `sr`，头文件 `sr/` |
| **D008** | 第一支胶片 / 相纸 | **Kodak Portra 400 + Kodak Portra Endura**（你已拍板） | M1 只对这一对负责；换胶片只需换 profile JSON |
| **D012** | 光谱上采样 | **Mallett 2019**（81×3 基函数） | 可移植、可复现；hanatos2025 往后放 |
| **D013** | LUT 定义域 | **display-encoded sRGB 进 → 出** | `lut apply` 直接吃普通 PNG；不能直接给线性/ACES 工作流 |
| **D014** | 输出色域压缩 | **M1 关闭** | 极端颜色会出负值/超 1，写图时钳制；M2 再做 CAM16-UCS |
| **D015** | 耦合剂 | **M1 关闭** | 先保证主链正确；couplers 进 M2 |
| **D016** | 印片中性滤色片 | 查上游数据库 **(0, 51.568, 52.534)** | 通用值 (0,65,55) 会偏绿，已排除 |
| **D017** | 16-bit I/O | **stb 读 + 自写 16-bit PNG** | OpenImageIO/EXR 推迟；16-bit 输出仅 `.png` |
| **D018** | parity 验收 | **PSNR ≥ 60 dB + 报告 ΔE76** | 实测 88.07 dB / ΔE76 max 0.0065 |
| **D019** | CI 覆盖 M1 | **Windows + macOS 都烘 LUT 并和参考对比** | 光谱链退化会在 CI 直接变红 |
| **D020** | M2 顺序 | **先色彩（真实照片 + Apple Log/HDR）→ 再视频编解码** | M2 颜色、M3 视频、M4 质感与性能、M5 界面 |
| **D021** | Apple Log / HDR | **Apple Log 输入 → 线性宽色域 → 胶片模型 → SDR 或 HDR(PQ/HLG) 输出** | HDR 曝光映射做成参数；动态元数据不做 |
| **D022** | 视频读写形态 | **独立程序 + FFmpeg 子进程**（你选的） | `spektra-roll-cli video` 直接吃 .mov；不把 FFmpeg 链进 exe |
| **D023** | Apple Log 输入落地 | **LUT 定义域改为 Apple Log 码值，内部解码到线性 BT.2020 → sRGB** | 与 colour-science 逐点一致（≤1 LSB）；负值暂时钳制 |
| **D024** | 扫描负片 | **反相 + 去片基 + D50 扫描 + 灰点中性化** | 天空 0.48–0.70、暗部死黑 7.7%→2.5%；无上游 parity |
| **D025** | 出片路径 | **负片为推荐路径，印片保留对比；反差做成 `--contrast`** | 1.0 保留余量，1.15 稍加反差，暗部比例随反差上升 |
| **D026** | 负片默认反差 | **1.15**（你确认） | CLI 默认 1.15；`--contrast 1.0` 拿回全余量；库默认仍是 1.0 |
| **D027** | 输出色域压缩 | **OkLCh 色度压缩，膝点 0.8/1.0/2.0** | Apple Log 默认开（负值 3,763→0）；sRGB 默认关保 M1 parity |
| **D028** | DIR 耦合剂 | **自层 + 层间抑制矩阵，Apple Log 默认开** | 与上游 PSNR 80.75 dB；饱和色增强，中性灰不变 |
| **D029** | HDR 输出 | **PQ / HLG 末端编码，白色 = `--peak` 尼特（默认 1000）** | 1.0 → PQ 0.75183；Air 是 SDR 屏，验收要靠 iPhone |
| **D031** | 帧区间 / ProRes | **`--start/--frames/--duration` + `--codec prores`（422 HQ）** | 音画同步；ProRes 120 MB / 4.1 s |
| **D032** | 多线程（提前） | **`parallelChunks` + `--threads`，0=自动** | 套 LUT 96→17 ms；视频 268→214 ms/帧（瓶颈转编码） |
| **D033** | 胶片质感 | **确定性颗粒 + 暖色光晕，逐帧叠加（不进 LUT）** | 同 seed/帧可复现；视频 +97 ms/帧；光晕半径仍是画幅比例 |
| **D034** | 颗粒调轻 | **模糊半径 = round(size)，demo 默认 0.4 / 0.45** | 噪声 std 0.014→0.0053，颗粒成团不毛躁 |
| **D035** | 眩光 + 扩散 | **screen 混合的宽模糊眩光 + 亮度差扩散** | 视频 311→486 ms/帧；模糊全分辨率，待降采样优化 |
| **D036** | 颗粒时间连续 | **两场交叉淡化，`--grain-rate 3`** | 帧间差 0.0025→0.0014；颗粒计算约翻倍 |
| **D037** | 模糊降采样 + 编码参数 | **宽模糊降 2–4 倍；`--crf` / `--preset`** | 579→469 ms/帧；瓶颈转 CPU 争用 |
| **D038** | 硬件编码 | **`--encoder qsv`**（hevc_qsv / h264_qsv） | 2.6 MP 下 200 vs 214 ms，收益有限；瓶颈在管道 |
| **D039** | 颗粒场缓存 | **LRU 保留最近两个场，按 seed/尺寸/半径/序号复用** | 全质感 476.5→358.7 ms/帧；输出与无缓存逐位一致 |
| **D040** | 分段渲染 / 续渲 / 时间码 | **`--segment` 写视频-only 分段，`--resume` 复用带标记的段，音频最后复制一次，timecode 按帧偏移** | 中断只重渲缺失段；99 帧输出保留 15:38:35:00，分段边界无跳变 |
| **D041** | 界面 v2 结构 | **参照 SpektraLab：三栏 + 顶栏、SpektraLab 灰/琥珀令牌、Section + SliderRow** | 画布成为最大区域；不复制其源码/图标；分析见 `docs/ui-analysis-spektralab.md` |
| **D042** | 冲印流程分组 | **右栏改“显影前 / 显影后 / 队列 / 日志”；底片参数与印片/扫描参数分开；暴露 DIR 耦合剂与色域压缩** | 参数顺序与电影流程一致；LUT 仍是单张烘焙，不做运行时两段渲染 |
| **D043** | 胶片 / 相纸选择 | **导入上游全部 profile；界面列出 11 支彩色负片 × 6 种相纸，自动选 target_print** | 界面可用组合从 1 对变成 66 对；反转片/T 型/电影印片暂不列出 |
| **D044** | 可选物理光谱引擎 | **`--engine spectral`：逐像素跑 FilmModel，不走 LUT；UI 引擎可选** | 2.6 MP 单帧 1081 ms，视频 1259 ms/帧；短片可全物理，长片仍用 LUT |
| **D045** | GPU 光谱可行性 | **D3D11 探针：2.6 MP 的 81 样本扫描 0.63 ms；下一步移植完整链** | Arc A750 潜力足够；GPU 路径 Windows-only，CPU 回退保留 |
| **D046** | GPU 光谱引擎 | **`--engine spectral-gpu`：HLSL 逐行翻译 CPU 参考链（D3D11 compute），常量缓冲一次上传，失败/无设备自动回退 CPU 光谱** | 2.6 MP 单帧 47–50 ms、视频 379 ms/帧；与 CPU 参考 82–119 dB，CI 有 `gpu_film` parity 测试；UI 列出“光谱 GPU” |
| **D047** | 空间质感上 GPU | **`--texture-engine auto/cpu/gpu`：halation / glare / diffusion / grain 逐 pass 翻译成 D3D11 compute；模糊保持 CPU 的累加顺序，自动回退** | 视频 382.7 → 245.6 ms/帧（1.56×）；质感本身约 160 → 38 ms；与 CPU 参考 143–166 dB；`test_gpu_texture` 九组配置 |
| **D048** | 预览实时化 | **`preview` 常驻进程：整段解码进内存（≤1024 MB、最长边 1600），`frame` 只跑颜色 + 质感并输出 8-bit RGBA；UI 拖动 90 ms 合并** | 预览 **1.7–2.2 s → 47–64 ms/帧**；首次打开 2.3–2.7 s；`ffprobe -show_streams` 一次取全（rotation 查询从 3.8 s 降到 42 ms） |
| **D049** | 融合 GPU 提交 | **颜色与质感共用一个 D3D11 设备：film 结果留在显存，质感以 SRV 直接读，只上传/回读一次** | 视频 233.6 → **207.1 ms/帧**；与分段路径 **逐位相同**；`test_gpu_chain` 与 CPU 参考 124 dB |
| **D050** | 视频管道修复 | **RGB48 量化去掉 `std::lround`（130 ms/帧），两个整帧循环并行；硬件解码实测更慢，不采用** | 视频 207.1 → **85–92 ms/帧**（2.4×）；`demo-m3` 358.7 → 245.5；输出逐位相同 |
| **D051** | 读写线程重叠 | **解码双缓冲 + 编码两槽位后台线程，读写不再阻塞 GPU 处理** | 视频 85–92 → **79.2 ms/帧**；`demo-m3` 226.8；分段/续渲不变；输出逐位相同 |
| **D052** | 并行分段渲染 | **`--jobs N`：每 worker 独立 GPU 对象/颗粒缓存，原子计数器领段；默认 min(3, 核数/4, 段数)** | CPU 路径 30 秒素材 **157.98 → 84.19 s（1.88×）**；GPU 路径 1.10×（单卡）；输出逐位相同 |
| **D053** | 长片预览代理 | **整段进内存会把最长边压到 960 px 以下时，生成 960 px 全 I 帧代理（`out/ui-proxy/`），按需取帧；失败回退内存缓存** | 30 秒素材 960×540 **100 ms/帧**（原先约 640 px）；重开 48 ms；4 秒素材不变 |
| **D054** | 真实点扩散核 | **光晕/眩光/扩散改用底片微米 + 双高斯重尾核（窄芯 + 宽裙），按 `--frame-width-mm` 换算；CPU 生成权重、GPU structured buffer 上传；`--spread-model box` 保留旧盒状模糊** | 同帧 box vs physical **57.03 dB**；GPU 视频 **82.2 ms/帧**；LUT+CPU 质感 +8.5%；parity **149–178 dB**；`test_texture` 新增物理核检查 |
| **D055** | 浮点交付 | **自带最小 OpenEXR（scanline、NONE、RGB、float/half）；`.exr` 即浮点且不钳制；支持 EXR 读入与 `--out-ext exr` 序列；`--bits 32` 配 PNG 报错** | float 30 MB / half 15 MB（2.6 MP）；读回 **PSNR = inf**；+3 EV 时 33% 像素 >1 保留；CI 双平台 EXR 冒烟 |
| **D056** | macOS GPU 颜色 | **Metal 移植光谱链（运行时编译，shared buffer）；无设备回退 CPU；质感留到 D057** | macOS CI `gpu_film` 真跑并通过；**fast math 必须关**，否则 NaN 保护被优化掉、整帧 NaN |
| **D057** | macOS GPU 质感 | **14 个 pass 翻成 MSL；一个 command buffer + `setBytes` 内联参数/taps；融合路径借用 film 的 MTLBuffer** | macOS CI 9/9（`gpu_texture` 2.11 s、`gpu_chain` 2.10 s）；真机逐位相同、融合 134.8 dB |
| **D058** | 线性浮点交付 | **`--exr-linear` + `--exr-trc srgb/pq/hlg` + `--peak`；1.0 = 白/峰值；不钳制；不改默认** | display 0.016..0.887 → linear 0.0012..0.762；+3 EV 线性最大 50.3 |
| **D059** | EXR 压缩 | **默认 ZIP（16 行/chunk），`--exr-raw` 关闭；stb zlib；压不动回退原始块；读取器支持 NONE+ZIP** | 已调色帧 −28%、带颗粒 ≈0%、half −38%；ffmpeg 交叉验证 92.6 dB |
| **D060** | 多相机 Log | **S-Log3 / C-Log3 / V-Log 官方曲线 + 原生 gamut 矩阵；CPU/GPU 同一套；UI 可选** | 曲线与 colour-science 完全一致；CPU vs D3D11 **110.67 dB** |
| **D061** | 更多 Log + 容器矩阵 | **追加 LogC3 / LogC4 / BMD Gen5 / DaVinci / S-Gamut3；未知 `inputMode` 回退 sRGB；`--in-matrix auto` 读素材 `color_space`** | 101 点曲线 0 差；CPU vs D3D11 118–120 dB；错误矩阵实测只有 44 dB |
| **D062** | OpenColorIO | **可选依赖 `SR_WITH_OCIO`（找不到就编 stub）；胶片模型只吃线性 Rec.709；解码烘进 LUT；GPU 回退 CPU** | OCIO sRGB 118.24 dB、Apple Log 93.60 dB；LUT 烘焙后不再依赖 OCIO |
| **D009** | Windows / 桌面 UI 技术栈 | **Qt 6（Qt Widgets）**（你已拍板） | 不装 MSVC，用 Qt 官方 MinGW；同一套 UI 以后可编到 macOS；动态链接 Qt 与 GPL-3.0 兼容 |
| **待定 D010** | 是否做 OFX 插件 | 建议**做，放第二期** | 尚未确认 |
| **待定 D011** | Apple Developer 什么时候买 | 建议**等有 Mac 再说** | $99/年，现在不花 |

### 还有一些"我代你做的技术决定"（也记录在案）

- 图像读写先用 stb 而不是 OpenImageIO —— 为了 M0 零依赖、编译快
- 用 MinGW 而不是 MSVC —— 因为 winget 装 VS Build Tools 失败（msstore 源 `0x80072efd`）
- 用 `.cmd` 包装 `.ps1` —— 因为 PowerShell 执行策略禁止直接跑脚本
- 静态链接 `-static` —— 让用户拿到一个 exe 就能跑，不用找 DLL

---

## 三、未完成的待办

### 🔴 下一步（按建议顺序）

1. **UI 的 OCIO 下拉**：CLI 已能用（`ocio-info` / `lut bake --ocio-input`），界面还没暴露。
2. **光谱 GPU 的 OCIO**：现在给了 `--ocio-input` 会回退 CPU 光谱。
3. **PIZ / DWA 压缩**：颗粒重的素材 ZIP 收益接近 0。
4. **代理清理 / 4K 预览**、**macOS Qt 打包**。

> 已完成：D047 GPU 质感、D048 预览实时化、D049 融合提交、D050 管道修复、D051 读写重叠、D052 并行分段、D053 预览代理、D054 真实点扩散核、D055 浮点 EXR、D056 Metal 光谱、D057 Metal 质感、D058 线性 EXR、D059 EXR ZIP、D060 多相机 Log、D061 更多相机 Log + 容器矩阵、D062 OpenColorIO。

### 🟡 中期路线（M4–M5）

| 阶段 | 内容 |
|---|---|
| **M4** | ✅ 颗粒场缓存、✅ 分段/续渲/时间码、✅ GPU 颜色/质感、✅ 融合提交、✅ 并行分段、✅ 预览代理、✅ 真实点扩散核、✅ 浮点/线性 EXR、✅ EXR ZIP、✅ OpenColorIO（D062）；待做：PIZ/DWA |
| **M5** | ✅ Qt 6 桌面界面主体、✅ 预览实时化、✅ macOS Metal 光谱、✅ macOS Metal 质感；待做：UI 的 OCIO 下拉、OFX 插件、渲染农场命令行、macOS Qt 打包 |

### 🟢 还没开始、但必须记住的欠账

- [x] ~~macOS 本机验收~~ → 已在 MacBook Air M1 上跑通（编译、测试、LUT parity、视频路径）
- [x] ~~没有时间维度~~ → `video` 命令已能吃 `.mov`、带原声、支持帧区间
- [x] ~~没有确定性颗粒种子~~ → `--seed` + `--grain-rate`，同 seed/帧逐位可复现
- [x] ~~长片中断要重头~~ → `--segment` + `--resume`，只补缺失段
- [x] ~~输出丢时间码~~ → 自动复制源 timecode，`--timecode` / `--no-timecode` 可覆盖
- [x] ~~颜色只能在 CPU 上物理算~~ → `--engine spectral-gpu`（D046），2.6 MP 单帧 50 ms；无显卡自动回退
- [x] ~~空间质感还没上 GPU~~ → `--texture-engine gpu`（D047），视频 382.7 → 245.6 ms/帧；与 CPU 参考 143–166 dB
- [x] ~~颜色与质感仍是两次 GPU 提交~~ → D049 融合提交，207.1 ms/帧；D050/D051 修完管道到 **79.2 ms/帧**
- [ ] **CI 上没有真实 GPU** → `gpu_film` / `gpu_texture` / `gpu_chain` 在公有 runner 上会 skip，着色器退化只能在本机或真机 CI 发现
- [x] ~~**macOS 没有 GPU 物理路径**~~ → D056 Metal 光谱、D057 Metal 质感，两条链都在 GPU
- [x] ~~**没有色彩管理**~~ → D062 可选 OpenColorIO：任意相机/ACES 空间当 LUT 输入域、ACES 2.0 显示器变换；没有 OCIO 的构建行为不变
- [ ] **OCIO 还没进 UI / GPU** → 界面下拉与 `spectral-gpu` 的 OCIO 都还没有（GPU 会回退 CPU 光谱）
- [x] ~~**浮点 / EXR**~~ → D055 已完成（float / half EXR，读回逐位一致，不钳制）
- [x] ~~**空间效果不是物理核**~~ → D054 微米 + 双高斯重尾核；box 模型保留对照
- [x] ~~没有视频解码的硬件加速~~ → 实测 QSV 解码+下载 21.8 ms/帧 vs 软解 19.5 ms/帧，**不值得做**；QSV 编码在管道修好后有意义（H.264 77 vs 104 ms/帧）
- [x] ~~长片预览只能缩成缩略图~~ → D053 960 px 全 I 帧代理，30 秒素材 960×540 · 100 ms/帧
- [ ] **Windows 代码签名** → 可选，Azure Trusted Signing 约 $10/月
- [ ] **法律风险未处理** → 上游 README 声称"直接受其方法启发的作品也算衍生"，闭源前必须先谈双授权
- [ ] **M2 遗留**：hanatos2025 上采样、1D shaper LUT、Dolby Vision 元数据

---

## 四、环境与运维备忘（重要，别重复踩坑）

### 本机工具链位置

```
CMake   C:\Program Files\CMake\bin\cmake.exe      (4.4.4)
Ninja   X:\agent\tools\ninja\ninja.exe            (1.13.2)
GCC     X:\agent\tools\mingw64\bin\g++.exe        (16.2.0 ucrt posix-seh)
ffmpeg  J:\ffmpeg\bin\ffmpeg.exe                  M2 起用来从 .mov 抽帧验收
Qt      X:\agent\tools\qt\6.8.3\mingw_64          Qt 6.8.3 (Widgets)
Qt GCC  X:\agent\tools\qt\Tools\mingw1310_64      13.1.0，只给 UI 构建用
OCIO    X:\agent\tools\ocio                      OpenColorIO 2.4.2 静态前缀（tools\ocio\build-ocio.cmd 可重建）
```

### ⚠️ 几个已知陷阱

1. **`X:\agent\tools\mingw64\bin` 里有 `cmake.exe` 和 `ninja.exe` 会"抢名字"**
   → 脚本里必须写绝对路径 `C:\Program Files\CMake\bin\cmake.exe` 和 `X:\agent\tools\ninja\ninja.exe`，不能靠 `Get-Command cmake`。

2. **PowerShell 执行策略禁止跑 `.ps1`**
   → 所有给用户的入口都是 `.cmd`，内部用 `powershell -NoProfile -ExecutionPolicy Bypass -File`。不要直接叫用户跑 `.ps1`。

3. **MinGW 编译的 exe 默认依赖运行时 DLL，直接双击会报 0xc0000135**
   → CLI 和测试目标已经加 `-static`；**Qt UI 例外**，它必须动态链接 Qt，靠 `windeployqt` 把 DLL 放到 exe 旁边。

4. **ffprobe 7+ 遇到 timecode 轨会把同一个 stream 打印两遍**（`stream_groups` 和 `streams` 各一次）
   → 所有解析 ffprobe 文本输出的地方都要只取第一行（`firstLine()`），CI 里的断言也要 `| head -n 1`。这是 2026-11-02 CI 变红的直接原因。

5. **Qt 自带的 MinGW 需要把它的 `bin` 放进 PATH**
   → `g++.exe` 本身能启动，但它调用的 `cc1plus.exe` 在 libexec 里，找不到同目录的 `libstdc++-6.dll` 会静默返回 0xC0000135。`build-ui.ps1` 已经先 `$env:PATH = "$mingw\bin;$qt\bin;..."`。

6. **macOS 的 libc++ 比 libstdc++ 严**
   → `std::filesystem::file_time_type::count()` 比 `long long` 宽，直接 `std::to_string` 会重载歧义；先 `static_cast<long long>`。CI 现在会在构建失败时把 `error:` 行打成 annotation。

7. **调试开关与环境**
   → `SPEKTRA_VIDEO_DEBUG=1` 打印视频分段耗时；`SPEKTRA_PREVIEW_DEBUG=1` 打印预览服务端的 look/open/frame 耗时；预览代理在 `out/ui-proxy/`（可随时删，会按素材重建）。

8. **OpenColorIO 只在这台机器是自建的**（D062）
   → 前缀 `X:\agent\tools\ocio`（静态、自包含），用 `tools\ocio\build-ocio.cmd` 重建；两个 MinGW 补丁在 `tools/ocio/mingw.patch` + 构建时给 yaml-cpp 补 `<cstdint>`。CI 的 Windows 不装 OCIO（跑 stub），macOS 用 Homebrew 的包。
   → **UI 的构建参数是 `-DSR_WITH_OCIO=OFF`**：Qt 的 GCC 13.1 连不上 GCC 16.2 编的 OCIO 静态库。别"顺手"打开。
   → 改 OCIO 相关代码后至少跑一次 `.\build\spektra-roll-cli.exe ocio-info`，确认自动选中的是 `Linear Rec.709 (sRGB)`。

### 标准构建命令（脚本已经封装，这是底层等价写法）

```powershell
cd X:\agent\SpektraRoll
cmake -S . -B build -G Ninja `
  -DCMAKE_MAKE_PROGRAM=X:\agent\tools\ninja\ninja.exe `
  -DCMAKE_CXX_COMPILER=X:\agent\tools\mingw64\bin\g++.exe `
  -DCMAKE_BUILD_TYPE=Release
# OpenColorIO: cmake 自动找 find_package 或 X:/agent/tools/ocio；
# 想强制关掉就加 -DSR_WITH_OCIO=OFF（会编 stub，功能不变）
cmake --build build
ctest --test-dir build --output-on-failure
```

### 你（产品负责人）只需要双击的文件

```
scripts\dev-install.cmd         装工具链（只做一次，不需要管理员权限）
scripts\build.cmd               编译 + 跑测试
scripts\demo-m1.cmd             M1 胶片样张（前后对比）
scripts\demo-m2.cmd             Apple Log → 印片 / 负片 / HDR 片段
scripts\demo-m3.cmd             胶片质感（干净帧 / 质感帧 / 整段视频）
scripts\demo-m4.cmd             分段渲染 + 模拟崩溃 + 断点续渲 + timecode
scripts\demo-gpu-texture.cmd     GPU 质感对比（CPU vs GPU 的 ms/帧与 PSNR）
scripts\demo-spread.cmd          空间扩散对比（旧盒状模糊 vs 新物理核，并排图 + ms/帧）
scripts\demo-exr.cmd             浮点交付（PNG16 / float EXR / half EXR + 读回比对）
scripts\demo-log-inputs.cmd      九种相机 Log 输入（各烘一张 LUT + GPU/CPU 对照表）
scripts\demo-ocio.cmd            OpenColorIO（配置信息 / Apple Log 双实现 / ACES 显示器变换 / LUT 对比）
scripts\build-ui.cmd            编译 Qt 6 界面、部署 DLL 并启动（首次约 1–2 分钟）
scripts\verify-m1.cmd           一键验收：编译 + 测试 + 烘 LUT + 和参考对比
scripts\demo.cmd                M0 的合成 LUT 冒烟测试（不是胶片）
scripts\push.cmd                登录 GitHub 并推送
```

macOS 上双击 `scripts\verify-macos.command`（需要 `xcode-select --install` 和 `brew install cmake ffmpeg`）。

### 验证是否还健康

```powershell
cd X:\agent\SpektraRoll
.\build\spektra-roll-cli.exe version        # 应输出 spektra-roll-cli 0.1.0
ctest --test-dir build --output-on-failure # 应 10/10 通过（gpu_* 无显卡时 skip；ocio 无库时 skip）
.\build\spektra-roll-cli.exe lut bake --out out\check.cube
.\build\spektra-roll-cli.exe lut compare --a tests\reference\portra400_endura_33.cube --b out\check.cube
# 应输出 PSNR ≈ 88 dB, PASS
.\build\spektra-roll-cli.exe ocio-info --ocio-config data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio
# 有 OCIO 时应打印 "OpenColorIO: 2.4.2" 和 "linear space: Linear Rec.709 (sRGB)"；
# 没有 OCIO 时应只报 "this build was compiled without OpenColorIO"
.\build-ui\spektra-roll-ui.exe --preview-selftest   # 第二次预览应 < 100 ms
```

### 视频命令（M3 核心）

```powershell
# 整段：解码 → 套 LUT → 带原声重新编码
.\build\spektra-roll-cli.exe video --lut out\m3-applelog-sdr.cube `
    --in X:\agent\footage\C0113.MP4 --out out\graded.mov `
    --trc srgb --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1 `
    --ffmpeg J:\ffmpeg\bin\ffmpeg.exe
```

- 没有 `--ffmpeg` 时用 PATH 里的 ffmpeg，也可以用环境变量 `SPEKTRA_FFMPEG`
- `--trc pq|hlg` 出 HDR；`--codec prores` 出 ProRes 422 HQ
- 帧区间：`--start 00:00:12.5 --frames 240`（或 `--duration 10`）
- 长片：`--segment 10 --resume` 分成 10 秒一段；中断后重跑同一条命令只补缺失段
- 时间码：默认复制源 `timecode`；`--timecode 01:00:00:00` 覆盖，`--no-timecode` 关闭

### parity 工具链（只给 AI / 技术朋友）

```
X:\agent\tools\python\python.exe                          Python 3.12.8 便携版
X:\agent\_upstream_spektrafilm\src\spektrafilm\            上游源码（depth 1）
tools\parity\reference_model.py                            参考链 + 表格导出
tools\parity\check_pipeline.py                             参考链 vs 真实上游 pipeline
tools\parity\README.md                                     怎么跑、当前数字
```

Python 环境里已装：numpy、scipy、colour-science 0.4.7、opt_einsum、numba、matplotlib、scikit-image。
导入上游时要 stub 掉 exiv2 / lensfunpy / rawpy / OpenImageIO，`reference_model.py` 已经处理好。

### 素材（Apple Log / ProRes）

```
X:\agent\footage\C0113.MP4    已收到（仓库外，不进 git）
```

规格（ffprobe）：iPhone 16 Pro + Blackmagic Cam，HEVC Rext 4:2:2 10-bit，
1214x2160 竖拍（rotation -90，显示为 2160x1214），BT.2020 非恒定亮度，
transfer = unknown（即 Apple Log），24 fps，4.125 s，99 帧。

`video` 命令会自动处理这个旋转（ffprobe 读 side_data rotation 后交换宽高）。
抽单帧时仍然要带 `scale=in_color_matrix=bt2020:in_range=tv:out_range=full`，
`scripts\demo-m2.ps1` 已经封装。

---

## 五、仓库结构速查

```
X:\agent\SpektraRoll\
├── src/core/include/sr/     公开头文件（lut3d, image, json, profile, film_model, metrics, transfer, gamut, texture, parallel, spectral_data, ocio）
├── src/core/src/            实现 + 生成的 spectral_tables.cpp + ocio.cpp / ocio_stub.cpp
├── src/cli/main.cpp         命令行入口（gen / convert / video / preview / lut bake|apply|compare|info / version）
├── src/cli/gpu_film.{hpp,cpp}  D3D11 光谱后端（D046），非 Windows 编译成回退占位实现
├── src/cli/gpu_texture.{hpp,cpp} D3D11 质感后端（D047），非 Windows 编译成回退占位实现
├── src/ui-qt/main.cpp       Qt 6 桌面界面（D009），调用 CLI，不复制管线
├── tests/                   9 个测试程序 + reference/ 冻结参考 LUT（三个 GPU 测试无显卡时 skip）
├── data/profiles/           上游 profile JSON（CC BY-SA 4.0）+ NOTICE.md
├── data/ocio/               ACES 2.0 studio 配置（Apache-2.0，D062）+ NOTICE.md
├── tools/parity/            Python 参考链、上游交叉检查、说明
├── tools/ocio/              MinGW 编 OpenColorIO 的脚本 + 补丁（build-ocio.cmd）
├── third_party/stb/         stb_image / stb_image_write（单头文件）
├── scripts/                 .ps1 + .cmd 双份，.cmd 给用户双击
├── .github/workflows/       build.yml：Windows + macOS 矩阵
├── docs/architecture.md     目标架构、M1 链路、确定性规则、M0–M5 路线图
├── docs/ui-analysis-spektralab.md  SpektraLab 界面组成分析与映射
├── docs/licensing.md        GPL / CC BY-SA / LUT / 商标 / 编解码专利约束
├── README.md                对外介绍
├── RUN.md                   给非程序员的操作手册
├── STATUS.md                一页纸进度
├── DECISIONS.md             决策台账 D001–D061
├── progress.md              本文件
└── LICENSE                  GPL-3.0
```

---

## 六、下游 AI 的“别做的事”清单

- ❌ **不要推翻 D001–D061**，那些是已经执行并验收/记录过的
- ❌ **不要问用户技术细节问题**，只能问“选 A 还是 B”
- ❌ **不要在没跑 `verify-m1.cmd` 的情况下改光谱链**，改完 PSNR 掉了要立刻知道
- ❌ **不要把 display-encoded sRGB 的 LUT 说成线性/ACES LUT**（D013）
- ❌ **不要以为写了 EXR 就是线性素材**：D055 的 EXR 存的是 display-encoded 值（sRGB / PQ 码值），只是没有钳制；进调色软件要设对应输入变换
- ❌ **不要在 EXR 路径上恢复 0..1 钳制**：浮点交付的意义就是把超白和负值留下来；PNG 才夹
- ❌ **不要静默把 `--bits 32` 降级**：`.png` 装不下浮点，必须报错让用户改成 `.exr`
- ❌ **不要把 EXR 常量当成“随便什么压缩都行”**：当前只写/读 NONE 压缩的 scanline；接 ZIP/PIZ 要同时改读和写并加测试
- ❌ **不要说“没有 Mac”**，产品负责人有 MacBook Air M1；macOS 相关先让他双击 `scripts\verify-macos.command` 拿结果
- ❌ **不要只在一个平台验证改动的路径**（issue #1 的教训）：CI 的 macOS 视频冒烟现在开着，别关掉
- ❌ **不要把颗粒的 `--grain-rate` 默认改回 1**，那是数字沸腾（D036）
- ❌ **不要在逐帧循环里绕过 `GrainFieldCache` 直接调 `applyGrain`**，那会把每帧两个噪声场重新算一遍（D039）
- ❌ **不要在分段渲染里逐段编码音频**：音频只从源文件复制一次（D040），否则 AAC 接缝和 A/V 漂移会回来
- ❌ **不要把分段里的颗粒帧号改回每段从 0 开始**，那会在段边界闪一下（D040）
- ❌ **不要在没有基准数字的情况下改质感默认值**，颗粒/眩光都是按你的反馈调过的（D034/D036）
- ❌ **不要在 Qt UI 里重新实现颜色/质感管线**：预览和渲染都调 `spektra-roll-cli`，否则 UI 和 CI 会漂移（D009）
- ❌ **不要把 Qt 静态链接或只发一个 exe**：Qt 必须动态链接并用 `windeployqt` 带 DLL（LGPL 合规）
- ❌ **不要把 `X:\agent\tools\qt` 提交进仓库**：它只在本机工具链里
- ❌ **不要直接把 SpektraLab 的 QML 源码/图标搬进来**：只借鉴布局和令牌，来源与许可见 `docs/ui-analysis-spektralab.md`（D041）
- ❌ **不要把参数再堆回左栏**：左栏是材料，参数在右栏；画布必须保持最大
- ❌ **不要偷懒改品牌**，SpektraRoll 已定案，改一次要动二进制名/CI/文档
- ❌ **不要删掉 GPU 着色器里的 `density != density` 判空**：有几条波长的 film density 是 NaN，CPU 参考同样跳过它们；少这个保护整条光谱和会变 NaN，输出直接全黑（D046）
- ❌ **不要把 `--engine spectral-gpu` 改成硬失败**：没有 D3D11 设备时必须回退 CPU 光谱，macOS 才能继续用（D045/D046）
- ❌ **不要在 CLI / UI 里另写一份质感顺序**：`sr::applyTexture()` 是唯一 CPU 参考链，GPU parity 测试也拿它当基准（D047）
- ❌ **不要把盒状模糊改成并行前缀和或换累加顺序**：GPU 现在逐位对齐 CPU 靠的就是“一行一列顺序累加”，换了就过不了 parity（D047；物理核时代实测 149–178 dB，见 D054）
- ❌ **不要把 `--texture-engine gpu` 改成硬失败**：编译失败、无设备、运行时错误都要回退 CPU 并只 warning 一次（D047）
- ❌ **不要在 Metal 里用默认的 fast math**：`MTLCompileOptions.fastMathEnabled` 默认 YES，会把 `density != density` 的 NaN 保护优化掉，光谱里几条 NaN 波长直接让整帧变 NaN（D056）；D3D11 复现不了
- ❌ **不要把 D3D 的 group 数直接传给 Metal 的 `dispatchThreads`**：它收的是总线程数，会少派 64 倍的线程；用 `dispatchThreadgroups:` 或自己乘好（D057）
- ❌ **不要在同一个 command buffer 里用共享缓冲传 PSF taps**：链内后一个效果会覆盖前一个的权重；taps 和参数都要用 `setBytes` 内联（D057）
- ❌ **不要在融合路径里把 film 的 MTLBuffer 当作 ping-pong 的写目标**：第一遍之后必须换回自己的缓冲，否则覆盖 film 结果（D057）
- ❌ **不要把 Metal 的 `Params` 改成 `float4` 数组**：现在用 `float[4]` 是为了和 C++ 结构逐字节一致；换 `float4` 会引入 16 字节对齐规则，错位后整帧都是垃圾（D056）
- ❌ **不要在 GPU 质感里省略颗粒的两场交叉淡化或 `norm`**：那是 D036 防闪烁的根，改了颗粒会重新沸腾（D047）
- ❌ **不要把 `--texture-engine` 漏出分段签名**：否则 CPU/GPU 质感的分段会被混着续渲（D047）
- ❌ **不要在 UI 预览里恢复“每帧一个 ffmpeg + 一个 CLI 进程”**：那条链 1.7–2.2 s/帧，现在由 `preview` 常驻进程 + 整段缓存接管（D048）
- ❌ **不要用 `ffprobe -show_entries side_data=rotation` 做几何探测**：这支素材要 3.8 s；一次 `-show_streams` 42 ms 就够（D048）
- ❌ **不要在拖动时间线时走单帧慢路径**：超出缓存的帧在拖动中直接跳过，松手才允许回退（D048）
- ❌ **不要把预览缓存设成无上限**：默认 1024 MB / 最长边 1600，长片会吃掉内存（D048）
- ❌ **不要把 `std::filesystem::file_time_type::count()` 直接塞进 `std::to_string`**：libc++ 的 rep 比 `long long` 宽，重载会歧义，只有 macOS 编译失败（D053）；先 `static_cast<long long>`
- ❌ **不要把 PSF 的 taps 塞进 D3D11 常量缓冲**：HLSL 的常量缓冲数组是 **16 字节/元素**，按 4 字节排会读到错位权重（parity 掉到 19–48 dB）；用 structured buffer + `kernelRadius`（D054）
- ❌ **不要在 HLSL 里重新算 `exp` 生成核权重**：CPU `sr::buildSpreadKernel` 是唯一来源，GPU 必须上传同一组 float 才能过 150 dB 的 parity（D054）
- ❌ **不要把光晕/眩光/扩散的尺寸改回"画幅比例"**：现在按底片微米算，`--frame-width-mm` 决定换算；改回比例会让 1080p 和 4K 观感不一致（D054）
- ❌ **不要删掉 `--spread-model box`**：那是旧观感的回退路径，也是新旧核对比的唯一途径（D054）
- ❌ **不要在物理核的测试里用 64 px 小图**：225 µm 在 64 px 宽上只有 0.4 px，等于没效果；按 `width / frameWidthMm` 选尺寸（640 px ≈ 4 px σ）（D054）
- ❌ **不要往 `InputTransfer` 中间插新枚举值**：`inputMode` 是直接 cast 成 int 传给着色器的，新值只能追加在末尾，否则 0–4 的语义全乱（D061）
- ❌ **不要让着色器里的未知 `inputMode` 落到最后一个分支**：D060 的最后一段默认是 V-Log，加输入后会静默用错曲线；现在显式回退 sRGB（D061）
- ❌ **不要按"输入类型"猜 YUV → RGB 矩阵**：同一种 Log 可能在 bt709 或 bt2020 容器里，猜错同帧只有 44 dB；跟素材的 `color_space` 标签走（`--in-matrix auto`）（D061）
- ❌ **不要在分段签名里漏掉 `input` 和 `matrix`**：`--engine spectral` 换输入后 `--resume` 会把两种观感混在一支片子里（D061）
- ❌ **不要把 OpenColorIO 变成硬依赖**：编不到时必须编 stub，CLI 照常工作，只有 `--ocio-*` 报错（D062）
- ❌ **不要省掉 OCIO 解码后的负值钳制**：不钳制时 (1,1,0) 这类越界色会让密度曲线跑飞，LUT 对比从 63.89 掉到 **9.84 dB**（D062）
- ❌ **不要在光谱 GPU 上假装支持 OCIO**：着色器里没有 OCIO，必须 warning 后回退 CPU 光谱；LUT 引擎下用 `--ocio-*` 要报错并提示改用 `lut bake`（D062）
- ❌ **不要在 UI 构建里打开 `SR_WITH_OCIO`**：Qt 的 GCC 13.1 和 OCIO 的 GCC 16.2 静态库 libstdc++ 符号对不上（`std::istream::seekg(std::fpos<...>)`）（D062）
- ❌ **不要改 `data/ocio/` 里的配置文件**：那是 ACES 官方 studio 配置原件（Apache-2.0），要换版本就整份替换并更新 NOTICE

---

## 七、下一次会话的理想开场

你说一句：

> “读 progress.md，继续。”

AI 就应该：

1. 先跑 `ctest --test-dir build --output-on-failure`：9 个测试，其中 `gpu_film` / `gpu_texture` / `gpu_chain` 无显卡时会 skip（正常）
2. 再跑 `scripts\verify-m1.cmd -NoOpen` 确认 M1 parity 还是 88.07 dB
3. 再跑 `scripts\demo-m3.cmd -NoOpen` 确认视频 + 质感链路没坏（约 40 秒）
4. 再跑 `scripts\demo-m4.cmd -NoOpen` 确认分段 + 续渲 + timecode（约 20 秒）
5. 跑 `scripts\build-ui.cmd -NoLaunch` 确认 Qt 界面还能编译部署
6. 颜色路径改过就补一次 GPU 校验：`lut apply --engine spectral` 与 `--engine spectral-gpu` 出图做 PSNR（应 ≥60 dB，本机实测 ≥82 dB）
7. 质感路径改过就跑 `scripts\demo-gpu-texture.cmd -NoOpen`：CPU vs GPU 两条 ms/帧、加速比与编码后 PSNR（物理核后本机约 2.9×、85.6 dB）
8. 空间扩散改过就跑 `scripts\demo-spread.cmd -NoOpen`：box vs physical 的 PSNR 与 ms/帧；GPU 视频应约 82 ms/帧
9. 浮点 / 图像 I/O 改过就跑 `scripts\demo-exr.cmd -NoOpen`：EXR 读回应与 PNG16 逐位一致（PSNR = inf），+3 EV 那行应显示约 33% 的像素 >1
10. 预览路径改过就跑 `.\build-ui\spektra-roll-ui.exe --preview-selftest`：第二次预览应 <100 ms（短片 47 ms；长片走 960 px 代理约 100 ms）
11. 视频管道改过就设 `SPEKTRA_VIDEO_DEBUG=1` 跑一次 2 秒渲染：应看到 `wait / convert / process / pack` 四项，总时长约 79–82 ms/帧
12. 分段改过就删掉 `.parts` 里的中间段再 `--resume` 一次，确认只补缺失段（并行下同样成立）
13. macOS / Metal 改过就在 Mac 上跑 `scripts/verify-macos.command`：`gpu_film` / `gpu_texture` / `gpu_chain` 都应真跑（不是 skip），第 5 步会打印设备信息、三组 parity 与 CPU vs Metal 的 PSNR；顺手跑 `scripts/bench-macos.command` 记录 2.6 MP 的 CPU vs Metal
14. Log 输入改过就 `lut bake --input s-log3 --scan negative`，再用 `--engine spectral` 与 `--engine spectral-gpu` 出图比 PSNR（应 ≥60 dB，本机实测 118–120 dB）；九种输入一次跑 `scripts\demo-log-inputs.cmd -NoOpen`
15. OCIO 改过就跑 `.\build\spektra-roll-cli.exe ocio-info`（应自动选中 `Linear Rec.709 (sRGB)`）和 `scripts\demo-ocio.cmd -NoOpen`（Apple Log 93.6 dB、LogC3 LUT 63.9 dB）；没有 OCIO 的构建应只报“without OpenColorIO”
16. 看 §三 的"下一步"（当前最该做：UI 的 OCIO 下拉）

约束：每次改完都要重跑 `lut compare`（不能掉到 88 dB 以下）和 9 个测试程序。
