# SpektraRoll · 进度交接书

> **这份文件的用途**：让你在新开一个对话时，把这一份丢给 AI，它就能接着干，不需要你重新解释背景。
>
> **最后更新**：2026-11-02
> **当前里程碑**：**M3 完成**（视频读写、帧区间、ProRes、多线程、胶片质感）；下一步做颗粒缓存或 M4
> **项目**：SpektraRoll —— 面向**动态影像（视频）**的开源胶片与印片模拟器
> **仓库**：https://github.com/haichangjyue/SpektraRoll （公开，GPL-3.0-or-later）
> **本地目录**：`X:\agent\SpektraRoll`

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
```

---

## 零之一、本次会话（2026-11-02）做了什么

一次会话把项目从"M2 尾声"推到"M3 完成"，按时间顺序：

1. 修好 HDR 视频：原来 demo 是单帧循环 4 秒，改成 **99 帧整段 + 原声**
2. **M3 视频读写**（D022）：`spektra-roll-cli video` 直接吃 `.mov`，ffmpeg 解码 → 逐帧套 LUT → 重新编码，音画同步
3. **帧区间 + ProRes**（D031）：`--start/--frames/--duration`、`--codec prores`（422 HQ）
4. **macOS 修复**（issue #1）：POSIX `popen` 不接受 `"rb"/"wb"`，已修，CI 两个平台都跑视频冒烟
5. **多线程**（D032）：套 LUT 96 → 17 ms
6. **胶片质感**：颗粒 + 光晕（D033）→ 调轻（D034）→ 眩光 + 扩散（D035）→ 颗粒时间连续（D036）
7. **速度**：宽模糊降采样（D037）、`--crf/--preset`、`--encoder qsv`（D038）

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
| macOS `popen` 修复（issue #1） | ✅ CI 两平台都跑视频冒烟 |
| `scripts\demo-m3.cmd`（干净帧 / 质感帧 / 整段视频） | ✅ |

### 6. 改名与上线

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

### 7. 实测数据（本机，真实跑出来的）

| 项 | 结果 |
|---|---|
| CPU | Intel i5-10600KF |
| GPU | Intel Arc A750（支持 D3D12 / Vulkan） |
| 内存 | 32 GB |
| 编译器 | MinGW-w64 GCC 16.2.0（便携版，无管理员权限） |
| CMake | 4.4.4（`C:\Program Files\CMake\bin\cmake.exe`） |
| Ninja | 1.13.2（`X:\agent\tools\ninja\ninja.exe`） |
| 单元测试 | **6/6** 程序通过（LUT、胶片模型、图像 I/O、Apple Log/传输、色域、质感） |
| 33³ LUT 烘焙 | **约 108 ms**（多线程，含写文件）；单线程 198 ms |
| 2.6 MP 套 33³ LUT（16-bit） | **约 17 ms**（多线程）；单线程 96 ms |
| 视频渲染（2.6 MP，HEVC） | 无质感 **187 ms/帧**；全质感 **469 ms/帧**；4.1 s 素材约 46 秒 |
| 颗粒时间稳定性 | 帧间差 0.0025（rate 1）→ **0.0014**（rate 3） |
| C++ vs Python 参考 | **PSNR 88.07 dB**，max \\|diff\\| 4.57e-4，ΔE76 max 0.0065 |
| Apple Log 解码 vs colour-science | 2.6 MP 帧，max \\|diff\\| **1.53e-5**（= 16-bit 1 LSB），mean 1.9e-8 |
| Apple Log 素材 | 2160x1214，24p，HEVC 10-bit 4:2:2，BT.2020 |
| 扫描负片 vs 印片（同一帧） | 天空 0.48–0.70 / 0.88–0.91；暗部 <0.10 由 7.7% 降到 2.5% |
| 负片反差 1.0 / 1.15 / 1.3 | 天空 p5–p95 0.48–0.70 / 0.49–0.75 / 0.49–0.79；暗部 2.5% / 6.8% / 13.1% |
| 印片曝光因子 | 0.837773013976（上游含一次 sRGB→sRGB 往返，0.837787256300） |
| 产物 | `build\spektra-roll-cli.exe`，静态链接，无 DLL 依赖 |

**重要推论**：套 LUT 已经压到 17 ms/2.6 MP；视频剩下的时间主要在**质感计算（约 280 ms）和管道/格式转换**上，不在编码器。下一步提速的关键是**颗粒场缓存**（rate=3 时一个场可复用 3 帧）。

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
| **待定 D009** | Windows UI 技术栈 | 建议 **WinUI 3（C++/WinRT）** | 尚未确认；选定后需要装 MSVC（目前只有 MinGW） |
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

1. **颗粒场缓存**（性价比最高）：`--grain-rate 3` 时一个噪声场能用 3 帧，现在每帧都新建两个场。预期 469 → 约 350 ms/帧，整段 46 → 34 秒
2. **断点续渲 + 时间码**：分段渲染、中断后从上一段继续；输出带上原始 timecode
3. **界面**：需要先拍板 D009（WinUI 3 还是 Qt）
4. **真实点扩散核**：眩光 / 光晕 / 扩散现在是"画幅比例 + 盒状模糊"，应该换成微米级、随分辨率缩放的核
5. **浮点 / EXR**（D017 欠账）：16-bit PNG 之外还没有浮点交付

### 🟡 中期路线（M4–M5）

| 阶段 | 内容 |
|---|---|
| **M4** | 颗粒场缓存、真实扩散核、SIMD/GPU、浮点/EXR、断点续渲 |
| **M5** | Windows WinUI 3（**需要先装 MSVC**）、macOS SwiftUI、OFX 插件、渲染农场命令行 |

### 🟢 还没开始、但必须记住的欠账

- [x] ~~macOS 本机验收~~ → 已在 MacBook Air M1 上跑通（编译、6/6 测试、LUT parity、视频路径）
- [x] ~~没有时间维度~~ → `video` 命令已能吃 `.mov`、带原声、支持帧区间
- [x] ~~没有确定性颗粒种子~~ → `--seed` + `--grain-rate`，同 seed/帧逐位可复现
- [ ] **没有色彩管理** → 没有 OCIO、没有 ACES 输入变换；Apple Log 是手写解码，不是 OCIO 配置
- [ ] **浮点 / EXR** → 只有 16-bit PNG
- [ ] **空间效果不是物理核** → 光晕/眩光/扩散都用画幅比例，不是微米
- [ ] **没有视频解码的硬件加速** → 解码/编码都是软件，QSV 只接了编码且收益有限
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
```

