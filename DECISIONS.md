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
  - 真机（MacBook Air M1，macOS 14.7.5 arm64）：`MTLCreateSystemDefaultDevice()` 仍然返回 nil，但 `MTLCopyAllDevices()` 找到唯一一块 **Apple M1**，兜底生效——`gpu_film` 六组 parity 最差 **2.99e-4**（容差 2e-3），320×180 的 CPU vs Metal **PSNR 115.44 dB**；同尺寸单帧 10.8 → 7.1 ms（这个尺寸主要耗在 PNG 读写）。
  - 2.6 MP 单帧（2160×1214，含 16-bit PNG 读写）：CPU 光谱 **424.3 ms** → Metal **129.0 ms（3.3×）**。
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
  - 2.6 MP 单帧（颜色都用 spectral-gpu，含 PNG 读写）：质感 CPU **253.6 ms** → Metal **176.6 ms**；减去同一颜色时间（129.0 ms）后，质感本身约 **125 → 48 ms（约 2.6×）**。
  - 320×180 单帧：CPU 11.5 ms → Metal **5.6 ms**；CPU vs Metal 光谱 **115.44 dB**。2.6 MP 的 ms/帧待测。
- **后果**：
  - macOS 现在颜色和质感都在 GPU，`--texture-engine auto` 在 `--engine spectral-gpu` 下自动用 Metal 质感，融合提交在 Mac 上重新生效。
  - Mac 的整帧时间不再固定多出约 160 ms 的 CPU 质感；实际收益要等 2.6 MP 实测。
  - `gpu-info` 在 Mac 上现在会显示 `texture-gpu: available`。

---

## D058 · 线性浮点交付（`--exr-linear`）

- **日期**：2026-11-03
- **背景**：D055 的 EXR 是 **display-encoded**（sRGB 或 PQ/HLG 码值）只是不钳制。调色/合成软件默认把 EXR 当线性读，直接用就会多套一次传输函数；上一版只能在文档里提醒“别当线性用”。这一轮让浮点交付可以选择真正的线性光。
- **实现**：
  - 新增 `--exr-linear`，配合 `--exr-trc srgb|pq|hlg`（默认 srgb）和 `--peak N`（默认 1000，仅 PQ 用）。
  - 核心函数 `sr::linearizeImage(Image&, FloatTransfer, peakNits)`：把显示编码样本反解成线性光，**1.0 = 参考白**——sRGB/HLG 是显示白，PQ 是 `--peak` 尼特。sRGB 解码**不钳制**，所以超白（>1 的码值）保留成更大的线性值。
  - 接到 `lut apply`（单帧和 `--in-dir` 序列）、`convert`、`gen`；输出不是 `.exr` 时直接报错（不静默忽略）。
  - 写盘前打印 `EXR: linear light (...)`，读取范围也随之变成线性值的范围。
  - 测试：sRGB 0.5 → 0.21404、1.5 的码值 → 2.53（超白保留）、PQ 1000 nits 的码值 → 1.0、HLG 0.25 往返；未知 transfer 名报错。
- **实测（2160×1214）**：
  - 同一帧 display-encoded 范围 0.0160 .. 0.8870 → linear 范围 **0.0012 .. 0.7619**（1.0 = 白）。
  - `convert --exposure 3`：display 最大 5.34 → linear 最大 **50.3**，仍然不钳制。
  - 体积、写盘耗时与普通 EXR 相同（float 30 MB / half 15 MB）。
- **后果**：
  - 交付有两种明确选择：默认 display-encoded（进 Resolve 设 sRGB/PQ 输入变换），或 `--exr-linear`（按线性解读）。
  - 仍然不是 OCIO：没有 ACES/相机矩阵/显示变换配置；要完整色彩管理还得接 OCIO。
  - 线性 EXR 的 `chromaticities` 仍是 sRGB/Rec.709 原色；HLG/PQ 的“1.0 = 白/峰值”是项目约定，已写进 `--help` 和 RUN。

---

## D059 · EXR ZIP 压缩（默认）

- **日期**：2026-11-03
- **背景**：D055 的 EXR 是不压缩 scanline，2.6 MP 一帧 30 MB，长序列和归档都很占地方。ZIP 是 OpenEXR 的标准无损压缩之一，所有常见软件都支持。
- **实现**：
  - 默认改为 **ZIP 压缩**（16 条 scanline 一个 chunk），`--exr-raw` 回到不压缩。
  - 压缩直接用 stb 的 zlib 实现（16-bit PNG 早就在用，没有新依赖），解压用 `stbi_zlib_decode_malloc_guesssize_headerflag`。
  - ZIP 的预处理与 OpenEXR 的 `ImfZip` 一致：**偶数字节放前半、奇数字节放后半**，再做逐字节差分；解压反向。
  - 每个 chunk 压不动时按规范回退成未压缩块（`packedSize == rawSize`），读取端两种都认。
  - 读取器现在同时支持 `NONE` 和 `ZIP`（以及混合：部分 chunk 未压缩）。
- **踩到的坑**：
  - 一开始把“偶数/奇数拆分”和“前后半合并”的方向写反了。**自己读自己完全正常**（对称），但 ffmpeg 解出来只有 **7.2 dB**；修正后 ffmpeg 解码 **91.5 dB / 92.6 dB**，与未压缩一致。
  - 这说明“自洽的往返测试”不能证明文件格式正确，必须用第二个解码器交叉验证。
- **实测（2160×1214，同一台机器）**：
  - ZIP 的收益**高度依赖内容**：
    | 内容 | 未压缩 | ZIP | 变化 |
    |---|---|---|---|
    | 已调色帧（identity LUT，无颗粒） | 31.49 MB | **22.73 MB** | −28% |
    | 带颗粒 / 光晕的成片（demo 那条链） | 31.49 MB | **31.47 MB** | ≈0% |
    | 半精度（同一成片） | 15.74 MB | **9.82 MB** | −38% |
    | 640×360 合成图表 | 2.77 MB | **38.7 KB** | −98.6% |
  - 结论：平滑 / 大面积色块收益大；**float 的颗粒会吃掉 zlib 的压缩率**，half 明显更好。
  - 写盘耗时：同一帧 16.0 → 22.6 ms（可忽略）；压不动的 chunk 自动存原始块，不会变大。
  - ffmpeg 独立解码 ZIP 文件与源图 **92.6 dB**（16-bit 往返舍入），我们的读取器逐位一致。
- **后果**：
  - 默认 ZIP，对任何内容都不会更大（压不动就存原始块），不需要用户选择。
  - 颗粒重的素材只省 20–30%；想再小需要 **PIZ / DWA**（更复杂的算法），本轮不做。
  - 文档里不再写“3–5 倍”这种没量过的数字，改成上表的实测。

---

## D060 · 多相机 Log 输入（S-Log3 / C-Log3 / V-Log）

- **日期**：2026-11-03
- **背景**：输入只有 sRGB 和 Apple Log。Sony / Canon / Panasonic 的 Log 素材要先用别处转成 sRGB 或 Apple Log 才能进管线，等于把最脏的一步留给用户。
- **实现**：
  - `InputTransfer` 扩成 `Srgb / AppleLog / SLog3 / CLog3 / VLog`；`--input` 接受 `s-log3`、`c-log3`、`v-log`（也接受无连字符写法）。
  - 曲线（逐行对齐 colour-science 0.4.7）：
    - **S-Log3**：`171.2102946929/1023` 分段，261.5 / 420 / 0.19 / 0.01 常数；
    - **C-Log3 v1.2**：0.097465473 / 0.15277891 三段，`×0.9` 的 reflection 输出；
    - **V-Log**：`0.181` 分段，`c=0.241514, d=0.598206, b=0.00873`。
  - 色域：S-Gamut3.Cine、Cinema Gamut、V-Gamut → 线性 Rec.709 的 3×3 矩阵，全部取自 `colour.matrix_RGB_to_RGB`（都是 D65，无色适应）。
  - CPU：`sr::decodeInputToLinearSrgb()` 统一做“曲线 → 矩阵 → 负值钳 0”，`FilmModel::transformFrom`、`convert`、预览服务端都改用它。
  - GPU：着色器里的 `appleLog` 布尔换成 `inputMode`（0–4），HLSL 与 MSL 都加同一组曲线和矩阵，保证 `--engine spectral-gpu` 对新输入同样有效。
  - UI：“画面类型”下拉增加 S-Log3 / C-Log3 / V-Log；选任何 Log 输入时自动勾上 DIR 耦合剂与 OkLCh 压缩。
