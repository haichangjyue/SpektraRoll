# DECISIONS — 决策台账

> 每条记录：背景 / 选项 / 我的建议 / 你的决定 / 后果。
> 目的：三个月后不用重新争论一遍；换人接手也能看懂为什么。

---

## D001 · 第一个可运行成果

- **日期**：2026-11-01
- **背景**：没有 Mac，只有 Windows 开发机。
- **选项**：
  - A. Windows 命令行先跑通
  - B. 先做 macOS 界面（只能 CI 编译，看不到画面）
  - C. 先写设计文档，不写可运行代码
- **建议**：A
- **你的决定**：**A**
- **后果**：最快看到结果；macOS UI 推迟到有 Mac 或有人代验之后。

---

## D002 · 代码托管

- **日期**：2026-11-01
- **选项**：
  - A. 现在就建 GitHub 公开仓库
  - B. 先私有，以后再公开
  - C. 还没有账号
- **建议**：A —— 公开仓库的 macOS CI runner 免费，正好补上"没有 Mac"的缺口。
- **你的决定**：**A**
- **后果**：双平台自动编译不再花钱；代码对外可见（GPL 本来就要求开源，无损失）。

---

## D003 · 许可证

- **日期**：2026-11-01
- **背景**：上游 spektrafilm 是 GPLv3，且明确"衍生作品必须开源"。
- **选项**：
  - A. GPL-3.0-or-later
  - B. 找作者谈商业授权（闭源）
  - C. 从零重建，不碰上游
- **建议**：A（你已表态要开源）
- **你的决定**：**A**
- **后果**：
  - 不能上 Mac App Store（GPL 与苹果条款冲突），走官网 DMG。
  - 插件（OFX）也必须 GPL。
  - profile / LUT 数据仍是 CC BY-SA 4.0，署名必须保留。

---

## D004 · 图像 I/O 方案（M0）

- **日期**：2026-11-01
- **选项**：
  - A. stb_image / stb_image_write（单头文件，零依赖，8bit）
  - B. OpenImageIO（16bit/EXR/色彩管理齐全，但依赖重）
  - C. libpng + zlib 手写
- **建议**：A 先跑通，P1 换 B。
- **决定**：**A**（我代你做的技术决定，写在台账里备查）
- **后果**：M0 只能 8bit；`lut apply` 不做传递函数转换。**这是欠账，不是设计。**

---

## D005 · 颜色变换的实现形态

- **日期**：2026-11-01
- **背景**：光谱管线跑不动视频。
- **决定**：**颜色烘焙成 3D LUT，空间效果（颗粒/光晕/扩散）留在 GPU pass。**
- **后果**：LUT 必须 float32、33³ 起步、四面体插值；否则暗部出色带。

---

## D006 · 构建系统

- **日期**：2026-11-01
- **决定**：CMake ≥ 3.20 + Ninja + C++20。
- **后果**：Windows 用 MSVC，macOS 用 Clang，CI 两边一致。

---

## D007 · 产品名与仓库名

- **日期**：2026-11-01
- **背景**：需要一个能长期用的名字，避免后面改二进制名、CI、文档。
- **选项**：`spektravideo`（临时代号）/ `SpektraRoll` / 其他
- **建议**：先用代号，等方向稳了再定
- **你的决定**：**`SpektraRoll`**，立即生效
- **后果**：
  - 仓库：https://github.com/haichangjyue/SpektraRoll
  - 二进制：`spektra-roll-cli`
  - C++ 命名空间：`sr`，头文件目录 `src/core/include/sr`
  - 本地目录：`X:\agent\SpektraRoll`
  - 与上游 `spektrafilm`、同类产品 `SpektraLab` 形成 "Spektra-" 家族，但品牌独立。

---

## D008 · 第一支胶片 / 相纸组合

- **日期**：2026-11-01
- **背景**：M1 要烘焙第一张真正的光谱胶片 LUT，必须先固定胶片与相纸。
- **选项**：
  - A. Kodak Portra 400 + Kodak Portra Endura
  - B. Kodak Vision3 250D + Kodak 2383
  - C. 两套都做
- **建议**：A（数据最全、最容易用眼睛判断"像不像"）
- **你的决定**：**A**
- **后果**：
  - M1 只对这一对负责；换胶片只需要换 profile JSON，不用改代码。
  - 电影负片（Vision3 + 2383）留到 M2 之后，作为第二套验证。

---

## D012 · 光谱上采样方法

- **日期**：2026-11-01
- **背景**：sRGB 只有 3 个通道，光谱模型需要每个像素的 81 个波长。上游提供两种上采样：Mallett 2019（81×3 基函数）和 hanatos2025（128×128×81 光谱 LUT，约 10 MB）。
- **选项**：
  - A. Mallett 2019：小、可移植、结果完全可复现，只多 243 个常数
  - B. hanatos2025：更接近上游当前默认，但要多打包 10 MB 数据
- **决定**：**A**（我代你做的技术决定）
- **后果**：
  - C++ 引擎自带 81×3 基函数，不需要运行时大数据文件。
  - 与上游 `rgb_to_raw_method="mallett2019"` 对齐；hanatos2025 往后放，作为可选项。

---

## D013 · LUT 的输入输出定义域

- **日期**：2026-11-01
- **背景**：3D LUT 必须说清楚它吃的是线性光还是显示编码值，否则整条链会差一个 gamma。
- **决定**：**display-encoded sRGB 进 → display-encoded sRGB 出**。内部先解码 CCTF，跑光谱链，再编码。
- **后果**：
  - `lut apply` 可以直接套在普通 PNG 上，不需要额外的色彩管理。
  - LUT 文件不能直接给 Resolve 的线性工作流用，那是 M2 的色彩/HDR 任务（D021）。

---

## D014 · 输出色域压缩（M1）

- **日期**：2026-11-01
- **背景**：上游默认用 `cam16ucs` + 亮度压缩把超色域结果压回 sRGB；实现重、参数多。
- **决定**：**M1 关闭**（`algorithm="off"`），parity 参考也按关闭配置。
- **后果**：
  - 极端饱和输入会得到负的或大于 1 的编码值，写 PNG 时被钳制。
  - 色域压缩作为 M2 的独立模块，不和光谱链一起调试。

---

## D015 · 耦合剂（couplers）

- **日期**：2026-11-01
- **决定**：**M1 关闭**（`dir_couplers.active = False`）。
- **后果**：先保证"灵敏度积分 → 特征曲线 → 印片 → 扫描"这条主链正确；耦合剂放 M2，颗粒、光晕、扩散放 M4。

---

## D016 · 印片机中性滤色片

- **日期**：2026-11-01
- **背景**：放大机灯泡 + KG3 热滤片 + 二向色滤色片决定印片光。上游按"相纸 / 光源 / 胶片"查表给出中性值。
- **决定**：直接查上游 `neutral_print_filters.json`：`kodak_portra_endura` + `TH-KG3` + `kodak_portra_400` = **(C=0, M=51.56801468495496, Y=52.53400422349596)**。
- **后果**：
  - 用通用值 (0, 65, 55) 会明显偏绿，已实测排除。
  - CLI 仍允许 `--cc C,M,Y` 覆盖，方便以后做"偏色"创作。

---

## D017 · 16-bit 图像 I/O

- **日期**：2026-11-01
- **背景**：D004 欠下的 16-bit 账。OpenImageIO 在 MinGW 上依赖重、编译慢。
- **选项**：
  - A. stb 读 16-bit PNG（`stbi_load_16`）+ 自己写 16-bit PNG（复用 stb 的 deflate）
  - B. 现在引入 OpenImageIO
- **决定**：**A**；OpenImageIO / EXR 推到真正需要浮点和 EXR 的时候。
- **后果**：
  - `--bits 8|16`，读 16-bit 时按 65535 归一化；16-bit 输出只支持 `.png`。
  - 写 PNG 的 chunk、CRC、IHDR 都是自己拼的，已用独立解码器验证位深正确。

---

## D018 · M1 的 parity 验收标准

- **日期**：2026-11-01
- **决定**：C++ 烘焙结果与 Python 参考实现在 33³ 全部 35,937 个点上比较：
  - **PSNR ≥ 60 dB**（`lut compare` 的默认门槛）
  - 同时报告 max |diff| 与 CIE76 ΔE（mean / p95 / max），用于人眼尺度判断
- **实测**：PSNR **88.07 dB**，max |diff| **4.57e-4**，ΔE76 mean **0.0015** / p95 **0.0035** / max **0.0065**（远低于 1 个 JND）。
- **后果**：阈值写进 `scripts\verify-m1.cmd`，以后改模型跑一次就知道有没有退化。

---

## D019 · CI 也跑 M1 光谱链

- **日期**：2026-11-01
- **背景**：CI 原来只做编译 + 单元测试 + `gen` / 合成 LUT 冒烟，光谱链坏了要在本地才会发现。
- **决定**：GitHub Actions 的 Windows 和 macOS 两个 job，在冒烟之后都执行 `lut bake` + `lut compare`（对 `tests/reference/portra400_endura_33.cube`）。
- **后果**：
  - 任何平台的光谱链退化都会在 PR/推送时直接变红，门槛仍是 D018 的 PSNR ≥ 60 dB。
  - 顺带证明了 macOS 编译出来的程序能跑通完整 M1 链路，而不只是"能编译"。

---

## D020 · M2 的范围与顺序（真实照片 → HDR → 视频）

- **日期**：2026-11-01
- **背景**：M1 的样张是合成测试图，能证明数值对，但判断不了"像不像胶片"。同时你明确要求支持 Apple Log / HDR 和 ProRes。
- **选项**：
  - A. 先做真实照片样张 + Apple Log/HDR 色彩链，视频编解码放后面
  - B. 先引 FFmpeg 做 ProRes 视频读写，色彩链的 HDR 改造往后放