### ⚠️ 三个已知陷阱

1. **`X:\agent\tools\mingw64\bin` 里有 `cmake.exe` 和 `ninja.exe` 会"抢名字"**
   → 脚本里必须写绝对路径 `C:\Program Files\CMake\bin\cmake.exe` 和 `X:\agent\tools\ninja\ninja.exe`，不能靠 `Get-Command cmake`。

2. **PowerShell 执行策略禁止跑 `.ps1`**
   → 所有给用户的入口都是 `.cmd`，内部用 `powershell -NoProfile -ExecutionPolicy Bypass -File`。不要直接叫用户跑 `.ps1`。

3. **MinGW 编译的 exe 默认依赖运行时 DLL，直接双击会报 0xc0000135**
   → CMake 里已经给所有可执行目标加了 `-static`，新加目标也要加。

### 标准构建命令（脚本已经封装，这是底层等价写法）

```powershell
cd X:\agent\SpektraRoll
cmake -S . -B build -G Ninja `
  -DCMAKE_MAKE_PROGRAM=X:\agent\tools\ninja\ninja.exe `
  -DCMAKE_CXX_COMPILER=X:\agent\tools\mingw64\bin\g++.exe `
  -DCMAKE_BUILD_TYPE=Release
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
scripts\verify-m1.cmd           一键验收：编译 + 测试 + 烘 LUT + 和参考对比
scripts\demo.cmd                M0 的合成 LUT 冒烟测试（不是胶片）
scripts\push.cmd                登录 GitHub 并推送
```

macOS 上双击 `scripts\verify-macos.command`（需要 `xcode-select --install` 和 `brew install cmake ffmpeg`）。

### 验证是否还健康

```powershell
cd X:\agent\SpektraRoll
.\build\spektra-roll-cli.exe version        # 应输出 spektra-roll-cli 0.1.0
ctest --test-dir build --output-on-failure # 应 6/6 通过
.\build\spektra-roll-cli.exe lut bake --out out\check.cube
.\build\spektra-roll-cli.exe lut compare --a tests\reference\portra400_endura_33.cube --b out\check.cube
# 应输出 PSNR ≈ 88 dB, PASS
```

### 视频命令（M3 核心）

```powershell
# 整段：解码 → 套 LUT → 带原声重新编码
.\build\spektra-roll-cli.exe video --lut out\m3-applelog-sdr.cube `
    --in X:\agent\footage\A001_11011538_C001.mov --out out\graded.mov `
    --trc srgb --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1 `
    --ffmpeg J:\ffmpeg\bin\ffmpeg.exe
```