- **验证**：
  - 三条曲线在 0..1 的 **101 点扫描与 colour-science 完全一致（max |diff| = 0）**；`s-log3` 18% 灰码值 0.410557185 → 0.180000000。
  - `test_transfer` 加入曲线参考值、完整线性 sRGB 转换（含负值钳制）与解析名检查。
  - `test_gpu_film` 加入 S-Log3 / C-Log3 / V-Log 三个 parity 用例；Windows 本机 CPU vs D3D11 **110.67 dB**（320×180）。
  - `lut bake --input c-log3` 正常出表。
- **后果**：
  - 四种 Log 素材可以直接进 LUT 烘焙、`lut apply`、视频、预览和 UI，不再需要外部转换。
  - 没有自动识别：`--input` 仍要用户选（或 UI 下拉），因为同一段素材的 Log 曲线无法可靠地从像素猜出来。
  - 色域按各家标准配对（S-Gamut3.Cine / Cinema Gamut / V-Gamut）；如果素材用的是别的 gamut（例如 S-Gamut3），需要以后加选项。

---

## D061 · 更多相机 Log + 容器矩阵跟素材走

- **日期**：2026-11-03
- **背景**：
  - D060 只覆盖了 Sony / Canon / Panasonic 各自的“标准搭配”，影视现场最常用的 **ARRI（LogC3/LogC4）** 和 **Blackmagic / DaVinci** 还进不来；Sony 也还有 **S-Gamut3（非 Cine）** 这一支。
  - 另一件事在多相机之后变成真问题：YUV → RGB 的矩阵原来按输入类型猜（apple-log → bt2020，其它 → bt709）。同一种 Log 可能被不同机器装进 bt709 或 bt2020 容器，猜错会让所有饱和色偏移（实测同帧 bt2020 vs bt709 只有 **44 dB**）。
- **实现**：
  - `InputTransfer` 追加 `SLog3SGamut3 / ArriLogC3 / ArriLogC4 / BmdFilmGen5 / DaVinciIntermediate`，**追加在枚举末尾**，D060 的 0–4 号 `inputMode` 不变。
  - 曲线（全部来自 colour-science 0.4.7，逐行照抄常数）：
    - **LogC3**：SUP 3.x、EI 800、Linear Scene Exposure Factor，`cut 0.010591 / a 5.555556 / b 0.052272 / c 0.24719 / d 0.385537 / e 5.367655 / f 0.092809`；18% 灰 = **0.3910068320**。
    - **LogC4**：`a 2231.8263090676883 / b 0.9071358748778103 / c 0.09286412512218964 / s 0.1135972086105891 / t -0.01805699611991131`；`E' >= 0` 走 `(2^(14*((E'-c)/b)+6) - 64)/a`，以下走线性延伸；18% 灰 = **0.2783958365**。
    - **Blackmagic Film Gen 5**：`A 0.08692876065491224 / B 0.005494072432257808 / C 0.5300133392291939 / D 8.283605932402494 / E 0.09246575342465753`，分段点 `D*0.005+E = 0.1338837831`；18% 灰 = **0.3835616438**。
    - **DaVinci Intermediate**：`M 10.44426855 / A 0.0075 / B 7 / C 0.07329248`，分段点 `0.02740668`；18% 灰 = **0.3360432724**。
  - 色域矩阵（`colour.matrix_RGB_to_RGB` → 线性 Rec.709，均为 D65）：
    - ARRI Wide Gamut 3、ARRI Wide Gamut 4、DaVinci Wide Gamut、S-Gamut3：不做色适应（白点就是 D65）。
    - Blackmagic Wide Gamut 的白点在 colour 里是 `(0.312717, 0.3290312)`，比 D65 多几位小数，所以这一条用 **CAT02** 适应到 sRGB 的 D65——中性灰才会精确保持中立。
  - `--input` 解析改成表驱动：大小写、连字符/下划线都可（`LogC3`、`arri_logc4`、`s_gamut3`），别名 `s-gamut3` / `bmd-film-gen5` / `blackmagic-film` / `davinci-intermediate`。错误信息统一用 `inputTransferList()`。
  - GPU：HLSL 与 MSL 各加四条曲线 + `inputMode` 5–9；**未知 mode 回退 sRGB**（D060 时最后一段默认落到 V-Log，加输入后会误用）。
  - `--in-matrix auto` 成为新默认：读素材的 `color_space` 标签（`bt2020nc` → bt2020，`bt709`/`smpte170m`/`bt470bg`/`smpte240m` 原样），标签缺失时回退 bt2020（与 CLI 以前一致）。`video` 与预览服务端都用它。
  - UI 的矩阵也改成读 ffprobe 的 `color_space`；但**标签缺失时仍按旧猜测**（apple-log → bt2020，其它 → bt709），所以没有标签的老素材观感不变。预览服务端把矩阵当启动参数，换了素材（标签不同）会自动重启服务端。
  - UI：“画面类型”下拉补齐 9 项；预览“按住看原片”对**任意** Log 输入都传 `--from <输入>`（以前只处理 apple-log）。
  - 顺带修一个 D060 遗留：分段签名加入 `input` 与 `matrix` 两行。以前用 `--engine spectral` 换输入再 `--resume` 会混用旧分段。
- **验证**：
  - 0..1 **101 点扫描**（本机编译出的 `sr::*Decode` 对比 colour-science）：S-Log3 / LogC3 / BMD Gen5 **相对差 0**；LogC4 / DaVinci Intermediate **≤ 2.3e-16**（最后一位舍入，我们的写法用 `exp2` 与它的 `2**` 顺序不同）。
  - `test_transfer`：四条曲线的码值参考点、五个输入到线性 sRGB 的混合值、18% 灰经矩阵后仍中立（≤3e-5，与既有 BT.2020 检查同量级）、解析名/别名/`isCameraLog`。
  - `test_gpu_film`：五个新 parity 用例，**worst ≤ 1.0e-6**。
  - 2.6 MP 单帧 CPU 光谱 vs D3D11（`--scan negative --couplers on --gamut oklch`）：**logc3 119.63 / logc4 118.06 / bmd-film 119.60 / davinci 119.64 / s-log3-sgamut3 119.97 dB**。
  - 五种输入各 `lut bake` 一次全部正常出表（113–133 ms）。
  - `--in-matrix auto` 与显式 `bt2020` 在 Apple Log 素材上 **逐位相同（inf）**；与 `bt709` 是 **44.02 dB**——证明矩阵确实在起作用，不是摆设。
  - `scripts\demo-log-inputs.cmd`：9 种输入各烘一张负片 LUT、各出一张图，并打印每种输入的 GPU/CPU PSNR（118.1–119.5 dB）。
- **后果**：
  - 支持 9 种输入：sRGB、Apple Log、S-Log3（两种 gamut）、C-Log3、V-Log、LogC3、LogC4、BMD Gen5、DaVinci Intermediate。
  - 仍然**手动选输入**，不做自动识别；曲线无法从像素可靠推断。
  - LogC3 只做 EI 800（业界默认）；SUP 2.x、其他 EI、ACEScct 没做。
  - 仍未引入 OCIO：这里是“官方曲线 + 固定 gamut 矩阵”，不是可配置的色彩管理。
  - 容器矩阵认标签，不认“相机品牌 + 输入类型”的默认表；标签缺失时回退 bt2020（与之前一致）。

---

## D062 · OpenColorIO 接入（可选依赖）