- **决定**：**A**（你选的）
- **后果**：
  - **M2 = 颜色正确**：真实照片样张、输出色域压缩、耦合剂、Apple Log 输入、HDR 输出
  - **M3 = 能吃视频**：FFmpeg / ProRes 读写、时间维度、帧区间批处理
  - **M4 = 质感与性能**：颗粒 / 光晕 / 扩散 / 眩光，确定性种子，SIMD / 多线程 / GPU
  - **M5 = 界面**：Windows WinUI 3 / macOS SwiftUI / OFX 插件
  - 原先挂在 M2 的 GPU 空间效果和性能优化顺延到 M4

---

## D021 · Apple Log / HDR 支持路线

- **日期**：2026-11-01
- **背景**：现在的 LUT 是 display-encoded sRGB（D013），只覆盖 SDR。Apple Log 是"对数曲线 + 宽色域"，用来装下手机传感器的高动态范围；真正 HDR 交付还需要 PQ/HLG 曲线、Rec.2020 色域和 10-bit 以上位深。
- **决定**：
  - **输入**：支持 Apple Log（按 Apple 公开公式解码，不需要 Apple SDK），同时保留 Rec.709/SDR 输入
  - **内部**：解码到线性宽色域后跑胶片模型，模型本身不变
  - **输出**：SDR（Rec.709 / sRGB）与 HDR（PQ 或 HLG + Rec.2020，≥10-bit）两条
  - **HDR 曝光映射**：作为独立参数，由你决定"高光能到多亮"，不做自动猜测
  - **LUT 形态**：HDR 用 1D shaper + 3D LUT（或 65³），不再只有 33³
- **后果**：
  - Apple Log 的验收依赖你提供的 iPhone 素材（M2 前置条件）
  - HDR 的"眼睛验收"需要 HDR 显示器；没有时只报数值，不承诺视觉结论
  - 动态元数据（Dolby Vision 之类）不在 M2 范围内

---

## D023 · Apple Log 输入的落地方式

- **日期**：2026-11-01
- **背景**：Apple Log 是 BT.2020 基底 + 对数曲线，不是 sRGB。需要一种不推翻 D013（LUT 域是编码值）和 D018（parity 门槛）的接入方式。
- **决定**：
  - 新增输入传输函数开关：`srgb`（默认，M1 行为一字不改）与 `apple-log`
  - `lut bake --input apple-log` 的 LUT 定义域改为 **Apple Log 码值立方体**；内部按 Apple 公开公式解码 → 线性 BT.2020 → 线性 sRGB → 胶片模型 → 编码 sRGB
  - 曝光用 `--exposure <EV>` 在进入胶片模型前施加（默认 0，演示用 +0.7）
  - 新增 `convert --from apple-log`，输出"只解码、不上胶片"的 SDR 对照图
  - 超 sRGB 色域产生的负线性值暂时钳制到 0（D014 的色域压缩仍未做）
- **后果**：
  - Apple Log 解码与 colour-science 0.4.7 逐点一致（16-bit 输出误差 ≤ 1 LSB）
  - 18% 中灰在 Apple Log 里编码为 **0.4882724585**，解码回来正好 0.18
  - 输出仍是 SDR sRGB；HDR（PQ/HLG）是 D021 的下一步
  - 素材不进入仓库，验收时用 ffmpeg 抽一帧 16-bit PNG；视频读写仍是 M3

---

## D024 · 扫描负片模式（不印相纸）

- **日期**：2026-11-01
- **背景**：印片模式在 8–9 档的逆光场景下，天空被压在 0.88–0.91（几乎无色阶），暗部掉进相纸黑位（7.7% 的像素 < 0.10）。上游的 `io.scan_film` 只是"直接扫负片"，输出是反相带橙罩的负像，不能当正片用。
- **决定**：
  - 新增 `lut bake --scan print|negative`，默认 `print`（M1 行为不变）
  - `negative`：胶片密度 → 反相（`Dmax − D`）→ **去掉片基密度**（橙罩消失）→ D50 下扫描 → 用"无染料 / 最大染料"两点把结果映射到 [0,1] → 用 18% 灰的每通道幂律把中间调中性化（补偿染料串色）
  - 不采用上游 `scan_film` 的原始输出（那是负像）
- **后果**：
  - 天空从 0.88–0.91 变成 **0.48–0.70**（云有层次）；暗部 < 0.10 的像素从 **7.7% 降到 2.5%**
  - 画面偏平、略偏暖是负片的特性，需要调色；印片模式继续保留，二者可对比
  - 这条路径**没有上游 parity**（上游没有正片化的负片扫描），验收改为内部不变量：黑=0、白 ≥0.8、18% 灰中性（通道差 <0.02）、灰阶单调
  - 中性化只是**一条幂律近似**，不是完整的 3×3 染料校正矩阵；后续颜色更准时要换成矩阵

---

## D025 · 负片成为推荐出片路径；反差做成参数

- **日期**：2026-11-01
- **背景**：你看了印片和负片两张样张，确认"负片是对的"。
- **决定**：
  - **Apple Log 素材的推荐路径是扫描负片**；印片保留，用来对比"洗出来的照片"是什么样
  - 新增 `--contrast <n>`：以扫描出的 18% 灰为轴做幂律，只改对比、不改色相（1.0 = 只用胶片曲线；1.15 = 稍加反差）
  - demo 同时出"负片"和"负片 + 1.15"两张，供直接比较
- **后果**：
  - 训练有素的眼睛可以直接调 `--contrast` 找到"有反差但不死黑"的位置
  - 反差越大，暗部 <0.10 的比例越高（1.0 时 2.5%，1.15 时 6.8%，1.3 时 13.1%）——这是取舍，不是越大约好
  - 曝光 `--exposure` 与反差 `--contrast` 是 M2 阶段唯一两个"创作旋钮"，其余保持物理模型

---

## D026 · 负片默认反差 1.15

- **日期**：2026-11-01
- **背景**：1.0（只走胶片曲线）偏平，1.3 暗部回潮；你在样张里选了 1.15。
- **决定**：
  - **CLI 的 `--scan negative` 默认反差 1.15**；显式 `--contrast 1.0` 拿回"全余量"版本
  - 库层 `BakeOptions` 的默认仍是 1.0（模型本身保持纯胶片曲线，默认值属于产品层）
  - demo 同时出 1.15 和 1.0 两张，标好用途
- **后果**：
  - 交付默认画面对得上你验收的那张
  - 需要留调色余量时明确写 `--contrast 1.0`，不再依赖隐式默认

---

## D027 · 输出色域压缩（OkLCh）

- **日期**：2026-11-01
- **背景**：M1 印片 LUT 有 **4,644 / 35,937** 个条目线性值是负的（Apple Log 版本 3,763 个），写图时被硬钳制，饱和色的色相会偏。负片路径因为按"无染料/最大染料"逐通道归一化，天然落在 [0,1] 内。
- **决定**：
  - 新增 `--gamut oklch|off`：在 OkLCh 里保持明度和色相、只压缩色度，把超色域颜色滚回 sRGB 立方体
  - 膝点取 **(0.8, 1.0, 2.0)**：只压色度超过 80% 边界的部分。上游默认膝点是 0（全范围压缩），连纯红都会从 1.0 压到 0.90，我们不采用
  - **Apple Log 输入默认开启**；sRGB 输入默认关闭，保证 M1 parity 逐位不变
- **后果**：
  - Apple Log 印片 LUT 的负值从 3,763 个降到 **0**，最饱和的像素最多改变 0.10（其余像素平均只差 2.5e-5）
  - 负片路径是空操作（本来就落在立方体内），留着开关是为了以后换输出色域（Rec.2020 / P3）时复用
  - 与上游没有逐点 parity（膝点不同），验收靠单元测试：色域内不变、色域外落回立方体、色相保持（<5e-3 rad）、膝点单调

---

## D028 · DIR 耦合剂

- **日期**：2026-11-01
- **背景**：D015 把耦合剂推到 M2。上游默认开启（`dir_couplers.active=True`），在 LUT 模式下会自动关掉空间扩散，只留逐像素的层间抑制化学。
- **决定**：
  - 实现 DIR 耦合剂：自层 + 层间抑制矩阵（负片系数 0.336/0.319/0.273 + 层间 0.353/0.302/0.154/0.353/0.168/0.226），先算"没有抑制时"的特征曲线，再把曝光减掉抑制量重新查曲线
  - **Apple Log 默认开启**；sRGB 参考路径默认关闭，M1 parity 保持逐位不变
  - `--couplers on|off` 可以覆盖
- **后果**：
  - 与上游参考 bake 对齐：**PSNR 80.75 dB**，max |diff| 6.8e-4，ΔE76 max 0.0126
  - 中性灰几乎不变（曲线预先补偿），饱和色明显更饱和——蓝色招牌、绿色植被的变化最明显
  - 空间扩散（20 µm 高斯）仍然不做：它无法烘进 3D LUT，属于 M4 的 GPU 阶段

---

## D029 · HDR 输出（PQ / HLG）

- **日期**：2026-11-01
- **背景**：M2 的最后一项。Apple Log 素材本身有约 6 档高光余量，SDR 交付把它们压成一片白。
- **决定**：
  - 新增 `--hdr pq|hlg` 与 `--peak <nits>`（默认 1000）
  - PQ：画面白（线性 1.0）映射到 `peak` 尼特，按 SMPTE ST 2084 编码；中性灰约 90 nits（白 1000 时）
  - HLG：按 BT.2100 OETF，1.0 = 峰值信号
  - 输出仍是 16-bit PNG（存 PQ 码值），演示脚本再用 ffmpeg 封装成 HEVC Main10 + PQ/BT.2020 的 `.mov`
- **后果**：
  - `lut bake --hdr pq` 的探针：输入 1.0 → 输出 **0.75183**（= 1000 nits 的 PQ 码值）
  - 胶片模型完全不变，只是末端编码从 sRGB 换成 PQ/HLG
  - **MacBook Air M1 的屏幕是 SDR**，在本机看 HDR 文件与普通视频无异；验收要用 iPhone 16 Pro（XDR 屏）或外接 HDR 显示器
  - demo 的 HEVC 片段用 4:2:0 10-bit 编码，饱和边缘会有轻微色度误差（PQ 空间最大 0.09），以 16-bit PNG 为准
  - demo 现在处理**整段素材**：`lut apply --in-dir/--out-dir` 批量套 LUT，再带原声重新编码（99 帧，4.125 s）