- 没有 `--ffmpeg` 时用 PATH 里的 ffmpeg，也可以用环境变量 `SPEKTRA_FFMPEG`
- `--trc pq|hlg` 出 HDR；`--codec prores` 出 ProRes 422 HQ
- 帧区间：`--start 00:00:12.5 --frames 240`（或 `--duration 10`）

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
X:\agent\footage\A001_11011538_C001.mov    已收到（仓库外，不进 git）
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
├── src/core/include/sr/     公开头文件（lut3d, image, json, profile, film_model, metrics, transfer, gamut, texture, parallel, spectral_data）
├── src/core/src/            实现 + 生成的 spectral_tables.cpp
├── src/cli/main.cpp         命令行入口（gen / convert / video / lut bake|apply|compare|info / version）
├── tests/                   6 个测试程序 + reference/ 冻结参考 LUT
├── data/profiles/           上游 profile JSON（CC BY-SA 4.0）+ NOTICE.md
├── tools/parity/            Python 参考链、上游交叉检查、说明
├── third_party/stb/         stb_image / stb_image_write（单头文件）
├── scripts/                 .ps1 + .cmd 双份，.cmd 给用户双击
├── .github/workflows/       build.yml：Windows + macOS 矩阵
├── docs/architecture.md     目标架构、M1 链路、确定性规则、M0–M5 路线图
├── docs/licensing.md        GPL / CC BY-SA / LUT / 商标 / 编解码专利约束
├── README.md                对外介绍
├── RUN.md                   给非程序员的操作手册
├── STATUS.md                一页纸进度
├── DECISIONS.md             决策台账 D001–D038
├── progress.md              本文件
└── LICENSE                  GPL-3.0
```

---

## 六、下游 AI 的“别做的事”清单

- ❌ **不要推翻 D001–D038**，那些是已经执行并验收/记录过的
- ❌ **不要问用户技术细节问题**，只能问“选 A 还是 B”
- ❌ **不要在没跑 `verify-m1.cmd` 的情况下改光谱链**，改完 PSNR 掉了要立刻知道
- ❌ **不要把 display-encoded sRGB 的 LUT 说成线性/ACES LUT**（D013）
- ❌ **不要把 16-bit PNG 当成终点**，浮点 / EXR 仍是欠账（D017）
- ❌ **不要说“没有 Mac”**，产品负责人有 MacBook Air M1；macOS 相关先让他双击 `scripts\verify-macos.command` 拿结果
- ❌ **不要只在一个平台验证改动的路径**（issue #1 的教训）：CI 的 macOS 视频冒烟现在开着，别关掉
- ❌ **不要把颗粒的 `--grain-rate` 默认改回 1**，那是数字沸腾（D036）
- ❌ **不要在没有基准数字的情况下改质感默认值**，颗粒/眩光都是按你的反馈调过的（D034/D036）
- ❌ **不要偷懒改品牌**，SpektraRoll 已定案，改一次要动二进制名/CI/文档

---

## 七、下一次会话的理想开场

你说一句：

> “读 progress.md，继续。”

AI 就应该：

1. 先跑 `scripts\verify-m1.cmd -NoOpen` 确认 M1 parity 还是 88.07 dB
2. 再跑 `scripts\demo-m3.cmd -NoOpen` 确认视频 + 质感链路没坏（约 1 分钟）
3. 看 §三 的"下一步"，从**颗粒场缓存**开始

约束：每次改完都要重跑 `lut compare`（不能掉到 88 dB 以下）和 6 个测试程序。