- **日期**：2026-11-03
- **背景**：产品负责人选 **A：引入 OpenColorIO**。之前只有“手写曲线 + 固定 gamut 矩阵”，没有 ACES/相机配置，也没有显示变换；D059 时因为“装了不了的环境编不了”押后，这一轮先把**能不能真的编出来**解决掉。
- **实现**：
  - **可选依赖**：`-DSR_WITH_OCIO=ON`（默认）。优先 `find_package(OpenColorIO CONFIG)`（macOS/系统 SDK）；否则找自建前缀（`$OCIO_ROOT`，或这台机器的 `X:/agent/tools/ocio`）。都找不到就编 `ocio_stub.cpp`，API 不变、所有 `--ocio-*` 明确报“这个构建没有 OpenColorIO”。
  - **Windows 怎么编出来的**（`tools\ocio\build-ocio.cmd`）：OCIO **2.4.2** + 两个 MinGW 补丁：
    1. `tools/ocio/mingw.patch`：`SystemMonitor_windows.cpp` 在窄 `tstring` 下的 WCHAR→UTF-8；
    2. 关掉 `OCIO_USE_WINDOWS_UNICODE`（MinGW 的 libstdc++ 没有宽字符 fstream），并给拉下来的 yaml-cpp 0.7.0 补 `#include <cstdint>`（GCC 13+ 不再间接包含）。
    产物是**自包含前缀**：`libOpenColorIO.a` + expat / Imath / minizip-ng / pystring / yaml-cpp / zlib 六个静态库，CMake 直接链接，Windows 上仍然是一个 exe。
  - **隔离**：只有 `src/core/src/ocio.cpp` 包含 OCIO 头；`ocio.hpp` 是纯 C++ 接口（pimpl），其余代码不需要知道 OCIO 是否存在。
  - **契约**：胶片模型永远吃 **线性 Rec.709**。`--ocio-input` 的处理器是 `输入空间 → 配置里的 Linear Rec.709`；`--ocio-linear` 可覆盖（不填时按 `Linear Rec.709 (sRGB)`、`Linear Rec.709`、`Linear sRGB`、`Utility - Linear - Rec.709`、`lin_rec709` 依次找，最后退化成“名字里同时有 linear 和 Rec.709 的唯一一项”）。
  - 负值在 OCIO 解码后**钳 0**，与内置 `decodeInputToLinearSrgb` 的行为一致（胶片模型对负光没有意义）。这也是第一次接 OCIO 时 PSNR 只有 9.84 dB 的原因：不钳制时 (1,1,0) 这种越界色会让密度曲线跑飞，钳制后 63.89 dB。
  - **CLI**：
    - `ocio-info [--ocio-config <file.ocio>] [--ocio-linear <name>]`：版本、配置名、线性空间、全部颜色空间、显示器 + view；
    - `lut bake --ocio-config <file> --ocio-input <space>`：LUT 定义域变成任意 OCIO 颜色空间（**解码烘进表里，`lut apply`/`video` 不需要 OCIO**）；
    - `convert --ocio-input <space>` 与 `--ocio-display <display> --ocio-view <view>`：解码/显示器变换。
  - `--engine spectral-gpu` 的着色器没有 OCIO：给了 `--ocio-input` 就打一次 warning 并回退 **CPU 光谱**，绝不静默出错。`lut apply`/`video` 在 LUT 引擎下用 `--ocio-*` 直接报错，并提示改用 `lut bake --ocio-input`。
  - 分段签名带上 OCIO 输入名，换空间续渲不会混。
  - **UI 暂不链接 OCIO**：UI 用 Qt 的 GCC 13.1，OCIO 静态库是 GCC 16.2 编的，libstdc++ 符号对不上（`std::istream::seekg(std::fpos<...>)`）；UI 也不直接调用 OCIO，所以 `build-ui.ps1` 传 `-DSR_WITH_OCIO=OFF`。UI 的 OCIO 下拉留到下一轮。
- **验证**：
  - `ctest` **10/10**（`ocio` 测试在没有 OCIO 的构建里 exit 77 skip）。
  - OCIO 的 `sRGB Encoded Rec.709 (sRGB)` vs 内置 `srgbDecode`：2.6 MP **118.24 dB**（max 1.5e-5 = 16-bit 1 LSB）。
  - OCIO 的 `Apple Log` vs 内置 Apple Log 解码：**93.60 dB**（两条完全独立的实现互证曲线和色域）。
  - OCIO 的 `ARRI LogC3 (EI800)` 负片 LUT vs 内置 `--input logc3`：**63.89 dB**，max 1.1e-2（差异来自 ACES 配置的 D60→D65 适应，不是曲线错误；18% 灰在两边都是 0.18）。
  - `ocio-info` 在 ACES 2.0 studio 配置上自动选中 `Linear Rec.709 (sRGB)`；`--ocio-display "sRGB - Display" --ocio-view "ACES 2.0 - SDR 100 nits (Rec.709)"` 正常出图。
  - 双击验收：`scripts\demo-ocio.cmd`（四节：配置信息 / Apple Log 双实现对比 / ACES 显示器变换 / ARRI LogC3 LUT 对比）。
- **后果**：
  - 有 OCIO 的构建可以用**任意**相机空间、ACES 空间、显示器变换；没有 OCIO 的构建行为与 D061 完全一致。
  - 仓库内置一份 **ACES 2.0 studio 配置**（Apache-2.0，`data/ocio/`），离线可用；也接受任意 `.ocio` 或 `$OCIO`。
  - Windows CI 不装 OCIO（自建一次 20 分钟，不划算）→ CI 跑的是 stub 路径；macOS CI 用 Homebrew 的 OCIO 真跑测试。
  - 仍未做：UI 的 OCIO 下拉（D063 已完成）、光谱 GPU 的 OCIO、OCIO 输出的 EXR 线性标记（`--exr-linear` 目前只认 sRGB/PQ/HLG）。

---

## D063 · UI 的 OpenColorIO 输入（下拉 / 烘焙 / 原片对比）

- **日期**：2026-11-04
- **背景**：D062 把 OCIO 接进了 CLI，但界面还只有九种内置输入。产品负责人要求“继续”，按 D062 待办第一条做 **UI 的 OCIO 下拉**。
- **实现**：
  - **CLI 增加 `ocio-info --json`**：UI 不解析人类可读的缩进文本；JSON 带 `version / config / configFile / linearSpace / colorSpaces / displays`，配置名里的引号和控制字符会转义。
  - **UI 左栏新增“色彩管理（OCIO）”**：勾选框 + 配置文件（默认仓库里的 ACES 2.0 studio 配置）+ `读取` 按钮 + 色彩空间下拉 + 一行说明（OCIO 版本、空间数量、实际线性空间）。勾选后“画面类型”自动变灰，两者互斥。
  - **参数接线**：烘焙 LUT、光谱预览、渲染视频都改用 `--ocio-config/--ocio-input`，不再传 `--input`（CLI 里两者互斥）。LUT 路径仍然把解码烘进表里，`lut apply` / `video` 不需要 OCIO。
  - **“按住看原片”**：LUT 引擎的 `look` 现在也接受 `--ocio-input`，预览服务端在 `configureLook` 里为 `raw` 单独编一个解码器；否则原片会按 sRGB 直接显示，和 LUT 的定义域对不上。
  - **UI 仍然不链接 OCIO**（Qt 的 GCC 13.1 链接不了 GCC 16.2 编的静态库）。UI 只调 CLI：`findCli()` 优先用 `scripts\build.cmd` 产出的 `build\spektra-roll-cli.exe`（完整构建，带 OCIO），其次才是同目录的 CLI；`SPEKTRA_CLI` 环境变量可覆盖。只有 build-ui 的机器会看到“这个构建没有 OpenColorIO”，其余行为不变。
  - **`--ocio-selftest`**：不点鼠标的验收。它强制一个固定的 look（Portra 400 + Portra Endura、0.70 EV、negative、反差 1.15、DIR、OkLCh、sRGB）和 Apple Log，走和“烘焙”按钮同一条代码路径写出 `out/ui-ocio-selftest.cube`，然后退出；不保存设置。
  - **顺手修掉的预览 bug**：`preview` 的 `spectral-gpu` 在 D062 之后会**静默忽略 OCIO**（GPU 着色器只认内置曲线），与 CPU 光谱相差 **21.34 dB**；现在和 `lut apply` / `video` 一致：打 warning，回退 CPU 光谱。
- **验证**（本机）：
  - UI 自检：**52 个色彩空间**，线性空间 `Linear Rec.709 (sRGB)`，Apple Log LUT 正常写出。
  - **UI 烘的 LUT vs CLI 同参数烘的 LUT：PSNR = inf，max |diff| = 0**（界面没有自己的第二条颜色管线）。
  - OCIO 的 `raw` 解码 vs `convert --ocio-input "Apple Log"`：8-bit/缩放后 **49.58 dB**；不带 OCIO 的 raw 只有 **19.72 dB**，证明解码确实生效。同一个预览进程里把 OCIO 关掉后，`raw` 与基线**逐位相同**（`look` 会清掉旧的处理器）。
  - 光谱预览 + OCIO：`ok look spectral`，1600×898 一帧 **537 ms**，`raw` **95.8 ms**。
  - `spectral-gpu` + OCIO：`ok look spectral`（回退 CPU），一帧 **529.9 ms**，不再是 34 ms 的“假 GPU”。
  - `ctest` **10/10**。
  - 双击验收：`scripts\demo-ocio-ui.cmd`（界面自检 → CLI 对照 → 用 UI 的 LUT 渲一帧 → 打开图片）。
  - CI：run **102** Windows + macOS 双绿；macOS 用 Homebrew 的 OCIO 真跑 `ocio-info --json`（Python 断言 52 个空间）+ LUT，Windows 跑 stub。