---

## D022 · 视频读写的形态：独立程序（已定）

- **日期**：2026-11-01
- **背景**：M3 要吃 `.mov`。两条路：独立程序自己做编解码，还是做 Resolve 插件由宿主负责。
- **决定**：**独立程序**（你选的）。编解码统一走 FFmpeg，先以**子进程**方式集成（`spektra-roll-cli video` 负责解码、套 LUT、编码），不把 FFmpeg 链接进 exe。
- **后果**：
  - 用户机器上需要有 `ffmpeg.exe`（demo 会指到 `J:\ffmpeg\bin\ffmpeg.exe`，也可用 `--ffmpeg` 或 `SPEKTRA_FFMPEG` 指定）
  - 好处：不用把 100 MB 的 FFmpeg 链进构建、Windows/macOS 行为一致、升级 FFmpeg 不用重编
  - 代价：exe 不再是"单文件零依赖"；以后要单文件分发时再切换成链接 libav*
  - **教训（issue #1）**：POSIX `popen()` 只接受 `"r"/"w"`，`"rb"/"wb"` 在 macOS 上直接返回 NULL。今后所有"只在一个平台跑"的路径都要想办法进 CI，否则等于没测
  - D010（OFX 插件）仍然待定，但不再和 M3 绑在一起

---

## D031 · 帧区间与 ProRes 输出

- **日期**：2026-11-01
- **背景**：M3 要能只渲一段，也要能把结果交给剪辑软件（ProRes）。
- **决定**：
  - `--start <秒 或 HH:MM:SS.mmm>`、`--frames N`、`--duration T`；解码用 `-ss`/`-frames:v`，音频用同样的 `-ss`/`-t`，保证音画同步
  - `--codec hevc|h264|prores`；ProRes 走 `prores_ks -profile:v 3`（422 HQ，10-bit 4:2:2），HDR 时同样写 BT.2020 / PQ 标记
- **后果**：
  - 1 秒片段（24 帧）验证通过：视频 1.000 s、音频 1.021 s（AAC 帧对齐的正常偏差）
  - 同一段素材出 ProRes 422 HQ 是 **120 MB / 4.1 s**——ProRes 本来就大，这是交换格式不是交付格式
  - 断点续渲、时间码（timecode track）还没做

---

## D032 · 多线程（提前做）

- **日期**：2026-11-01
- **背景**：视频 268 ms/帧，1 分钟 1080p 要跑 5 分钟，验收节奏会被拖死。原计划放 M4，但代价小、收益立竿见影。
- **决定**：
  - 新增 `sr/parallelChunks`（`std::thread`，按区间切分），纯函数逐像素的特性让并行天然安全
  - `--threads N`（0 = 自动 = 硬件线程数 − 1，给编解码进程留一个核）
  - 用在：LUT 烘焙、单帧 / 序列套 LUT、视频逐帧
- **后果**：
  - 单帧套 LUT：96 ms → **17 ms**（5.5×）
  - 烘焙总时长：245 ms → 108 ms（其中约 80 ms 是写 `.cube` 文件，计算部分更快）
  - 视频：268 → **214 ms/帧**，瓶颈已经从 LUT 转到 x265 编码
  - 输出逐位不变（两次烘焙 ΔE = 0.0000，PSNR PASS）
  - 下一步提速只能靠编码器（`--crf`/preset）或硬件编码（QSV/NVENC），那属于 M4

---

## D033 · 胶片质感：颗粒 + 光晕

- **日期**：2026-11-02
- **背景**：颜色链已经站住，但画面还"太干净"——缺的正是让人一眼认出胶片的东西。颗粒和光晕都是**空间效果**，不能烘进 3D LUT，只能在逐帧处理时叠加。
- **决定**：
  - 新增 `sr/texture`：颗粒用确定性哈希噪声（seed + frame + x/y 决定，可复现）+ 盒状模糊聚团，权重按到 0.5 的距离衰减（中灰最强、暗部亮部最弱）
  - 光晕：高光阈值 → 三次盒状模糊近似高斯 → 暖色（R 1.0 / G 0.42 / B 0.18）叠加
  - CLI：`--grain 0..1`、`--halation 0..1`、`--seed N`，外加 `--grain-size`、`--halation-radius/-threshold`
  - 在 `video` 与 `lut apply` 的最末端执行，LUT 与胶片模型完全不动
- **后果**：
  - 同一 seed + frame 输出逐位相同；换 frame 得到新的颗粒场（视频里颗粒会动）
  - 视频 214 → **311 ms/帧**（质感约 97 ms）
  - 光晕半径现在按画幅宽度比例给，还不是物理微米；真实点扩散核留到 M4
  - 眩光、扩散、halation 的反射分量都还没做

---

## D034 · 颗粒调轻：从"电视雪花"改成"胶片颗粒"

- **日期**：2026-11-02
- **背景**：你反馈"画面看起来比较毛躁"。
- **原因**：旧实现里 `--grain-size 1.2` 被换算成模糊半径 `round(1.2)-1 = 0`，等于**完全没有模糊**——每像素独立噪声，看起来像数字雪花，不像胶片颗粒。
- **决定**：
  - 模糊半径改为 `round(size)`：默认 size 1.4 → 半径 1（3×3 聚团），不再是逐像素噪声
  - demo 默认值改为 **grain 0.4 / halation 0.45**（数值比原来大，但实际观感约为原来的 1/2.7）
  - 亮度权重不变：中灰最强，暗部/亮部最弱
- **后果**：
  - 100% 裁切里的噪声标准差从约 0.014 降到 **0.0053**（p99 0.015），颗粒成团、边缘更柔和
  - 觉得还想更轻/更重：`demo-m3.cmd -Grain 0.25` 或 `-Grain 0.55`，不用改代码
  - 这是观感参数，不追求物理单位；真正的颗粒模型（按层、按曝光、按扫描分辨率）留到 M4

---

## D035 · 眩光 + 扩散

- **日期**：2026-11-02
- **背景**：颗粒和光晕补上后，画面还差"镜头里那层雾"和"柔化"。这两项和颗粒一样是空间效果，放在同一处逐帧执行。
- **决定**：
  - **眩光（glare）**：整幅亮度做一次很宽的模糊，用 **screen 混合**（`1-(1-a)(1-b)`）叠回去——暗部被抬、对比略降，高光几乎不动；带一点暖色（R 1.0 / G 0.96 / B 0.92）
  - **扩散（diffusion）**：把"模糊后的亮度 − 原亮度"作为偏移加到三个通道上，亮处柔化、旁边的暗处吃到一点光，**色相不变**
  - 顺序固定为：光晕 → 眩光 → 扩散 → 颗粒（模拟光线从乳剂到镜头再到扫描的顺序）
  - CLI：`--glare`、`--diffusion`，各自带 `-radius`；demo 默认 0.2 / 0.1
- **后果**：
  - 视频从 311 ms/帧涨到 **486 ms/帧**（四层空间效果全开）
  - 两个模糊都是盒状近似，半径按画幅宽度比例给，不是物理单位
  - 模糊目前全分辨率做，是当前最大开销；下一步可以用降采样模糊换 3–4 倍速度

---

## D036 · 颗粒的时间连续性

- **日期**：2026-11-02
- **背景**：单帧观感满意，但看视频时"噪点闪烁、挺跳"。
- **原因**：每帧直接用 `frame` 做哈希，帧与帧之间的噪声场**完全不相关**。24 fps 下就是数字沸腾（boiling），真胶片的颗粒不会这样抖。
- **决定**：
  - 颗粒场改为**两场交叉淡化**：`field(t) = mix(F(k), F(k+1), phase)`，`k = t / rate`，`phase = (t mod rate) / rate`
  - 默认 `--grain-rate 3`（3 帧完成一次过渡）；`1` 等于回到旧行为
  - 混合会损失约 30% 振幅，按 `1/sqrt((1-p)²+p²)` 归一化，颗粒浓度保持不变
- **后果**：
  - 静态素材上帧间平均差 **0.0025 → 0.0014（−44%）**，闪烁明显减轻
  - 颗粒计算量约翻倍（多生成一个场，+70 ms/帧）
  - 想更稳可以 `--grain-rate 6`；想回到"颗粒每帧全新"用 `--grain-rate 1`

---

## D037 · 模糊降采样 + 编码参数开关

- **日期**：2026-11-02
- **背景**：四层质感全开后 579 ms/帧，其中眩光 + 扩散 + 光晕约 234 ms，颗粒 139 ms。
- **决定**：
  - 宽模糊改用**降采样模糊**：眩光降 4 倍、光晕/扩散降 2 倍，模糊后再双线性放大；半径小于 2× 因子时回落到全分辨率三次盒模糊
  - 视频新增 `--crf`（默认 16）和 `--preset`（默认 medium），透传给 x264/x265
- **后果**：
  - 579 → **469 ms/帧**（−19%）；单帧 306 → 279 ms
  - 与全分辨率版本相比 max |diff| 0.011、mean 0.0008，肉眼无差
  - `--crf/--preset` 的实际收益被 CPU 争用抵消：我们的线程池吃 11 个核，x265 也要 12 个线程，两者互相抢；真正的下一步是硬件编码（QSV/NVENC）
  - 颗粒仍是最贵的单层（约 139 ms），后续可用场缓存把跨帧复用做掉

---

## D038 · 硬件编码选项（QSV）

- **日期**：2026-11-02
- **背景**：想靠 GPU 编码把渲染压下来（这台机器是 Intel Arc A750）。
- **决定**：新增 `--encoder sw|qsv`，QSV 走 `hevc_qsv` / `h264_qsv`，HDR 用 `p010le` + Main10；`--crf` 映射到 `global_quality`（+4 经验偏移）。
- **后果**：
  - 功能可用（HDR 输出仍是 HEVC Main10 + PQ/BT.2020，已验证）
  - **提速有限**：2.6 MP 下 200 ms vs 软件 214 ms。实测发现瓶颈不在编码器，而在**管道与逐像素转换**（每帧 15 MB 读入、15 MB 写出外加两次格式转换）
  - 大画幅（4K 以上）时 QSV 的优势会更明显，所以保留这个开关
  - 真正的下一步是减少纹理计算（颗粒场缓存）而不是换编码器

---

## D039 · 颗粒场缓存

- **日期**：2026-11-02
- **背景**：D036 的两场交叉淡化让每帧都重新生成**两个**噪声场并各模糊两次。但 `--grain-rate 3` 时一个场连续三帧都在用，实际上每三帧只有一个场是新的——其余都是重复劳动。
- **决定**：
  - 新增 `sr::GrainFieldCache`：只保留**最近使用的两个场**（LRU），按 `seed / 宽 / 高 / 半径 / 场序号` 做键
  - `applyGrain` 增加可选 `cache` 参数；视频和 `lut apply --in-dir` 传同一个缓存，单张图不传
  - 交叉淡化改成在应用像素时按公式现算，不再单独写一个混合缓冲，缓存路径和不带缓存路径走**同一段像素代码**，所以输出逐位一致
  - 缓存不是线程安全的；渲染循环本来就是逐帧顺序执行
- **后果**：
  - 2.6 MP 全质感视频 **476.5 → 358.7 ms/帧**（总时长 47.17 → 35.51 s，−25%），比预估的 350 还好一点
  - 颗粒场的生成从"每帧两个"降到"每个场序号一个"；`--grain-rate 3` 时约省 2/3
  - 缓存占用约 2 × 10.5 MB（2160×1214 float），比原来每帧临时分配两个场 + scratch 更省
  - 单张图（`lut apply`）路径不变；`test_texture` 新增"带缓存 == 不带缓存"和"场只生成一次"的检查

---

## D040 · 分段渲染、断点续渲、时间码

- **日期**：2026-11-02
- **背景**：长片一次跑完，中途断掉就得从头再来；输出文件也没有保留相机的 timecode。
- **决定**：
  - **分段**：`--segment SEC` 把渲染拆成 `<out>.parts/part-00000.mov` 等文件，每段只写视频
  - **音频只复制一次**：所有段完成后再从源文件把音频复制进最终文件，避免 AAC 逐段编码/拼接产生缝隙和 A/V 漂移
  - **断点续渲**：`--resume` 只复用带 `.done` 标记、且帧范围和当前计划一致的段；崩溃留下的半截文件没有标记，会被重渲。`--resume` 单独出现等价于 `--segment 10`
  - **配方校验**：`<out>.parts/config.txt` 记录输入、画幅/帧率、LUT 内容哈希、编码参数、纹理参数和帧范围；换了任何一项就自动清空重渲，避免把不同 codec/观感的分段拼在一起
  - **颗粒连续**：段内用**全片帧号**做颗粒场索引，段边界处的颗粒和整段渲染完全一致
  - 合并：ffmpeg concat demuxer + `-c copy`，再从源文件复制音频；`--keep-parts` 保留分段文件
  - **时间码**：读源文件的 `timecode` 标签，`--start` 时按帧数偏移；`--timecode` 覆盖，`--no-timecode` 关闭
  - 渲染时先写一个视频-only 临时文件，再统一 mux（单段也一样），所以单段和分段的音频路径只有一条
- **后果**：
  - 本机 4 s 素材：5 段；删掉第 4 段后重跑，日志显示 3 段 `already done`，只渲染缺失的 1 段（约 5 s 而不是 35 s）
  - 输出 99 帧 / 4.125 s / AAC 原声 / timecode `15:38:35:00`；`--start 1` 正确变成 `15:38:36:00`
  - 分段与整段输出逐帧 PSNR 约 46 dB（差异来自各段强制关键帧，不是内容跳变）
  - 未做：多段并行渲染（仍是单进程顺序）、drop-frame 时间码的精确偏移（按标称帧率计算，每小时差几帧）
  - CI 增加一次分段+时间码冒烟（Windows + macOS）

---

## D009 · Windows / 桌面 UI 技术栈

- **日期**：2026-11-02
- **背景**：命令行和渲染链已经稳定，下一步需要一个非程序员能用的界面；之前一直悬着没有拍板。
- **选项**：
  - A. WinUI 3（C++/WinRT，Windows 原生观感）
  - B. **Qt 6（Qt Widgets）**
- **你的决定**：**B · Qt 6**
- **后果**：
  - 不再要求安装 MSVC：Qt 官方提供 MinGW 构建，和现有 MinGW 工具链同一路线
  - 同一套 UI 代码以后可以直接编译到 macOS（Qt Widgets 跨平台），Windows 先交付
  - Qt 采用 LGPLv3 / GPLv3 双授权；本项目 GPL-3.0-or-later，用动态链接的 Qt 完全兼容（不分发闭源二进制）
  - 安装体积大：Qt 运行时 + MinGW 工具链约 1–2 GB，放在 `X:\agent\tools\qt`，不进仓库
  - 界面只做壳：预览和渲染仍调用同一套 C++ 核心与 `spektra-roll-cli`，不复制颜色/质感逻辑
  - 首版范围：选素材 → 选/烘 LUT → 调曝光与质感 → 预览一帧 → 渲染整段（带日志、进度、分段续渲）

---

## D041 · 界面 v2：按 SpektraLab 的三栏结构重做

- **日期**：2026-11-02
- **背景**：v1 把所有参数堆在左栏，像调试面板；SpektraLab（GPL-3.0，公开仓库）有成熟的桌面工具布局，值得作为参照。
- **决定**：
  - 三栏 + 顶栏：左边“材料”（素材 / 胶片 / 输出），中间画布 + 时间线 + 状态条，右边“参数 / 队列 / 日志”
  - 移植 SpektraLab 的设计令牌：`ground #5f5f5f`、`card #2d2d2c`、`text #faf8f4`、`muted #b7b6b3`、`accent #e1a95f`、2–3px 圆角、10–14px 字号
  - 参数统一成 `SliderRow`（标签 + 右对齐数值 + 细滑块）和可折叠 `Section`（1px 分隔线 + 13px 粗体标题）
  - 队列页需要更宽，切到“队列”时右栏自动 330 → 500px
  - 视频特有的时间线、队列、timecode、分段/续渲保留，不照搬照片条/裁切/RAW 解码
- **不做的**：不复制 SpektraLab 的 QML 源码、图标或截图；名称/图标是对方商标资产。只借鉴布局结构、颜色令牌和交互模式，并在 `docs/ui-analysis-spektralab.md` 记录来源。
- **后果**：
  - 界面从“表单面板”变成桌面工具：画布成为最大区域，参数密度提高但可折叠
  - 功能路径不变，预览/渲染/队列仍走同一个 CLI
  - 直方图、适合/100% 缩放已补上；裁切、胶片预设列表等列在分析文档的“下一步”

---

## D042 · 参数按电影冲印流程组织：显影前 / 显影后

- **日期**：2026-11-02
- **背景**：SpektraLab 把参数分成 Pre-Dev / Post-Dev；用户问 SpektraRoll 能否按电影胶片的冲印过程设计。
- **决定**：
  - 右栏改成四个 tab：**显影前 / 显影后 / 队列 / 日志**
  - **显影前（底片）**：曝光（进入底片前）、胶片（Kodak Portra 400）、DIR 耦合剂（显影阶段）、颗粒、颗粒稳定度、光晕
  - **显影后（印片与扫描）**：相纸（Portra Endura）、扫描方式（负片/印片）、反差、OkLCh 色域压缩、眩光、扩散、预览时间
  - 左栏保留材料与交付：素材、LUT/烘焙、输出（编码、色彩、区间、分段）
  - 新增两个真实参数：`--couplers on/off` 和 `--gamut oklch/off`，暴露到界面并写进 LUT 配方签名
  - 切换素材类型时，按 CLI 默认自动设置：Apple Log 耦合剂/色域压缩开，SDR 关
- **诚实边界**：LUT 仍是一次性烘焙的单张 3D 表，界面按**物理阶段**分组参数，不是运行时两段渲染；文档和提示里不把它说成两段光谱管线。
- **后果**：
  - 参数顺序与冲印流程一致：拍摄→底片→显影→印片/扫描→输出
  - 用户能直接关掉 DIR 耦合剂或色域压缩做 A/B
  - 显影前/后切换不会改变画布位置；队列索引改为 2

---

## D043 · 胶片 / 相纸 profile 选择

- **日期**：2026-11-02
- **背景**：界面一直只能用 Portra 400 + Endura；上游 spektrafilm 实际带 28 个 film/paper profile。
- **决定**：
  - 把上游全部 profile JSON 原样导入 `data/profiles/`（CC BY-SA 4.0，NOTICE 更新）
  - 界面动态扫描 profile 的 `info`：**只列出模型已验证的组合**
    - 11 支彩色负片（D55）：Fujifilm C200 / Pro 400H / X-Tra 400；Kodak Ektar 100、Gold 200、Portra 160/400/800/Push1/Push2、Ultramax 400
    - 6 种相纸：Fujifilm Crystal Archive II；Kodak Ektacolor Edge、Endura Premier、Portra Endura、Supra Endura、Ultra Endura
  - 选胶片后自动切到它的 `target_print`（在可用相纸里）；`--film` / `--paper` 传给 `lut bake`
  - 胶片/相纸文件名写进 LUT 配方签名，换材料会触发重新烘焙
  - 暂不列出：4 支反转片（缺直扫路径）、Vision3 200T/500T（T 光源，M1 只支持 D55）、Vision3 250D/50D、Verita 200D 和 2383/2393（target 是电影印片，引擎还不能当相纸用）
- **后果**：
  - 界面从 1 对材料变成 **11 × 6** 的可用组合；C200 + Crystal Archive、Portra 800 + Endura 已实测能烘
  - `data/profiles/` 增加约 5.5 MB 数据；NOTICE 明确标注可用/暂不可用
  - 反转片和电影印片是后续单独的引擎工作，不在本轮混进来

---