- **后果**：
  - 界面可以用任意相机/ACES 空间当 LUT 输入，名字来自配置本身，加空间不用改代码。
  - UI 的 OCIO 能力取决于它调用的 CLI：完整构建（`build\`）有；只有 `build-ui\` 的便携包没有，会明确提示。
  - 仍未做：光谱 GPU 的 OCIO（现在回退 CPU 光谱）、`--exr-linear` 认 OCIO 显示变换。

---

## D064 · 测试素材换成 Sony FX30 S-Log3

- **日期**：2026-11-04
- **背景**：产品负责人提供 `X:\agent\footage\C0113.MP4`（Sony FX30，S-Log3 / S-Gamut3.Cine，3840×2160，50p，10-bit 4:2:2，6.24 s / 312 帧，**没有 color_space / transfer / primaries 标签，但 color_range 标成 full（pc）**；有 PCM 48 kHz 立体声音轨，相机 timecode 在 rtmd 数据流里：**00:13:37:46**）。旧的 iPhone `A001_11011538_C001.mov` 不再当测试素材。仓库从未跟踪过视频文件（`git ls-files` 里没有 `.mov`/`.mp4`），所以没有二进制要删，只有脚本和文档里的路径。
- **实现**：
  - UI `defaultInput()` → `C0113.MP4`；“画面类型”默认改成 `s-log3`。老安装如果保存的是旧默认路径（`A001_11011538_C001`）且输入是 Apple Log，载入时一次性迁到新素材 + S-Log3。
  - 9 个 demo 脚本的 `$Footage` 默认值、ffmpeg 抽帧矩阵（`bt2020` → `bt709`）、`--input/--from apple-log`（→ `s-log3`）、产物名（`m2/m3/m4-applelog*` → `*-slog3*`）全部同步。
  - `video` 调用显式加 `--in-matrix bt709`：这个文件没有标签，CLI 的 `auto` 按 D061 会回退 `bt2020`，对 S-Gamut3.Cine 是错的。
  - ffmpeg 抽帧与 CLI 的 `video`/`preview` 解码从 `in_range=tv` 改成 **`in_range=auto`**：跟随素材的 `color_range` 标签；这个文件是全范围，硬编码 tv 会把 S-Log3 码值压暗约 1%，无标签素材仍然等价于 tv。
  - OCIO 演示与 UI 自检改用 `S-Log3 S-Gamut3.Cine`（配置里存在）；`demo-ocio` 的对照从 Apple Log 改成 S-Log3。
  - `demo-m4` 的分段/续渲演示保留相机 timecode（00:13:37:46）和 PCM 音频，不再写死 `--timecode`。
- **验证**：
  - ffprobe：3840×2160、50 fps、312 帧、yuv422p10le、`color_range=pc`（全范围）、无 color_space / transfer / primaries 标签；PCM 48 kHz 立体声；timecode 00:13:37:46 在 rtmd 数据流里。
  - 抽帧：`in_range=auto` 与 `in_range=pc` 逐位相同，和 `in_range=tv` 不同——所以脚本/CLI 改用 `in_range=auto`，不再硬编码 tv。
  - `demo-ocio`：内置 S-Log3 vs OCIO `S-Log3 S-Gamut3.Cine` **61.44 dB**（曲线一致，差异来自 ACES 的色域/白点转换）。
  - `demo-ocio-ui`：UI 自检 52 个空间；UI 烘的 LUT 与 CLI **inf**；S-Log3 原片正常解码/出图。
  - `demo-cine`：同一帧输出 Vision3 500T + 2383 / Vision3 负片 / Portra 400 三张。
  - `demo-m4`：100 帧 / 2 段，续渲只补缺失段；输出 timecode **00:13:37:46**、音频 PCM。
- **后果**：
  - 4K50 素材的预览会走 960 px 代理；逐帧渲染成本约是 2.6 MP 的 2.4 倍（按像素计）。
  - 无标签的 Sony XAVC 必须 `--in-matrix bt709`；UI 已按输入类型回退 bt709，CLI 的 `auto` 仍按 D061 回退 bt2020。

---

## D065 · 电影片库：Vision3 + 2383/2393 + T/K75P

- **日期**：2026-11-04
- **背景**：产品负责人选 **A（电影片库）**。仓库里 28 个 profile 有 11 个进不了管线：4 支 Vision3、Verita、2 种电影印片（2383/2393）、4 支反转片。对“动态影像胶片模拟器”来说，Vision3 + 2383 是核心片库。
- **实现**：
  - `ProfileInfo` 增加 `stage`；第二层乳剂接受 `support=paper` **或** `support=film + stage=printing`（2383/2393）。
  - 参考光支持 **D55 / T**；观看光支持 **D50 / K75P**。新增 `kIlluminantT`（CIE A / Incandescent）与 `kIlluminantK75P`（Kinoton 75P），用 colour-science 0.4.7 生成、按上游同样的“均值 1”归一。
  - 负片扫描用**胶片自己的** viewing illuminant（D50），印片扫描用**第二层乳剂的**（2383 是 K75P）。K75P → D50 用 CAT02 适应；D50 保持严格单位矩阵，旧片库数值逐位不变。
  - CPU 与两个 GPU 后端（D3D11 / Metal）同步：常量缓冲新增 `printViewingIlluminant[81]`、两套扫描归一化与 3×3 矩阵；顺手补了 16 字节对齐。
  - UI 片库：负片允许 `D55` 或 `T`，第二层允许相纸或电影印片；电影印片在下拉里标注“（电影印片）”。选 Vision3 会自动选中 2383。
- **验证**：
  - `lut bake --film kodak_vision3_500t --paper kodak_2383 --scan print` vs 上游 `tools/parity/reference_model.py`：17³ **88.85 dB**，max |diff| **4.48e-4**；6 个探针逐项一致。
  - `test_film_model` 新增：片库 stage、印片曝光因子 **1.720784698861**、0.0 / 0.184 / 0.5 / 1.0 / 暖红 / 绿地的锚点；250D + 2393 中灰中性。
  - `test_gpu_film` 新增：Vision3 500T/2383 的 print 与 S-Log3 negative，worst **1e-6**；`ctest` **10/10**。
  - `spektra-roll-ui.exe --stocks-selftest`：**films=16 / papers=8**，Vision3 500T、Verita、2383、2393、Portra 400/Endura 全在。
  - 4K50 真实素材（光谱 GPU，Vision3 500T + 2383）：4 帧 ProRes **764.6 ms/帧**（含 4K 上传/回读 + GPU 质感）。
  - CI：run **104** Windows + macOS 双绿；macOS 真跑 Metal 光谱/质感/融合链与 OCIO。
  - 200T + 2393、250D + 2383、Verita + 2383 都能烘表。
- **后果**：
  - 界面可用 profile 从 **17 → 24**（16 支负片 + 6 相纸 + 2 电影印片）；只剩 4 支反转片没接。
  - 仍未做：反转片直扫（Ektachrome/Velvia/Provia/Kodachrome）、印片机 Y/M 微调 / 预闪 / 光源选择、Vision3 的颗粒/光晕物理模型、`--exr-linear` 的 OCIO 显示变换。

---

## D066 · OCIO + 光谱 GPU 的回退提示

- **日期**：2026-11-04
- **背景**：产品负责人在 UI 的渲染队列（偶尔预览一帧）看到状态栏写着 **`warning: ; falling back to the CPU spectral engine`**，看起来像渲染报错。
- **原因**：
  - 选 OCIO 输入 + `--engine spectral-gpu` 时，CLI 先打一条 `warning: OpenColorIO input is CPU-only for now; ...`，把 `gpu` 置 false；紧接着通用回退分支又用**空的 `error`** 打了一条 `warning: %s; falling back ...`。
  - UI 的 `drainRenderOutput` / `drainQueueOutput` 把**每一行 stderr** 都塞进状态栏，所以最后看到的就是那条空 warning。
  - 这不是渲染失败：任务照常跑，只是退回 CPU 光谱（D062 的已知限制），速度会慢很多。
- **修复**：
  - CLI 的 `lut apply` / `video`：OCIO 分支已经解释过回退，不再打第二条；如果 `error` 为空，用明确的文字“GPU spectral engine is unavailable”而不是空的 `%s`。
  - UI：`warning:` 行显示为 **“提示：”**；OCIO 那句翻成“OCIO 输入只能用 CPU 光谱，已自动回退（渲染会变慢）”。日志里仍保留 CLI 原文。
  - 队列/渲染开始时，如果冻结的参数里同时有 `--ocio-input` 和 `--engine spectral-gpu`，先写一条中文提示，不等 CLI 报警。
  - CI：macOS 的 OCIO 步骤加一条 `lut apply --engine spectral-gpu --ocio-*`，断言输出里没有 `warning: ;`。
- **验证**：
  - `lut apply --engine spectral-gpu --ocio-config ... --ocio-input "S-Log3 S-Gamut3.Cine"` 现在只输出一条 OCIO 回退 warning，没有空 warning。
  - `ctest` 10/10；UI 构建与 `--stocks-selftest` 正常。
  - CI：run **107** Windows + macOS 双绿；macOS 的 OCIO 步骤断言日志里没有 `warning: ;`。
- **后果**：
  - OCIO + 光谱 GPU 仍然自动用 CPU 光谱（这条限制没变），但现在提示明确，不会伪装成失败。
  - 想要全速：用 LUT 引擎（界面默认），或改用 `--engine spectral` 明确 CPU，或关掉 OCIO。

---

## D067 · 反转片直扫（Ektachrome / Velvia / Provia / Kodachrome）

- **日期**：2026-11-04
- **背景**：D065 之后片库里只剩 4 支反转片没接（`type=positive`）。反转片显影后本身就是正像，不能印到负片相纸上，也不需要“负片倒相”；正确路径是**直接扫描已显影的染料**。
- **实现**：
  - `ScanMode` 追加 `Positive`；CLI `--scan slide`（也接受 `positive` / `reversal`）。片库里的 `type=positive` 胶片在不写 `--scan` 时自动走直扫。
  - 负片扫描会去掉片基（橙 mask 随之消失）；反转片直扫**保留片基**，因为透明片基是画面的一部分。
  - 扫描后按 `--scan slide` 自己的“清澈片基 → 最深染料”做黑白平衡，并用 18% 中灰按通道 power 中和染料串色；`--contrast` 以中灰为支点（反转片默认 **1.0**）。
  - 正片 + `--scan print` / `--scan negative`，或负片 + `--scan slide`，都在 CLI 里明确报错，不会静默出怪图。
  - CPU 与 D3D11 / Metal 同步：常量缓冲新增 `positiveWhite/Black/Gamma` 与 `positiveGreyLuma`，`scanNegative` 枚举改为三态 `scanMode`；两个着色器都实现“带片基扫描”。
  - UI 片库纳入 4 支反转片（显示“（反转片）”）；选反转片自动切到“扫描反转片（直扫）”并停用相纸下拉，切回负片时恢复。`--stocks-selftest` 增加 `slide=1`。
- **验证**：
  - `test_film_model` 新增：Ektachrome 配置、黑白端点、中灰中性、单调性、0.5 / 1.0 锚点；另外三支反转片都能配置并到白。
  - `test_gpu_film` 新增 Ektachrome + slide + couplers + gamut：CPU vs D3D11 worst **6e-6**；`ctest` **10/10**。
  - 4K50 FX30 单帧：光谱 CPU **2685.6 ms** vs 光谱 GPU **261.3 ms**，两者 **119.09 dB**（ffmpeg PSNR）。
  - `spektra-roll-ui.exe --stocks-selftest`：**films=20 / papers=8**，`slide=1`。
  - `scripts\demo-slide.cmd -NoOpen`：四支反转片都正常烘表 + 出 4K 图（33³ 约 100–110 ms）。
  - `verify-m1` 仍是 **88.07 dB**；M1/负片/印片路径数值未变。
  - CI：run **110** Windows + macOS 双绿；macOS 真跑 Metal 的 slide parity。
- **后果**：
  - 界面可用 profile **24 → 28**（20 底片 + 6 相纸 + 2 电影印片）；所有随仓库提供的 stock 都已接通。
  - 反转片直扫的黑白平衡是**扫描仪式**的工程处理，和上游默认的“原始密度扫描”不同；上游没有正片直扫的 parity 目标。
  - 仍未做：印片机 Y/M 微调 / 预闪 / 光源选择、扫描仪 unsharp、空间物理模型、光谱 GPU 的 OCIO。

---

## D068 · RED R3D / R3D NE：自带 SDK 的本地骨架

- **日期**：2026-11-04
- **背景**：产品负责人问能否支持 Nikon ZR 的 **R3D NE**（REDCODE RAW 的 Nikon 变体），并选择 **A：先做 BYO SDK 骨架**。R3D 是私有 RAW，`ffmpeg` 主线不能解码；RED SDK 是闭源、EULA 要求不能分发头文件/静态库，和本项目的 GPL-3.0-or-later 官方发布方式冲突。
- **实现**：
  - 新增 `sr/red.hpp` 纯接口 + `red_stub.cpp`（默认）+ `red_sdk.cpp`（仅 `-DSR_WITH_REDSDK=ON`）。
  - CMake：`SR_WITH_REDSDK` 默认 OFF；ON 时要求 `SR_REDSDK_ROOT` 指向用户自己的 SDK，找不到 `R3DSDK.h` / `R3DSDK` 库就明确报错并指向 `docs/red-sdk.md`。
  - CLI 新增 `red-info [--in <clip.r3d>] [--json]`：报告是否编译、SDK 版本、运行时库目录；有 SDK 时读尺寸/帧数/帧率/音频/timecode。`video` / `preview open` 遇到 `.r3d` 给出“BYO SDK / REDCINE-X ProRes 4444”路径，不再掉进 ffprobe。
  - UI 识别 `.r3d` 扩展名：调 `red-info --json`；没有 SDK 就显示指引，有 SDK 就显示元数据并说明“解码待接入（D068）”。文件选择/拖放接受 `.r3d`，单条预览/渲染会在 `requireInput` 拦下并解释。
  - 新增 `scripts/build-red.ps1` / `.cmd`（`-Check` 只检查文件，不启动 SDK）和 `docs/red-sdk.md`（BYO SDK 步骤、运行时 `SPEKTRA_RED_SDK`、许可证边界）。
  - 新增 `test_red`：默认构建断言 `.r3d` 识别 + stub 行为；`ctest` **11/11**。CI 增加一步：官方构建必须 `built: false`，且 `video --in sample.r3d` 返回 RED 指引。
- **边界**：
  - **不随仓库/安装包分发 SDK**，官方 CI 和 `build\` 永远是 stub。
  - 带 SDK 的 `build-red\` 只限本地自用；再分发要自己同时满足 GPL 与 RED EULA。
  - D068 **不解码帧**：只读元数据。真正渲染 R3D 需要下一阶段（帧源抽象、音频、timecode、预览），而且只能在有 SDK 的机器上测。
  - 现行的 R3D 渲染路线：REDCINE-X 导出 ProRes 4444（RWG/Log3G10）→ `lut bake --ocio-input "Log3G10 REDWideGamutRGB"` → `video --lut`。
- **验证**：
  - 默认构建 `red-info`：`built=false`，错误信息清楚；`video --in sample.r3d` 返回 RED 指引。
  - `test_red`：`.r3d` / `.R3D` / `.R3d` 识别，`.mov` / 无扩展名 / `.r3d.tmp` 拒绝；stub probe 报明确错误。
  - `CMake -DSR_WITH_REDSDK=ON` 无 SDK 时在配置阶段报错，不会生成半成品。
  - `red_sdk.cpp` 用最小假头文件做过 `-Wall -Wextra -Wpedantic` 语法/接口检查；并用假 `libR3DSDK.a` 完整走通 `-DSR_WITH_REDSDK=ON` 的配置、编译和 `red-info --in`（fake 4096×2160 / 120 帧），真实 SDK 仍需用户机器验证。
  - `ctest` **11/11**；`verify-m1` 88.07 dB 不变；UI `stocks selftest` 正常。
  - CI：run **113** Windows + macOS 双绿；官方构建的 `red-info` 断言 `built=false`，`.r3d` 视频输入断言返回 RED 指引。
- **后果**：
  - 官方包仍无 RED 支持；有 SDK 的用户可以本地编译并先用 `red-info` 验证文件和 SDK。
  - 下一步若继续：把 `red_sdk.cpp` 接上 `Clip::DecodeVideoFrame` / 新 SDK 解码，抽象 `video` 的帧源（ffmpeg / RED），再处理音频、timecode、预览代理；约 1.5–2.5 D，CI 只能测 stub。

---

## D069 · RED R3D / R3D NE：本地 MSVC 插件 + 帧解码

- **日期**：2026-11-04
- **背景**：用户提供了 Nikon ZR 的 R3D NE 样片（`ZR_RED_Color_Science_Sunflower.R3D`，6048×3402 / 249 帧 / 23.976）和 RED R3D SDK 9.2.1。D068 的静态链接骨架在 Windows 上不可用：SDK 只提供 MSVC 静态库（`R3DSDK-2017MD.lib`），主程序是 MinGW，C++ ABI 不兼容；直接把闭源 SDK 链接进 GPL 二进制也不合适。
- **架构**：
  - 核心 CLI（GPL、MinGW）只编译 `red_plugin.cpp` loader；运行时 `LoadLibrary`/`dlopen` 找 `spektra-red.dll`（或 `SPEKTRA_RED_PLUGIN`）。
  - `src/red-plugin/red_plugin_api.h` 是纯 C ABI；`src/red-plugin/red_plugin_msvc.cpp` 是唯一包含 RED 头文件的文件，由 MSVC 编译成插件。
  - 插件返回 16-bit interleaved RGB；默认 `Primary_Development_Only` → **REDWideGamutRGB + Log3G10**，也可 `Full_Graded` → Linear Rec.709 / sRGB。
  - 解码后端是 **CPU**（`OPTION_RED_NONE`）；GPU 解码未接。
  - `scripts/build-red.ps1` 用 MSVC 编插件，复制普通 CLI 和 SDK Redistributable DLL 到 `build-red\`（git 忽略）；官方 `build\`/CI 不受影响。
  - CLI 新增 `red-decode`：单帧 / `--out-dir` 序列，`--format log3g10|linear|srgb`，`--scale full|half|quarter`，PNG/EXR。
  - `scripts/red-to-prores.ps1`：`red-decode` → `lut apply --in-dir` → ffmpeg ProRes 的短片桥；无音频。
  - `red-info` 现在能报告插件路径、SDK 版本和 R3D 元数据；`.r3d` 的 `video`/`preview` 仍被拦截并指向 `red-decode`。
- **验证**：
  - 本机加载 SDK 9.2.1：`red-info --in` 返回 6048×3402 / 249 帧 / 23.976 / TC 11:09:14:03 / 音频 0 通道。
  - `red-decode --scale half --format srgb` 出 3024×1701 图，颜色正常；`log3g10` 经 OCIO 显示变换正常。
  - `lut bake --ocio-input "Log3G10 REDWideGamutRGB"` + `lut apply` 出 Vision3 250D + 2383 成片；`demo-red.cmd` 正常。
  - `red-to-prores` 出 3 帧 ProRes 422 HQ（3024×1701，~23.976 fps，无音频）。
  - 性能：half 约 **3.8 s/帧**（含 SDK 初始化 + 30 MB PNG），full 6K 约 **14 s/帧**（含 100 MB PNG）。
  - `ctest` **11/11**；官方构建（无插件）`red-info` 仍是 `built=false`，CI 断言改为“plugin not found”。
  - CI：run **116** Windows + macOS 双绿（真实解码只在有 SDK 的本机验证）。
  - 用假 `R3DSDK.h` 做过 `-Wall -Wextra -Wpedantic` 检查；真实 SDK 用 MSVC 完整编译并实际解码通过。
- **后果**：
  - R3D NE 从“只读元数据”升级为“可解码帧”；用户可以用 `red-decode` + `lut apply` 完成整段序列的胶片模拟。
  - 还没做：`video --in clip.R3D` 直读、音频/timecode 封装、UI 预览/渲染直连、GPU 解码、并行解码。
  - 官方包仍然不带 RED 支持；`build-red\` 只限本地自用，不能分发 SDK/插件。

---

## D070 · R3D NE 直读视频（`video --in clip.R3D`）

- **日期**：2026-11-04
- **背景**：D069 已经能 `red-decode` 单帧/序列 + `lut apply`，但普通用户还是希望一条 `video` 命令直接吃 `.R3D`。
- **实现**：
  - `VideoRender` 增加 `red` / `redScale` / `redFormat`；`renderRange` 在 R3D 模式下用 `sr::RedReader` 取帧，不再开 ffmpeg 解码管道。
  - LUT 引擎：R3D 解成 **Log3G10 / REDWideGamutRGB**，直接套用 `--ocio-input "Log3G10 REDWideGamutRGB"` 烘的 LUT。
  - 光谱引擎：R3D 解成 **sRGB Rec.709**，作为 `InputTransfer::Srgb` 进入 `FilmModel`；禁止同时传 `--input` / `--ocio-*`（否则语义冲突）。
  - 元数据：帧数/帧率/timecode 来自插件；`--start` 会按 `addFrames` 偏移输出 timecode；`--frames` 超出剩余帧会截断并 warning。
  - 默认编码为 **ProRes 422 HQ**（half 的 3024×1701 高度是奇数，H.264/HEVC 4:2:0 不接受；`--codec prores` 可处理）。
  - `--red-scale full|half|quarter` 控制解码尺寸；6K full 很慢，half/quarter 适合预览。
  - R3D 分支暂不支持 `--segment/--resume`（明确报错）；输出为**视频-only**，不封装 R3D 音频。
  - UI 仍不直连 `.r3d`，但提示改成“CLI 已可直接 video --in clip.R3D”。
  - `scripts/demo-red.cmd` 增加 5/5：直接渲染 3 帧 `out\red-clip.mov`。
- **验证**：
  - `video --in sample.R3D --lut ... --frames 3 --red-scale half`：3024×1701 ProRes 422 HQ，**2.0 s/帧**（含 CPU R3D 解码 + LUT + 编码）。
  - `--engine spectral-gpu`（Vision3 250D + 2383，half）：**2.29 s/帧**，输出正常。
  - `--red-scale quarter`：1512×850，**0.69 s/帧**。
  - `--start 1 --frames 1`：timecode `11:09:14:03 → 11:09:15:03`，帧位置正确。
  - `demo-red.cmd` 正常出 4 张 PNG + 1 段 3 帧 ProRes。
  - `ctest` 11/11；官方构建（无插件）仍走“plugin not found”错误路径。
  - CI：run **119** Windows + macOS 双绿（run 118 只是 CI 断言字符串没跟上 D070 的错误文案）。
- **后果**：
  - R3D NE 现在是“`video --in clip.R3D` 一条命令出片”；LUT 和光谱 GPU 都能用。
  - 还没做：R3D 音频封装、`--segment/--resume`、UI 预览/渲染直连、GPU 解码/并行解码。
  - 官方包仍然不带 RED，`build-red\` 只限本地自用。

---

## D071 · R3D 收尾：音频 + UI 直连 + segment

- **日期**：2026-11-04
- **背景**：D070 已经能 `video --in clip.R3D`，但还缺 R3D 音频、界面预览/渲染直连、`--segment/--resume/--jobs`。产品负责人选 **A**。
- **音频**：
  - 插件 C ABI 增加 `sr_red_has_audio` / `sr_red_write_audio`；`red_plugin_msvc.cpp` 按 SDK 的 `AudioBlockCountAndSize` + `DecodeAudioBlock` 写 **32-bit PCM Sun AU**（和 SDK 的 `ExtractAudioSample.cpp` 一致）。
  - `RedReader::hasAudio/writeAudio`；`video` 在渲染前把 R3D 音频写成临时 `.au`，最后用 ffmpeg 封装：ProRes 用 **PCM 24-bit**，H.264/HEVC 用 **AAC 192k**；`--start/--frames` 会同步裁剪音频。
  - 样片本身 **0 音频通道**，所以本机只能验证“无音频路径”和“合成 AU 的封装”；真实 R3D 音频仍需一条有声样片验证。
- **segment / resume / jobs**：
  - 移除 R3D 的 `--segment` 限制。`renderRange` 本来就为每个 part 新建 `RedReader`，所以每个分段独立打开 clip；`finishRedVideo` 用 concat 列表 + 可选 AU 一次性封装。
  - `--resume` 的签名包含 `red/scale/format`，不会混用旧分段；`--jobs` 的 worker 各自持有 `RedReader`。
- **UI 直连**：
  - 左栏新增“R3D 解码：full / half / quarter”；`probeInput` 对 `.r3d` 用 ffprobe 读元数据 + `red-info` 判断插件可用性，时间线/预览时间可用。
  - LUT 引擎：`bakeLut` 对 R3D **强制** `--ocio-config data/ocio/... --ocio-input "Log3G10 REDWideGamutRGB"`；`lookSignature` 加入 `r3d:<scale>`，换 R3D/scale 会重烘。
  - 光谱引擎：`lookArgs` 对 R3D 不再传 `--input/--ocio-*`（插件解 sRGB）。
  - 预览：`previewFromServer` 对 R3D 返回 false，`previewRedLegacy` 用 `red-decode` 出当前帧（LUT 用 log3g10、光谱用 srgb），再 `lut apply` / 光谱链；拖动时只在松手后渲染。
  - 渲染：`buildVideoArgs` 加 `--red-scale`；选 R3D 自动把编码切到 ProRes（half 3024×1701 高度奇数）；单条渲染/入队可用。批量拖入 R3D 仍明确提示先用单条。
  - 新增 `--red-selftest`（探测 + 烘 LUT + 预览）和 `--red-render-selftest`（真实 Render 按钮路径，2 帧）。
- **验证**：
  - CLI `--segment 0.1 --frames 4 --red-scale half`：2 段 × 2 帧，`--jobs` 2 个 worker 正常；删掉第 2 段后续渲只补缺失段。
  - UI `--red-selftest`：`ok: 预览：3024 × 1701 @ 1.92 s · 7993 ms`。
  - UI `--red-render-selftest`：`ok: 渲染完成`，输出 3024×1701 / 2 帧 ProRes 422 HQ。
  - 合成 AU + ffmpeg：ProRes 视频 + PCM 24-bit 音轨封装成功；真实 R3D 音频未测（样片 0 通道）。
  - `ctest` 11/11；`verify-m1` 88.07 dB。
  - CI：run **121** Windows + macOS 双绿（真实音频仍只在有样片时验证）。
- **后果**：
  - R3D 现在是“CLI 一条命令 + UI 单条预览/渲染”都可用；分段/续渲/并行也支持。
  - 还没做：真实 R3D 音频验证、UI 批量 R3D、GPU/并行解码、6K 全分辨率性能。
  - 官方包仍然不带 RED，`build-red\` 只限本地自用。

---

## D072 · macOS UI 用 SwiftUI + 引擎契约（Phase 0）

- **日期**：2026-11-04
- **背景**：产品负责人对 Windows Qt UI 的观感不满意；项目 M0 的原始策略是 **共享 C++ 引擎 + macOS SwiftUI / Windows 原生 UI**，D009 只拍了 Windows 用 Qt，并没有否决 macOS SwiftUI。现在决定先把 macOS UI 做出来，再以它为设计基准重做 Windows UI。
- **三个选择**：
  - 架构：**A · SwiftUI 壳 + 现有 CLI**（不让 SwiftUI 直接链接 C++ 核心）。
  - 分发：**B · 先做未签名 `.app` 本地验收**，公证/Developer ID 后补。
  - v1 范围：**A · 与 Qt 功能对齐**（不是精简 MVP）。
- **决定**：
  - macOS UI 用 **SwiftUI**；所有颜色/胶片/渲染逻辑仍在 `spektra-roll-cli`，UI 不复制引擎。
  - **macOS 是参考实现，`design/` 是设计源**；不是“macOS 代码是永久标准”。
  - macOS v1 + 设计冻结前，**Windows Qt UI 只修 bug、不加新功能**；之后进入 **M6：Windows UI v3**，按同一份设计规格重做。
  - **两套 UI 的维护成本正式接受**；用 `data/ui-schema.json`、`docs/ui-parity.md`、golden argv、CI 双 UI 编译来防漂移。
  - macOS R3D 插件**不在本期范围**；UI 明确显示“R3D 仅 Windows”，不阻塞 UI 交付。
  - 分发先本地未签名；公证需要 Apple Developer Program，等 UI 稳定后再做。
- **Phase 0 交付（本轮）**：
  - CLI 新增 `capabilities --json`：版本、平台、OCIO/RED/GPU 能力、输入/编码/预览协议，UI 用它决定控件可用性。
  - CLI 新增 `catalog --json`：直接从 `data/profiles` 输出 UI 应显示的 20 底片 / 8 相纸印片，含 `targetPrint`、`positive`、`printFilm`；SwiftUI 不再重写过滤规则。
  - CLI `video --progress json`：输出 `start` / `progress` / `done` 单行 JSON 事件；并行分段有互斥锁，行不会写坏。
  - `preview` 协议升到 2，新增 `hello` 握手；`frame`/`raw` 仍是 8-bit RGBA 文件传输。
  - 新增 `data/ui-schema.json`（42 个参数：标签、范围、默认值、CLI flag、显示条件）、`design/tokens.json`（v0 基线）、`docs/engine-api.md`、`docs/ui-parity.md`。
  - 顺手修掉 CLI `help` 里“R3D 无音频、不支持 segment”的过期说明。
- **验证**：
  - `capabilities --json` / `catalog --json` 都是合法 JSON；`catalog` 为 **20 films / 8 papers**，约 213 ms。
  - `preview` 的 `hello` 返回 `protocol: 2`。
  - `video --progress json` 48 帧输出 `start` + 2 条 `progress` + `done`；分段 `--jobs 2` 下 JSON 行完整。
  - `scripts\verify-d072.cmd` **5/5 OK**（capabilities / catalog / preview hello / ctest / JSON 进度）。
  - `ctest` **11/11**（含 GPU / OCIO / RED stub）。
- **后果**：
  - Phase 1 需要一台装了 Xcode 的 Mac；Windows 本机只能做 CLI/文档部分。
  - Windows 用户会在一段时间内继续用旧 Qt UI；如果不可接受，M5.5 后可先做 1–2 个 D 的 Qt 换肤。
  - 设计不冻结就不开始 M6，否则 Windows 复刻会追移动目标。
  - 两套 UI 不是“一次成本”，是长期税；`ui-parity.md` 和 CI 是必须的，不是可选项。

---

## D073 · macOS UI 初版：SwiftUI 三栏 + 品牌暗色

- **日期**：2026-11-05
- **背景**：Xcode 16.2 + command line tools 已安装；产品负责人给出 macOS UI 的初版设计：暗色/近黑、白色药丸按钮黑字、思源黑体、标题加粗；三栏（左「底片」、中播放、右检查器）；参考 SpektraLab 的工作流但不复制其源码/图标/截图。
- **决定**：
  - **品牌一致 + 平台自适应**：颜色/间距/组件取自 `design/tokens.json`；窗口、菜单、文件选择器用 macOS 原生。
  - 三栏宽度：左 **220**、右 **380**、中间自适应；窗口最小 1280×800。
  - 字体：**思源黑体**；没装时回退 Noto Sans SC / Noto Sans CJK SC / PingFang SC / 系统。标题一律 bold。
  - 主按钮：**白色胶囊 + 黑色文字**；次按钮：深色胶囊 + 白字 + 1px 边框。
  - 左侧「底片」：双击空白导入视频/项目目录；单列缩略图；双击缩略图载入播放器。导入时弹窗选择 **画面类型** 或 **OCIO 色彩空间**；OCIO 配置由后台自动选择，不让用户填路径。
  - 中间：AVPlayer 播放原片；**空格播放/暂停**；暂停时可开「胶片效果」，用 CLI 渲染当前帧。
  - 右侧 tab：**INFO / FILM / PRINT / ADVANCED / OUTPUT**。
    - FILM：画幅（35mm / Super 35 / 16mm / Super 16 / 65mm / IMAX）、胶片按彩色负片/电影负片/反转片分类、颗粒/光晕/防光晕层/耀光/扩散。
    - PRINT：相纸/电影印片分类、扫描方式、反差、OkLCh。
    - ADVANCED：曝光、DIR、强度、引擎、GPU 质感、LUT 插值、线程、OCIO 状态。
    - OUTPUT：编码、SDR/PQ/HLG、CRF/preset、输出目录/文件名、渲染、进度、日志、取消。
  - **SwiftUI 仍然只是壳**：所有颜色/胶片/渲染在 `spektra-roll-cli`，UI 只生成 argv、读 JSON、显示状态。
  - 字体用 OFL 的 Noto Sans CJK SC（思源黑体的 Google 发行版）按需下载，不提交二进制；构建脚本负责打包。
  - macOS v1 **不支持 R3D**（无插件）；导入时明确提示。
  - Windows Qt 继续冻结；等 macOS v1 设计冻结后按同一份 token 重做 Windows UI v3。
- **交付**：
  - `ui-macos/`：Swift Package（`Package.swift` + SwiftUI 源码），不依赖 Xcode 工程文件；`swift build` 可在 Xcode/CLT 下编译。
  - `scripts/build-macos-ui.sh`：编 CLI → 编 SwiftUI → 组装 `SpektraRoll.app` → 拷 data/ffmpeg/fonts → ad-hoc 签名 → 打开。
  - `scripts/fetch-macos-fonts.sh`：下载 Noto Sans CJK SC Regular/Bold（OFL）。
  - `docs/ui-design-macos.md`：布局、交互、参数映射、v1 范围、验收。
  - `design/tokens.json` 升到 v2：暗色品牌、白色药丸、思源黑体、三栏尺寸。
  - CI：macOS job 增加 `swift build --package-path ui-macos -c release`，先保证编译不退化。
- **验证**：
  - Windows 本机不能编译 SwiftUI；用 Windows 上的 Swift 6.4 做了 `-parse`，Foundation 引擎层 `-typecheck` 通过。
  - macOS CI run **#128**（commit `0f7b593`）`swift build` 通过；此前 #125/#126 失败于 `AppState` 缺 `inspectorTab`，已修；#127 起 CI 会把 Swift 错误打成 annotation。
  - **Mac 本机验收（2026-11-05）**：`swift build` 6 秒通过；`scripts/build-macos-ui.sh` 组出约 167 MB 的 `.app`；启动、三栏、字体、缩略图、AVPlayer 播放、时长、INFO、胶片效果、UI 渲染全部正常；真实 `C0113.MP4`（4K50 / 312 帧）缩略图/播放/INFO/4K 胶片预览正常；合成 3 秒测试片渲染输出 640×360 / 3.00 s / H.264 / yuv420p。
  - 远程验收坑：SSH 在 `Background` launchd domain，`screencapture` 抓不到 GUI 窗口；用 app 内 `SPEKTRA_CAPTURE=1` 自截图。Stage Manager 会隐藏窗口，验收时临时关闭、结束后恢复。见 `docs/macos-dev-notes.md`。
  - OCIO 暂不可用：Homebrew `opencolorio` 在 macOS 14 上无 bottle，源码构建 `python@3.15` 被 clang tail-call 错误挡住；cmake 已从手动 4.4.4 切到 Homebrew 4.4.5（手动目录备份在 `/opt/homebrew/opt/cmake-manual-4.4.4`）。首版用内置 10 种 Log 输入。
- **后果**：
  - 第一次构建需要 Xcode 16.2、`brew install cmake ffmpeg`，以及可选的字体下载。
  - `.app` 目前是 ad-hoc 签名；分发给别人前要做 Developer ID + 公证。
  - OCIO 在 macOS 仍可能链接 Homebrew 动态库；正式分发前要静态链接或捆绑 dylib。
  - 播放器 v1 是原片播放 + 暂停时单帧胶片预览；时间线逐帧预览留给下一轮。

---

## D074 · macOS UI v1 验收反馈（v1.1 范围）

- **日期**：2026-11-05
- **背景**：产品负责人在 Mac 上验收 D073，总体“效果比预期好”，提出 6 点改进；当时已晚，决定先记录、下一轮实现。
- **6 点反馈**：
  1. 视频下方加**直方图 + 宽容度**等数据参考（在胶片效果按钮那一排的上方）。
  2. 视频画面**缩放/平移**，可仔细查看。
  3. **播放时实时胶片预览**：按下播放时做预渲染缓存，然后播放胶片效果。
  4. 胶片效果**关闭**时显示“输入色彩空间 → 标准显示 sRGB”的画面，而不是 Log 灰片。
  5. 胶片效果开启时，**任何参数变化立即刷新画面**，不需要重新开关。
  6. 在胶片效果按钮**左侧加“导出”按钮**，点击跳到 OUTPUT。
- **决定**：
  - 这 6 点作为 **macOS UI v1.1** 的优先范围；OCIO 单独一轮，不混进来。
  - 实时预览方向：**播放时预渲染低分辨率/代理片段并缓存**，按参数 hash 失效；UI 不复制颜色逻辑。
  - “关闭胶片效果”也必须走同一套代理管线生成 display 版本，不能直接显示 AVPlayer 原始 Log。
  - 参数变化：防抖 + 缓存失效 + 当前帧自动重渲染。
  - Windows Qt 继续冻结，等 macOS v1.1 设计稳定。
- **状态**：**未开工**；待办/实现提示见 `progress.md` 的 D074 一节。
- **后果**：实时预览缓存是最大工作量，可能需要 CLI 增加“预览片段”能力；直方图/宽容度、缩放、导出按钮可以先做。

---

## D081 · 光谱 GPU 接 OCIO + 线性 EXR 跳过显示变换

- **日期**：2026-11-05
- **背景**：D080 在 macOS 上接好了静态 OCIO 2.6.0，但两处仍是欠账：`--engine spectral-gpu` + `--ocio-input` 会回退 CPU 光谱（D062 的限制）；`--exr-linear` 遇到 `--ocio-display/--ocio-view` 时仍用 sRGB/PQ/HLG 反推，无法处理任意显示变换。
- **决定**：
  - 不把 OCIO 塞进 Metal / D3D11 着色器，也不要求 GPU 链接 OCIO。GPU 后端在**上传前用 OCIO 的 CPU 处理器批量解码**成线性 Rec.709，再交给现有光谱着色器；两个后端共用同一实现。
  - 着色器新增 `inputMode = 10`（已是线性 Rec.709）。为了保持 `--strength < 1` 的语义与 CPU 参考一致，OCIO 路径额外保留原始编码帧的 GPU 缓冲，混合时用原始值而不是线性值。
  - `convert --exr-linear` 遇到 OCIO 显示变换时**跳过显示变换，直接写显示前的线性 Rec.709**；任意 view 的逆变换没有定义，反推只会产生错误的“线性”。
- **验证**：
  - `test_gpu_film` 新增 `ocio logc3 / negative +gamut`，与 `FilmModel::transformFrom` worst **1e-6**；Windows 与 Mac 本机 `ctest` 均 **11/11**。
  - `lut apply` 的 OCIO 路径 CPU vs `spectral-gpu`：Windows D3D11 **109.43 dB**，Mac Metal **113.70 dB**；`--strength 0.5` 读回 **inf**。
  - `convert --ocio-display ... --exr-linear` 与不带 OCIO 的线性 EXR 读回 **inf**（Windows + Mac）。
  - `video --engine spectral-gpu --ocio-input "S-Log3 S-Gamut3.Cine"` 正常出片，不再有回退 warning。
  - CI macOS OCIO 步骤改为断言 `spectral-gpu` 日志里**没有** `warning:`，并新增 OCIO 线性 EXR 冒烟。
  - CI run **153** Windows + macOS 双绿（macOS 真跑 Metal `gpu_film` 和 OCIO 步骤）。
- **后果**：
  - OCIO 的逐帧 CPU 解码仍有成本（4K 每帧数十毫秒级），但远低于回退 CPU 光谱；后续如需再快，可把 OCIO 变换烘成 3D LUT 上 GPU。
  - Metal 与 D3D11 行为一致；没有 OCIO 的构建完全不变。

---

## 待决策（还没问你）

- D010：是否做 OFX 插件（建议做，第二期）
- D011：Apple Developer 账号什么时候买