## D044 · 可选物理光谱引擎（`--engine spectral`）

- **日期**：2026-11-02
- **背景**：用户要求基于本机（i5-10600KF / 32 GB / Arc A750）预算，能物理计算的地方就用物理计算；LUT 只是物理链的采样。
- **决定**：
  - CLI 新增 `--engine lut|spectral`（`lut apply` 与 `video`）
  - `spectral` 不走 LUT：每个像素直接调用 `FilmModel::transformFrom`，材料（`--film/--paper`）、输入解码（`--input`）、曝光、扫描、反差、DIR 耦合剂、OkLCh、HDR 全部实时进入物理链
  - 默认仍是 `lut`；UI 输出栏新增“引擎”选择，默认 LUT，可切“光谱（物理，慢）”
  - 空间质感仍按原顺序叠在颜色之后；队列/分段/续渲/时间码对两种引擎都可用
  - 光谱模式不需要 LUT 文件，队列不复制 LUT 快照
- **实测（本机，12 线程）**：
  - 2.6 MP 单帧光谱：**1081 ms**
  - 2.6 MP 视频 12 帧：**1258.9 ms/帧**（含编码/管道）
  - 同尺寸 LUT 全质感：358.7 ms/帧；无质感：187 ms/帧
  - 4 s / 99 帧全光谱约 **2 分钟**（LUT 约 35 秒）
  - 同一帧光谱 vs 33³ LUT：**PSNR 62.19 dB**，说明 LUT 的采样误差很小
- **后果**：
  - 短片/单帧最终渲染可以完全物理；预览切换光谱约 1 秒/帧，不是实时
  - 长片/4K 仍建议 LUT；若要实时物理，下一阶段需要 Arc A750 的 Vulkan/D3D12 计算着色器
  - 这仍不是“每帧 81 波长”的着色器：光谱链在 CPU 上逐像素执行，空间效果仍是程序化近似

---

## D045 · GPU 光谱后端可行性（Arc A750）

- **日期**：2026-11-02
- **背景**：CPU 光谱引擎 2.6 MP 约 1081 ms/帧，距实时还很远；本机有 Intel Arc A750。
- **探针**（`tools/gpu-probe/`，D3D11 compute）：
  - 2160×1214 = 2.6 MP，每线程 81 次 density/exp2/CMF 累加
  - **0.63 ms/次**（5 次热身后，提交到 GPU 完成）
  - 设备确认：Intel Arc A750，D3D_FEATURE_LEVEL_11_0
- **决定**：
  - 下一步把完整 `FilmModel::transformFrom` 移到 D3D11 compute（HLSL）
  - 用 CPU 光谱输出做参考，要求 ≥60 dB；不通过就不接入
  - 接口 `--engine spectral-gpu`，失败/非 Windows 自动回退 CPU `/ spectral`
  - 颜色走 GPU 后，空间质感仍在 CPU/后续 GPU pass
- **后果 / 预期**：
  - 扫描阶段 GPU 比 CPU 快约三个数量级；完整链预计 5–20 ms/帧，足以实时预览
  - 仍需上传曲线/密度表、处理 float32 精度、回读纹理；工作量大，分阶段做
  - macOS 无 D3D11，继续用 CPU 光谱或 LUT

---

## D046 · GPU 光谱后端落地（`--engine spectral-gpu`）

- **日期**：2026-11-02
- **背景**：D045 探针测得 81 波长扫描阶段 0.63 ms；CPU 完整链 1081 ms/2.6 MP，差距三个数量级。
- **实现**：
  - `src/cli/gpu_film.{hpp,cpp}`：D3D11 compute + 运行时 HLSL 编译，逐行翻译 `FilmModel::transformFrom`
  - 负片扫描、印片、DIR 耦合剂、OkLCh 压缩、PQ/HLG、sRGB/Apple Log 解码、`--strength` 混合全部在着色器内完成
  - 参数走一张 22.9 KB 的常量缓冲（曲线、密度、CMF、光源、矩阵、标量），输入/输出各一张 structured buffer，读回后按 3 通道解包
  - `FilmModel::gpuData()` 把 double 常量摊平成 float32；曲线轴非均匀时拒绝 GPU
  - 没有 D3D11 设备、着色器编译失败或运行时出错时，自动回退 CPU 光谱并在 stderr 打 warning
  - 新增 `tests/test_gpu_film.cpp`（无显卡时 exit 77 记为 skipped），逐样本与 CPU 参考比对
- **实测（本机 Arc A750）**：
  - 2.6 MP 单帧 **50 ms**（含 42 MB 上传 + 42 MB 回读），CPU 光谱 1038–1158 ms，约 **21–23×**
  - 2.6 MP 视频（ProRes 422 HQ，3 帧）：**1427 → 379 ms/帧**（含编码与管道），约 **3.8×**
  - 与 CPU 光谱的 PSNR：负片 **119.1 dB**、印片 **inf（逐位相同）**、PQ 82.9 dB、HLG 90.1 dB、strength 0.6 时 121.3 dB
  - `test_gpu_film` 六组配置最差偏差 **3e-4**（编码空间），远优于 60 dB 目标
- **后果**：
  - UI 引擎下拉新增“光谱 GPU（物理，快）”，默认仍是 LUT；无显卡时界面无感回退
  - 视频现在受限于编码与管道，颜色不再是瓶颈；空间质感仍由 CPU 叠加
  - macOS 无 D3D11，继续走 CPU 光谱 / LUT（`gpu_film.cpp` 在非 Windows 上编译成占位实现）

---

## D047 · 空间质感上 GPU（`--texture-engine`）

- **日期**：2026-11-02
- **背景**：D046 之后颜色只占约 50 ms/帧，2.6 MP 视频仍是约 380 ms/帧；其中约 160 ms 是颗粒 / 光晕 / 眩光 / 扩散，剩下是解码、16-bit 转换与编码管道。
- **实现**：
  - `src/cli/gpu_texture.{hpp,cpp}`：D3D11 compute，逐 pass 翻译 `texture.cpp` 的 halation / glare / diffusion / grain。
  - 可分离盒状模糊按“一行 / 一列一个线程”实现，保持与 CPU 相同的浮点累加顺序，结果几乎逐位一致。
  - 宽模糊沿用 CPU 的降采样 → 2 段盒状模糊 → 双线性升采样（眩光 4×，光晕 / 扩散 2×）；小画幅仍走全分辨率 3 段。
  - 颗粒场用同一套 hash 在 GPU 上重建（含两次盒状模糊），`--grain-rate` 的交叉淡化与归一化保持不变。
  - 新增 `--texture-engine auto|cpu|gpu`：`auto` 在 `--engine spectral-gpu` 时启用；`gpu` 也可配合 LUT；无 D3D11 / 编译失败 / 运行时错误时自动回退 CPU，并在 stderr 打一次 warning。
  - `sr::applyTexture()` 成为 CPU 参考链的唯一入口；CLI、`tests/test_gpu_texture.cpp` 都调用它。
  - UI 输出栏新增“GPU 质感”勾选框（默认开，随设置保存），预览、渲染、队列带同一条参数。
- **实测（本机 Arc A750，2.6 MP，ProRes 422 HQ，全质感）**：
  - 视频 **382.7 → 245.6 ms/帧**（1.56×）；`demo-gpu-texture` 两段 2 秒 **386.8 → 233.6 ms/帧**（1.66×）
  - 质感部分本身 **约 160 → 约 38 ms/帧**
  - 单帧 `lut apply`（含进程启动与着色器编译）309.5 → 214.5 ms
  - 与 CPU 参考 PSNR：真实负片帧 **123.1 dB**；`test_gpu_texture` 九组配置 **143–166 dB**，最差偏差 4.5e-7
  - 编码后两段视频 PSNR 83.6 dB（差异来自编码器，不是质感链）
- **后果**：
  - 颜色与质感都在 GPU 后，视频下一瓶颈是 ffmpeg 解码 / 编码与 16-bit 往返（空管道约 207 ms/帧）
  - GPU 质感首次调用要编译 11 个着色器，单张图片会多约 0.2 s；批量/视频可忽略
  - 分段渲染签名新增 `texture-engine`，旧分段会重渲而不是和不同后端混用
  - macOS 没有 D3D11，继续用 CPU 质感（自动回退）

---

## D048 · 预览实时化（`preview` 常驻进程 + 整段帧缓存）

- **日期**：2026-11-02
- **背景**：颜色和质感都上了 GPU 后，UI 预览仍然是每次 “ffmpeg 抽帧 → 写 16-bit PNG → CLI 进程 → 读 PNG”，实测 **1.7–2.2 s/帧**，拖时间线完全没有手感。
- **先把时间量清楚**（2.6 MP 素材）：
  - ffmpeg 抽一帧并写 16-bit PNG：约 1000 ms
  - CLI 每次启动解析 `.cube` 文本（35,937 行）：约 215 ms
  - 16-bit PNG 读 + 写：约 600–700 ms（写约 250 ms）
  - D3D11 设备 + 着色器编译：150–700 ms（每次进程）
  - 真正的颜色 + 质感计算只有约 50 ms
- **实现**：
  - 新增 `spektra-roll-cli preview`（D048）：常驻进程，行协议走 stdin/stdout（`look` / `open` / `backend` / `texture` / `frame` / `raw` / `quit`）。
  - `open` 一次性把整段解码进内存（默认上限 1024 MB、最长边 1600 px），`frame N` 只做 16→float、颜色、质感、8-bit RGBA 打包。
  - 预览输出改成 **8-bit RGBA 原始帧**，UI 用 `QImage::Format_RGBA8888` 直接接管，不再有 PNG 编解码。
  - 换 look 只重载 LUT / 重配模型，**不丢帧缓存**；`backend gpu|cpu` 对应界面“GPU 质感”勾选框。
  - 时间线拖动改为 90 ms 合并，拖动中每约 90 ms 出一帧（约 11 fps），松手再补一帧；超出缓存的帧在拖动时直接跳过，不走慢路径。
  - `probeGeometry` 改成一次 `ffprobe -show_streams`：原来单独查 `side_data=rotation` 在这支素材上要 **3.8 s**，合并后 **42 ms**。
  - 旧路径保留为回退（服务启动失败、超出缓存、无 ffmpeg 时）。
  - 新增 `--preview-selftest`：启动界面后跑两次真实预览并打印状态行，供脚本/CI/支持使用。
- **实测（本机，1600 × 898 预览，99 帧缓存）**：
  - LUT 预览：**1.7–2.2 s → 47–64 ms/帧**（约 30×）
  - 光谱 GPU 预览：**→ 43 ms/帧**
  - 首次打开素材：2–4 s（解码整段，一次性，取决于磁盘）；之后换曝光/质感不再解码
  - 内存：99 帧 × 8.6 MB ≈ **853 MB**（`--cache-mb` 可调）
- **后果**：
  - 预览和渲染仍然走同一套 C++ 管线，但预览是**降分辨率**（最长边 1600）且 8-bit 显示；状态栏会写明尺寸与耗时
  - 长片超过缓存预算时，超出的帧回退到单帧路径（状态栏提示“超出缓存，单帧”）
  - `preview` 依赖 stdin/stdout 行协议，Windows 与 POSIX 都用 `popen` 解码，macOS 可复用

---

## D049 · 颜色 + 质感合并成一次 GPU 提交

- **日期**：2026-11-03
- **背景**：D047 之后视频是 233.6 ms/帧，但颜色和质感是两次独立提交，各自上传 42 MB、回读 42 MB；空管道（只解码 + 编码）约 207 ms/帧，说明 GPU 那两段已经比管道贵不了多少。
- **实现**：
  - `GpuFilm::attach()` / `GpuTexture::attach()`：质感后端可以共用 film 后端的 D3D11 设备（所有权仍在 film）。
  - `GpuFilm::processToDevice()` + `readback()`：拆开“上传 + dispatch”和“回读”，融合路径不再读回中间结果。
  - `GpuTexture::processBuffer()`：直接以 SRV 读 film 的输出 buffer，先做一次 GPU 内拷贝进自己的 ping-pong 缓冲，再跑原来的质感链，最后只回读一次。
  - 只有“颜色在 GPU + 质感在 GPU + 至少一个质感项开启”时才走融合；否则仍是原路径。
  - 任何一步失败都回退到分段提交，并在 stderr 打一次 warning。
- **过程中修掉的三个坑**：
  - film 的输出 buffer 原来只有 `BIND_UNORDERED_ACCESS`，补上 `BIND_SHADER_RESOURCE` 才能被质感链读。
  - film dispatch 之后没有解绑 UAV，质感再以 SRV 读同一 buffer 会读到 0（整帧全黑）。
  - `attach()` 之后 `initialize()` 只检查了 device 就提前返回，着色器没编译 → 访问冲突；改成按“着色器是否已编译”判断。
- **实测（本机 Arc A750，2.6 MP，ProRes 422 HQ，全质感）**：
  - 视频 **233.6 → 207.1 ms/帧**（1.13×）；相对 D046 的 379 ms/帧累计 **1.83×**
  - 编码后输出与 D047 的分段路径 **逐位相同**（ffmpeg PSNR = inf）
  - 新增 `tests/test_gpu_chain.cpp`：融合路径 vs CPU 参考 **124.1 dB**，最差 6.9e-6；确定性通过
  - 预览（光谱 GPU + GPU 质感）**43 → 38 ms/帧**
- **后果**：
  - 颜色 + 质感已经基本被解码/编码管道掩盖（207 ms 就是空管道水平）；下一步想再快只能动 ffmpeg（硬件解码 / 并行编码）
  - 融合路径依赖两个后端同设备；`--engine lut` + `--texture-engine gpu` 仍是分段路径
  - `test_gpu_chain` 和另外两个 GPU 测试一样，无 D3D11 时 exit 77

---

## D050 · 视频管道修复（`std::lround` 是最大的单项开销）

- **日期**：2026-11-03
- **背景**：D049 之后视频是 207.1 ms/帧，怀疑是 ffmpeg 管道。给循环加分段计时后，结论完全不同：
  ```
  timing: read 13  convert 4  process 44  pack 130  write 6 (ms/frame)
  ```
- **原因**：RGB48 量化循环用了 `std::lround`，2.6 MP 每帧 780 万次库调用，**130 ms/帧**，比整个 GPU 颜色 + 质感（44 ms）还贵约 3 倍。预览服务端 D048 里已经踩过同一个坑。
- **修改**：
  - 量化改成 `static_cast<unsigned short>(value * 65535.0f + 0.5f)`；
  - RGB48→float 与 float→RGB48 两个整帧循环改成 `sr::parallelChunks`；
  - 新增 `SPEKTRA_VIDEO_DEBUG=1` 分段计时（read / convert / process / pack / write）。
- **硬件解码（测量后不采用）**：
  - 软解 2160×1214 HEVC 4:2:2 10-bit → rgb48：**19.5 ms/帧**
  - `-hwaccel qsv -hwaccel_output_format qsv` + `hwdownload,format=y210le` + scale：**21.8 ms/帧**
  - `-hwaccel d3d11va`：**23 ms/帧**
  - 下载与格式转换比省下的解码时间更贵，所以没有把 `--hwdecode` 做进 CLI（探测命令还必须带上完整滤镜链，否则会“探测成功、渲染失败”）。
- **实测**：
  - 视频（光谱 GPU + GPU 质感，2.6 MP，ProRes）：**207.1 → 85.1/91.9 ms/帧**（2.4×）
  - `demo-m3`（LUT + CPU 质感）：358.7 → **约 235 ms/帧**
  - 输出与修改前 **逐位相同**（ffmpeg PSNR = inf）
  - 编码器现在是可见成本：ProRes 85、H.264 + QSV **77**、H.264 软编 104、HEVC 软编 104 ms/帧（QSV 编码终于有意义了）
- **后果**：
  - 剩下的时间主要是 GPU 44 ms + 读 14 + 写 7；分段并行或读写线程重叠还能再省 15–20%，留给下一轮
  - `--encoder qsv` 从“收益有限”变成 H.264 下约 1.35×，建议 H.264 输出默认尝试

---

## D051 · 解码 / 编码与处理重叠（读写线程）

- **日期**：2026-11-03
- **背景**：D050 之后视频 85–92 ms/帧，分段计时是 `read 14 / convert 9 / process 44 / pack 5 / write 7`——解码和编码各让循环空等十几毫秒，而 ffmpeg 本来就在独立进程里。
- **实现**：
  - `FramePipeReader`：一个后台线程把下一帧读进双缓冲，主循环 `next()` 只等“已经就绪”的帧；解码与 GPU 处理重叠。
  - `FramePipeWriter`：两个槽位 + 后台写线程，主循环把打包好的帧交出去就继续下一帧；`finish()` 先排空再 join，失败标记传回。
  - 两个线程都在关闭管道之前停止/join，避免读写已关闭的 `FILE*`。
  - 计时项从 `read/write` 改成 `wait`（等读线程的时间）。
- **实测（2.6 MP，ProRes，全质感）**：
  - 视频 **85–92 → 79.2 ms/帧**；`timing: wait 4 convert 11 process 48 pack 5`
  - 与重叠前输出 **逐位相同**（PSNR = inf）
  - `demo-m3`（LUT + CPU 质感）245.5 → **226.8 ms/帧**
  - 分段 + 续渲不受影响（48 帧 2 段 84.4 ms/帧，`part 1/2: already done`）
  - 相对 D046 的 379 ms/帧累计 **4.8×**
- **后果**：
  - 现在 79 ms 里 48 ms 是 GPU 颜色 + 质感，剩下是转换/打包与等待；再往下只剩并行分段（多 worker）或减小 GPU 工作量
  - 线程只在一次 `renderRange` 内存在，分段之间重新创建，行为与以前一致
  - `SPEKTRA_VIDEO_DEBUG=1` 现在打印 `wait / convert / process / pack`

---

## D052 · 并行分段渲染（`--jobs`）

- **日期**：2026-11-03
- **背景**：D051 之后单帧 79 ms，分段还是顺序渲染。长片/队列本可以并行，实测 CPU 路径（LUT + CPU 质感）单段约 220 ms/帧，是典型的可并行负载。
- **实现**：
  - 每个 worker 持有一份 `VideoRender` 副本、自己的 `GpuFilm` / `GpuTexture`（GPU 后端刻意不做线程安全，设备也不共享）和自己的 `GrainFieldCache`。
  - worker 从 `std::atomic` 计数器领段；`parts` 预分配到 index，进度打印与错误收集各自加锁。
  - 默认 `--jobs 0` 自动取 `min(3, 核数/4, 段数)`；`--jobs 1` 回到顺序。
  - 语义不变：每段的帧号仍用全局基址（颗粒连续）、签名/续渲标记/音频只复制一次都不动。
- **实测（30 秒素材，6 段 × 5 秒，ProRes 422 HQ）**：
  - LUT + CPU 质感：**157.98 → 84.19 s（1.88×）**
  - 光谱 GPU + GPU 质感：53.17 → 48.42 s（1.10×）——三次 GPU pass 抢同一张卡，48 ms 的 GPU 时间是串行资源
  - 4 秒素材按 1 秒分段（5 段）时只有 1.12×：每段的 seek / ffmpeg / 设备初始化固定成本占了大头
  - 两种配置的输出都与顺序渲染 **逐位相同**（PSNR = inf）
  - 并行续渲正常：9 段里删掉第 3 段重跑，其余 8 段 `already done`，只补 1 段
- **后果**：
  - CPU 渲染（LUT / CPU 光谱）收益明显；GPU 渲染受限于单卡，收益主要是把转换/打包/IO 重叠掉
  - 每个 worker 一套显存与内存（2.6 MP 每 worker 约 400 MB 显存）；显存紧张的机器用 `--jobs 1`
  - 分段越短，固定成本占比越高；长片用 5–10 秒一段最划算

---

## D053 · 长片预览磁盘代理

- **日期**：2026-11-03
- **背景**：D048 的内存缓存用“按预算降分辨率”来装下整段；30 秒素材只剩约 640 px，几分钟的片子会降到缩略图级别。超出的帧原本还会回退到 1.7 s 的单帧路径。
- **实现**：
  - `open` 时如果“整段进内存”把最长边压到 960 px 以下（或根本装不下），就生成一张 **半分辨率、全 I 帧代理**（最长边 960、x264 `-g 1 -preset veryfast -crf 18`），放在 `out/ui-proxy/<hash>.mov`，key 含素材路径/修改时间/尺寸/代理尺寸。
  - 代理已存在就直接复用；`frame N` 越出内存缓存时，用 `ffmpeg -ss` 从代理取单帧（全 I 帧，seek 快），再走同一套颜色/质感。
  - 代理失败时回退到内存缓存，不硬失败。
  - 协议里 `frame` 的回复带上实际宽高（内存帧与代理帧尺寸可能不同），UI 按回复尺寸加载；`open` 的 `total` 现在是可预览总帧数。
- **实测（本机）**：
  - 30 秒 / 722 帧素材：代理生成 **8.7 s、30.6 MB**；之后预览 **960 × 540**，单帧 **100 ms**（原先内存缓存只能给约 640 px）
  - 重新打开同一素材：`open` **48 ms**（代理直接复用）
  - 4 秒素材不受影响：仍走内存缓存 **1600 × 898 · 47 ms**
  - `--preview-selftest` 两次预览分别 539 / 100 ms（长片）与 2194 / 47 ms（短片）
- **后果**：
  - 长片第一次打开会多一次代理生成（约 0.3× 实时长度），之后秒开；代理文件留在 `out/ui-proxy/`，可随时删
  - 代理是 8-bit 4:2:0 的半分辨率预览源，只影响“超出内存缓存”的那部分帧的画面细节，不影响渲染
  - 代理尺寸固定 960，换来一致的预览尺寸与可预期的取帧时间

---

## D054 · 真实点扩散核（物理单位 + 重尾形状）

- **日期**：2026-11-03
- **背景**：光晕 / 眩光 / 扩散从 D033–D037 起就是“画幅比例半径 + 三次盒状模糊”。它看起来像高斯，和真实胶片/镜头的关系只有“大概那么宽”：半径随画幅宽度走（换长宽比或裁切就变），核形状也没有真实 PSF 的宽尾巴。D047 把这三个 pass 逐行搬到 GPU 后，形状问题被原样复制。
- **实现**：
  - **物理单位**：三个效果各加一个 `spreadMicrons`，表示在底片上的有效 σ（微米）。像素换算 = `µm × width / frameWidthMm`，`--frame-width-mm` 默认 36（135 全画幅）；Super 35 用 ~24.9。于是 1080p / 4K / 8K 渲染出来是同一个物理大小。
  - **重尾核**：每个效果是“窄核 + 宽裙”的双高斯混合，按有效 σ 归一化（保持总展宽不变，只改形状）：
    - 光晕 `{(0.88, 0.69), (0.12, 2.20)}`、眩光 `{(0.90, 0.49), (0.10, 2.80)}`、扩散 `{(0.82, 0.713), (0.18, 1.80)}`；
    - 截断在**最宽分量**的 3σ（不是有效 σ 的 3σ），所以 3σ 之外的裙部能量是同等方差高斯的 **7–10×**，这正是胶片基材散射、镜头纱光和 Pro-Mist 长拖尾的来源。
  - **成本控制**：`spreadDownsampleFactor` 按核半径选 1–8 倍降采样，再把核按同样比例缩小到低分辨率；仍是“降采样 → 分离卷积 → 双线性升采样”，和 D037 的思路一致，但物理宽度不变。
  - **CPU/GPU 同一组权重**：核在 CPU 上由 `sr::buildSpreadKernel` 生成（`spread.hpp` 里的 `SpreadKernel`），GPU 通过 **structured buffer t3** 上传同一组 float，而不是在 HLSL 里重算 exp。踩到的坑：HLSL 常量缓冲里的数组是 **16 字节/元素**，先按 4 字节排进去导致 parity 掉到 19–48 dB。
  - **卷积顺序**：一行/一列一个线程，tap 从 `-r` 到 `+r` 顺序累加，和 CPU 的 `convolveSeparable` 完全一致 → parity 反而比盒状模糊时代更高。
  - **可回退**：`--spread-model box` 保留 D033–D037 的“画幅比例 + 盒状模糊”路线，CPU/GPU 两条老代码都没删，用于对照和旧分段签名。
  - **配套**：`--halation-spread 225`、`--glare-spread 725`、`--diffusion-spread 122`、`--frame-width-mm 36`；渲染签名加上 `spread ...` 一行（换模型/尺度不会混着续渲）；预览协议 `texture` 后面新增两个数（1=物理 / 0=盒状、底片宽度），UI 已同步。
- **实测（本机，2160×1214）**：
  - 默认值下的有效 σ 与旧盒状模糊一致（225 µm ≈ 13.5 px、725 µm ≈ 43.5 px、122 µm ≈ 7.3 px），所以观感强度不变，只改形状。
  - 同帧 PSNR（box vs physical，全效果）：**57.03 dB**——差异集中在高光边缘的柔化与外晕，不是整体曝光变化。
  - GPU 视频（光谱 GPU + GPU 质感，ProRes）：**82.2 ms/帧**（D051 时 79.2，+3.8%）。
  - LUT + CPU 质感视频：box 231.8 → physical **251.4 ms/帧**（+8.5%）；单帧 PNG 275.7 → 297.6 ms；`demo-m3` 220.8 → **235.1 ms/帧**。
  - `demo-gpu-texture`：CPU 236.7、GPU **82.6 ms/帧（2.87×）**，编码后 PSNR **85.6 dB**。
  - `test_gpu_texture` 九组配置 **149.3–177.7 dB**（最差 2.98e-7），比 D047 的 143–166 dB 更稳。
  - `test_texture` 新增：物理尺度随分辨率线性、核归一化、对称、3σ 外裙部 > 1%、box 路线仍可用。
- **后果**：
  - 空间效果第一次有了“可解释的尺寸”，4K 和 1080p 不会再因为画幅比例不同而看起来不一样。
  - 默认物理核比盒状模糊宽裙更长、芯更窄，高光边缘更“贴边 + 外晕”；如果观感偏软，用 `--halation-spread` 等调小，或 `--spread-model box` 回到旧观感。
  - 段签名已包含模型与尺度，旧 `.parts` 会自动重渲而不是混用。
  - 仍不是严格的物理测量：双高斯只是对真实 PSF 的近似，没有按胶片层厚/折射率计算；下一步若要更真，需要实测的 halation/glare PSF 曲线。

---

## D055 · 浮点交付（OpenEXR）

- **日期**：2026-11-03
- **背景**：D017 把 OpenImageIO / EXR 推后时留了一句“等真正需要浮点的时候”。现在颜色、质感、HDR 都齐了，唯一的交付还是 16-bit PNG：`toShort()` 把 0..1 之外全部切掉，HDR / 高光 / 后续调色都没有余量。DaVinci / Nuke / 合成也默认吃 EXR。
- **实现**：
  - **自带最小 OpenEXR**（`src/core/src/image.cpp`）：scanline、NONE 压缩、RGB 三通道、FLOAT32 或 HALF，读写都是自己拼，不引入新依赖。头里写 `chromaticities`（sRGB/Rec.709 + D65）。
  - **输出格式按扩展名走**：`--out *.exr` 一律写 EXR（默认 float，`--exr-half` 写 half），`bits` 不参与；`.png` 仍只接受 8/16，`--bits 32 --out x.png` 明确报错。
  - **EXR 输入**：`loadImage` 认 `.exr`（NONE 压缩，FLOAT/HALF，任意通道顺序，必须有 R/G/B），`sourceBits` 报 32；PNG 输入 32-bit 时输出默认降到 16（PNG 装不下）。
  - **序列**：`lut apply --in-dir/--out-dir --out-ext exr` 把一整段 PNG 序列转成 float EXR；不加 `--out-ext` 就保持输入扩展名。
  - **不钳制**：`convert` 在写 EXR 时不再把结果夹到 0..1（写 PNG 仍夹），所以 `--exposure 3` 的超白能留下来。
  - **可见的交代**：写 EXR 时打印 `range: min .. max (x% below 0, y% above 1, written unclipped)`。
  - **测试 / CI**：`test_image` 覆盖 float 精确往返、half 精度往返、±越界值保留、16-bit PNG 对照钳制、`--bits 32` + PNG 报错；CI 新增 EXR 冒烟（写 EXR → 自己读回 → 出 PNG），Windows + macOS 都跑。
- **实测（2160×1214）**：
  - float EXR **30.0 MB**、half EXR **15.0 MB**、16-bit PNG **15.0 MB**（PNG 有压缩，EXR 目前没有）。
  - 写 EXR 与写 16-bit PNG 的耗时基本一样（同一帧 295 vs 293 ms，含颜色 + 质感）。
  - 正常帧：EXR 读回再写 16-bit PNG 与原 PNG **PSNR = inf**（逐位相同）。
  - 独立解码器校验：ffmpeg 解 float EXR → PNG 与原图 **92.6 dB**（1 LSB 级舍入），half EXR **82.8 dB**。
  - `convert --from apple-log --exposure 3`：EXR 范围 **0.0000 .. 5.3438**，**32.973%** 的像素 >1 且保留；同参数 PNG 全部切到 1。
- **后果**：
  - 浮点交付有了，但仍不是“场景线性 EXR”：样本是 **display-encoded**（sRGB 或 PQ/HLG 码值），只是不钳制。导入 Resolve/Nuke 时要设对应的输入变换，不要当线性素材用。
  - 不压缩是为了少一个依赖；2.6 MP 一帧 30 MB，长序列会很大。half 是一半，日常交付够用。
  - 下一步若要做真正的线性交付，应该加 `--exr-linear`（按输出 TRC 反解）或直接接 OCIO，而不是偷偷改现有定义。

---

## D056 · macOS 的 GPU 颜色（Metal）

- **日期**：2026-11-03
- **背景**：D046/D054 之后 Windows 的颜色和质感都在 GPU，macOS 还只有 CPU 光谱：2.6 MP 约 **1.08 s/帧**，比 Windows 的 50 ms 慢 20 倍。产品负责人有 MacBook Air M1，要求把 macOS 的 Metal 路径补上。
- **范围**：这一步只搬**颜色（光谱 FilmModel）**，这是最大头；空间质感仍是 CPU（约 160 ms/帧），下一个 D057 再上 Metal。CLI/UI 的行为不变：`--engine spectral-gpu` 在 macOS 上从“回退 CPU”变成真的跑 Metal。
- **实现**：
  - 新增 `src/cli/gpu_film_metal.mm`（Objective-C++，仅 Apple 编译），替换 `gpu_film.cpp`；实现同一个 `GpuFilm` 接口，所以 CLI、预览服务端、UI、测试都不改调用方式。
  - 着色器是 `gpu_film.cpp` 里 HLSL 的逐行 MSL 翻译；**运行时用 `newLibraryWithSource:` 编译**，不做 `.metallib` 构建步骤、不加第三方依赖。
  - 常量缓冲用与 C++ `Params` 完全相同的**标量数组**布局（`float x[4]` 而不是 `float4`），避免 16 字节对齐带来的悄悄错位；`static_assert` 之外靠 parity 测试守住。
  - 输入/输出/常量都用 `MTLResourceStorageModeShared`（Apple silicon 统一内存），`dispatchThreads` 一次提交，`waitUntilCompleted` 后直接 memcpy 读回，不需要 staging。
  - **`MTLCreateSystemDefaultDevice()` 可能返回 nil**（GUI 会话/多 GPU/虚拟机/后台会话都会遇到），所以先取默认设备，失败再用 `MTLCopyAllDevices()` 取列表里的第一块；两者都没有才回退 CPU。
  - 新增 `spektra-roll-cli gpu-info`：打印 macOS 版本、架构、`MTLCopyAllDevices` 的每块设备（名称 / low power / headless）、默认设备是否为 nil，以及光谱/质感两条 GPU 链是否可用。排障时先看这一条。
  - CMake：Apple 上 `enable_language(OBJCXX)`、按平台选择 `gpu_film_metal.mm` / `gpu_film.cpp`、链接 Metal + Foundation、给 `.mm` 加 `-fobjc-arc`。
  - 没有 Metal 设备时 `available()` 返回 false，调用方照旧回退 CPU 光谱，不硬失败。
- **踩到的坑（重点）**：
  - **Metal 默认开 fast math**，编译器会假设没有 NaN，把光谱求和里 `density != density` 的 NaN 保护直接优化掉。光谱表里有几条波长的密度本来就是 NaN（CPU 和 D3D11 都靠这个判断跳过），结果**整帧变 NaN**：普通输出是 NaN，负片路径被 `clamp` 变成全 0。修法是编译 Metal 库时 `MTLCompileOptions.fastMathEnabled = NO`。
  - **真机上的 `MTLCreateSystemDefaultDevice()` 返回 nil**：产品负责人的 MacBook Air M1（Metal 3）上 `verify-macos.command` 报 “no Metal device”，三个 GPU 测试全 skip。默认设备 API 在 macOS 上不是“有 GPU 就一定给”，所以加了 `MTLCopyAllDevices()` 兜底，并用 `gpu-info` 把设备列表打出来；如果两台都用不了，至少能一眼看出是“没枚举到”还是“枚举到但没选上”。
  - 这个 fast-math 坑在 Windows 上完全复现不了，只能靠 macOS CI 的 `gpu_film` 失败输出定位；测试现在会在失败时打印逐像素 GPU/CPU 值并显式检测 NaN。
  - `test_gpu_chain`（融合提交）在没有 Metal 质感后端时会 attach 失败；现在先查 `GpuTexture::available()`，没有就 skip（D057 之后会自动开始跑）。
- **实测**：
  - macOS CI（run **80**）：`gpu_film` 从“3.1 s、NaN/偏差 1.0 失败”变成通过；`gpu_texture` / `gpu_chain` 按预期 skip；Windows 全绿。
  - 本机（Windows）不受影响：9/9 测试、GPU 测试仍真跑；`gpu-info` 输出 `D3D11 feature level 0xB000 / Intel Arc A750 / spectral-gpu: available`。
  - 真机（MacBook Air M1，macOS 14.7.5 arm64）：`MTLCreateSystemDefaultDevice()` 仍然返回 nil，但 `MTLCopyAllDevices()` 找到唯一一块 **Apple M1**，兜底生效——`gpu_film` 六组 parity 最差 **2.99e-4**（容差 2e-3），320×180 的 CPU vs Metal **PSNR 115.44 dB**；同尺寸单帧 10.8 → 7.1 ms（这个尺寸主要耗在 PNG 读写，不代表 2.6 MP 的速度）。
  - `MTLCreateSystemDefaultDevice()` 在 macOS 上返回 nil 并不是“没有 Metal”：Metal 3 完全可用，只是默认设备 API 没给，必须再用设备列表兜底。
  - **2.6 MP 的 Mac 速度还没有数字**：CI 只跑 17 个像素的 parity，真机验收也只跑了 320×180。要完整对比请在 Mac 上用 `video` 命令渲染一段真实素材。
- **后果**：
  - macOS 的 `--engine spectral-gpu` 现在是真的 GPU 颜色；UI 里“光谱 GPU（物理，快）”在 Mac 上不再是 CPU 回退。
  - 质感仍在 CPU，所以 Mac 的整帧时间 ≈ Metal 颜色 + 160 ms 质感；D057 把质感搬上去之后才是完整对齐 Windows。
  - Metal 着色器没有 `.metallib` 缓存，首次 configure 会编译一次（几分之一秒），之后复用 pipeline。

---

## D057 · macOS 的 GPU 质感（Metal）

- **日期**：2026-11-03
- **背景**：D056 让 macOS 的颜色走 Metal，但颗粒 / 光晕 / 眩光 / 扩散还在 CPU（约 160 ms/帧），融合提交在 Mac 上也用不了。这一轮把 `gpu_texture.cpp` 的 14 个 compute pass 全部翻成 MSL，并重新打开融合路径。
- **实现**：
  - 新增 `src/cli/gpu_texture_metal.mm`（Objective-C++，仅 Apple 编译），实现同一个 `GpuTexture` 接口；CMake 在 Apple 上用它替换 `gpu_texture.cpp`。
  - 14 个 kernel 是 HLSL 的逐行翻译：光晕阈值、亮度、盒子模糊（一行/一列一个线程，累加顺序不变）、物理 PSF 卷积、降采样/升采样、颗粒场与叠加、三个 composite 和 copy。
  - **整条链编码进一个 command buffer**，只在最后 `waitUntilCompleted` 一次；参数用 `setBytes` 内联，PSF 的 taps 也内联（最大 241 个 float ≈ 964 B，在 4 KB 限制内）。这样不会出现“共享常量缓冲被下一帧/下一个效果覆盖”的竞态。
  - 融合路径（D049）：`processBuffer()` 直接借用 film 后端的 `MTLBuffer` 作为第一遍输入，**不拷贝 42 MB**；第一遍之后把 ping-pong 的另一块重新指向自己拥有的缓冲，绝不写回 film 的结果。
  - 输入/输出/scratch 都是 `MTLResourceStorageModeShared`；kernel 与 film 一样用 `fastMathEnabled = NO` 编译，保证与 CPU 参考的浮点顺序一致。
  - 没有 Metal 设备时 `available()` 返回 false，调用方回退 CPU 质感。
- **踩到的坑**：
  1. **`dispatchThreads` 收的是“总线程数”，不是 D3D 的 group 数**。照搬 D3D 的 `(count+63)/64` 会把线程数缩小 64 倍；改用 `dispatchThreadgroups:` 并显式给 threadgroup 尺寸，行/列 pass 分别为 `(1,64)` 和 `(64,1)`。
  2. PSF taps 一开始放在共享 `MTLBuffer` 里，一个 command buffer 内的多个效果会互相覆盖；改成 `setBytes` 内联。
  3. 融合路径第一遍读 film 的缓冲，交换后不能把它当成写入目标；否则会把 film 结果覆盖掉。
  4. ARC 下不能对 `id` 数组用非 const 引用遍历（编译错误），改成索引循环。
  5. `fastMathEnabled` 在 macOS 15 SDK 里标记弃用（建议 `mathMode`），目前只是警告；为了兼容旧系统继续使用。
- **实测**：
  - macOS CI（run **85**）：9/9 全过，`gpu_texture` **2.11 s**、`gpu_chain` **2.10 s**（之前是 skip），说明 Metal 质感与融合提交都在真机上跑了。
  - Windows 本机：9/9 全过，D3D11 路径未受影响。
  - 真机 M1（macOS 14.7.5）：`test_gpu_texture` 九组配置全部 **逐位相同**（worst 0.0，PSNR 999 = 无差异）——质感链只有加/乘/比较，没有超越函数，Metal 关掉 fast math 后与 CPU 完全一致。
  - `test_gpu_chain`：融合 vs CPU **134.80 dB**（worst 3.67e-6），剩余差异来自 film 的超越函数，不是质感。
  - 320×180 单帧：CPU 11.5 ms → Metal **5.6 ms**；CPU vs Metal 光谱 **115.44 dB**。2.6 MP 的 ms/帧待测。
- **后果**：
  - macOS 现在颜色和质感都在 GPU，`--texture-engine auto` 在 `--engine spectral-gpu` 下自动用 Metal 质感，融合提交在 Mac 上重新生效。
  - Mac 的整帧时间不再固定多出约 160 ms 的 CPU 质感；实际收益要等 2.6 MP 实测。
  - `gpu-info` 在 Mac 上现在会显示 `texture-gpu: available`。

---

## 待决策（还没问你）

- D010：是否做 OFX 插件（建议做，第二期）
- D011：Apple Developer 账号什么时候买
