# STATUS

**最后更新**：2026-11-03 · 里程碑 **M5 进行中（D009 = Qt 6）** · 项目名 **SpektraRoll**

## 一句话

`spektra-roll-cli video` 现在能直接吃 iPhone 的 **Apple Log** 视频：逐帧套 Portra 400 光谱 LUT，叠加颗粒、光晕、眩光、扩散，带原声输出 SDR 或 **HDR（PQ/HLG）**，也能直接出 ProRes 422 HQ。颜色和质感**合并成一次 GPU 提交**，管道也修掉了每帧 130 ms 的量化开销、并让解码/编码与处理重叠：2.6 MP 全质感视频 **79–82 ms/帧**（D046 时 379，累计约 4.7×），与旧路径逐位相同。三个空间效果自 D054 起是**按底片微米计的真实点扩散核**（窄芯 + 宽裙），1080p / 4K 观感一致。界面预览是**常驻进程 + 整段帧缓存**：短片拖动时间线 **38–64 ms/帧**（以前 1.7–2.2 s）；长片自动生成 960 px 代理，**100 ms/帧**。长片可以分段渲染，中断后只补缺失的段，输出保留相机 timecode。交付可以是 16-bit PNG，也可以是**不钳制的 float / half EXR**（D055，读回逐位一致）。macOS 的颜色已经走 **Metal**（D056；质感还在 CPU，D057 补）。Qt 6 桌面界面负责选素材、烘 LUT、调质感、预览和渲染。

仓库：https://github.com/haichangjyue/SpektraRoll
本地：`X:\agent\SpektraRoll`

## M3 已完成：视频读写 + 胶片质感

- [x] `spektra-roll-cli video --lut x.cube --in clip.mov --out graded.mov [--trc pq]`
- [x] 逐帧解码 → 套 LUT → 重新编码，原声保留，HDR 自动带 PQ/HLG 标记
- [x] 自动处理竖拍素材的旋转（-90 元数据）
- [x] 帧区间：`--start 00:00:12 --frames 240`（音画同步）
- [x] ProRes 422 HQ 输出：`--codec prores`（SDR 和 HDR 都有）
- [x] macOS 本机验收脚本：`scripts/verify-macos.command`（**已在 Mac 上跑通，含视频路径**；期间发现并修复 POSIX `popen` 模式 bug）
- [x] 多线程（D032）：套 LUT 96→17 ms
- [x] **胶片质感**（D033/D035）：颗粒 + 光晕 + 眩光 + 扩散，逐帧叠加；`scripts\demo-m3.cmd`
- [x] 颗粒调轻（D034）：噪声 std 0.014→0.0053，不再"毛躁"（**已验收**）
- [x] **颗粒时间连续**（D036）：两场交叉淡化（`--grain-rate 3`），帧间差 −44%，视频不再闪烁
- [x] **模糊降采样**（D037）：眩光降 4 倍、光晕/扩散降 2 倍，579→469 ms/帧
- [x] **硬件编码选项**（D038）：`--encoder qsv`，2.6 MP 下收益有限（瓶颈在管道）
- [x] CI 现在 Windows + macOS 都跑视频冒烟
- [x] **颗粒场缓存**（D039）：一个噪声场跨帧复用，全质感 **476.5 → 358.7 ms/帧**
- [x] **分段渲染**（D040）：`--segment 10` 写成 `<out>.parts/`
- [x] **断点续渲**（D040）：`--resume` 只补缺失段；删掉中间段重跑验证过
- [x] **时间码**（D040）：复制源 timecode，`--start` 自动偏移；`--timecode` / `--no-timecode` 可覆盖
- [x] CI 两平台跑分段 + 时间码冒烟（Windows + macOS 绿）
- [x] 音频只在最后从源文件复制一次，分段边界没有 AAC 接缝
- [x] `scripts\demo-m4.cmd`：渲染 → 模拟崩溃 → 续渲
- [ ] 多段并行渲染

## M5 进行中：Qt 6 桌面界面（v2）

- [x] **D009 拍板 Qt 6**（Qt Widgets），不再走 WinUI 3 / MSVC
- [x] Qt 6.8.3 + MinGW 13.1 安装在 `X:\agent\tools\qt`（不进仓库）
- [x] `spektra-roll-ui`：素材、LUT、曝光、负片/印片、反差、SDR/HDR、颗粒/光晕/眩光/扩散
- [x] **预览一帧**：ffmpeg 抽帧 → 套 LUT + 质感 → 窗口显示（2160×1214 实测）
- [x] **时间线拖动**预览，素材信息条显示分辨率/帧率/时长/帧数/timecode
- [x] **按住看原片**对比、拖放素材、快捷键 Ctrl+P/R/B、输出文件夹
- [x] **烘焙 LUT** 按钮（Apple Log / SDR，HDR 自动带 PQ/HLG）
- [x] **渲染整段**：编码、起始、帧数、分段、续渲；日志、进度条、取消
- [x] **渲染队列**：参数冻结入队、自动避重名、顺序渲染、停止/移除/清空；LUT 快照保证已入队任务不被后续改参影响
- [x] 队列**持久化**（`out/ui-queue/queue.json`），重启后恢复；每行有进度条和用时/剩余时间；支持多文件拖入批量入队
- [x] **界面 v2**（D041）：三栏 + 顶栏、SpektraLab 灰/琥珀令牌、可折叠 Section、参数滑块；分析见 `docs/ui-analysis-spektralab.md`
- [x] 队列页自动加宽，参数页保持窄栏；截图更新
- [x] **直方图叠加** + **适合/100% 缩放**（滚轮缩放、拖拽平移、Ctrl+0/1、双击切换）
- [x] **冲印流程分组**（D042）：右栏“显影前 / 显影后 / 队列 / 日志”；DIR 耦合剂与 OkLCh 色域压缩可在界面开关
- [x] **胶片 / 相纸选择**（D043）：11 支彩色负片 × 6 种相纸；选胶片自动切到推荐相纸
- [x] **可选物理光谱引擎**（D044）：`--engine spectral` / UI“引擎：光谱”，2.6 MP 约 1.1 s/帧；默认仍是 LUT
- [x] **GPU 探针**（D045）：Arc A750 D3D11，2.6 MP × 81 样本 **0.63 ms**
- [x] **GPU 光谱引擎**（D046）：`--engine spectral-gpu` / UI“光谱 GPU”，2.6 MP 单帧 **50 ms**（CPU 1.1 s），视频 1427 → 379 ms/帧；与 CPU 参考 PSNR 82–119 dB，`test_gpu_film` 六组配置最差 3e-4；无 D3D11 时自动回退 CPU 光谱
- [x] **GPU 质感**（D047）：`--texture-engine auto/cpu/gpu` / UI“GPU 质感”勾选框，颗粒 + 光晕 + 眩光 + 扩散全上 D3D11；视频 **382.7 → 245.6 ms/帧**，与 CPU 参考 143–166 dB，`test_gpu_texture` 九组配置最差 4.5e-7
- [x] **预览实时化**（D048）：`preview` 常驻进程 + 整段帧缓存；LUT 预览 **1.7–2.2 s → 47–64 ms/帧**，光谱 GPU 43 ms；拖动时间线 90 ms 合并；`--preview-selftest` 可脚本验收
- [x] **融合 GPU 提交**（D049）：颜色 + 质感共用一个设备，一次上传/回读；视频 **233.6 → 207.1 ms/帧**，与分段路径逐位相同，`test_gpu_chain` 与 CPU 参考 124 dB；预览 43 → 38 ms
- [x] **视频管道修复**（D050）：RGB48 量化去掉 `std::lround`（单帧 130 ms）+ 两个整帧循环并行；视频 **207.1 → 85–92 ms/帧**，输出逐位相同；`SPEKTRA_VIDEO_DEBUG=1` 可看分段耗时；硬件解码实测更慢，不采用
- [x] **读写重叠**（D051）：解码双缓冲 + 编码后台线程；视频 **85–92 → 79.2 ms/帧**，`demo-m3` 226.8 ms/帧，分段/续渲不变，输出逐位相同
- [x] **并行分段**（D052）：`--jobs N`（默认自动 min(3, 核数/4, 段数)）；30 秒素材 6 段，CPU 路径 **157.98 → 84.19 s（1.88×）**，GPU 路径 1.10×（单卡）；输出逐位相同，并行续渲正常
- [x] **长片预览代理**（D053）：整段进内存会把最长边压到 960 px 以下时生成 960 px 全 I 帧代理（`out/ui-proxy/`）；30 秒素材预览 **960×540 · 100 ms/帧**，重开 48 ms；4 秒素材仍走内存缓存 1600×898 · 47 ms
- [x] **真实点扩散核**（D054）：光晕 / 眩光 / 扩散改成“底片微米 + 双高斯重尾核”，按 `--frame-width-mm` 换算，1080p/4K 物理尺寸一致；`--spread-model box` 保留旧盒状模糊；CPU/GPU 用同一组核权重，parity **149–178 dB**；GPU 视频 82.2 ms/帧，`demo-gpu-texture` 2.87×
- [x] **浮点 / EXR**（D055）：自带最小 OpenEXR 读写（NONE 压缩、float/half、RGB），`.exr` 输出不钳制；`lut apply` / `convert` / `gen` / 序列 `--out-ext exr` 全支持；正常帧读回 **PSNR = inf**，+3 EV 时 33% 像素 >1 被保留；CI 双平台跑 EXR 冒烟
- [x] **macOS Metal 光谱**（D056）：`gpu_film_metal.mm`，运行时编译 MSL、shared buffer、无设备回退 CPU；macOS CI `gpu_film` 真跑通过；**必须 `fastMathEnabled = NO`**（否则 NaN 保护被优化掉，整帧 NaN）；真机 M1 首次出现 `MTLCreateSystemDefaultDevice` 返回 nil，已加 `MTLCopyAllDevices` 兜底与 `gpu-info`；质感仍 CPU（D057）
- [x] `scripts\demo-spread.cmd`：同一帧 / 同一段视频各渲 box 与 physical 两遍，打印 PSNR 和 ms/帧，并排对比
- [x] `scripts\demo-exr.cmd`：同一帧出 PNG16 / float EXR / half EXR，读回比对 + 超曝光对照
- [x] `scripts\demo-gpu-texture.cmd`：同一条命令跑 CPU / GPU 两遍，打印 ms/帧、加速比和编码后 PSNR
- [x] `scripts\build-ui.cmd`：配置 → 编译 → windeployqt → 启动
- [x] 界面不复制管线，预览和渲染都调用同一个 `spektra-roll-cli`
- [x] 截图：`docs/ui-preview.png`、`docs/ui-queue.png`
- [ ] 裁切、macOS 打包

![Qt UI](docs/ui-preview.png)

![Post-dev inspector](docs/ui-postdev.png)

![Render queue](docs/ui-queue.png)

## 这次交付（M1）

- [x] 导入 spektrafilm 的 Portra 400 / Portra Endura profile（CC BY-SA 4.0，已写署名）
- [x] C++ 实现最小光谱链：Mallett 上采样 → 灵敏度积分 → 特征曲线 → 印片 → 扫描
- [x] 印片机中性滤色片按上游数据库查表（0 / 51.568 / 52.534）
- [x] 烘焙真实 LUT：`lut bake`，33³ float32 `.cube`
- [x] parity harness：`lut compare`（PSNR + CIE76 ΔE），并冻结参考 LUT
- [x] 16-bit PNG 读写：`gen --bits 16`，`lut apply` 自动保持位深
- [x] 双击验收：`scripts\demo-m1.cmd`、`scripts\verify-m1.cmd`
- [x] 单元测试 4 个程序全绿（LUT / 胶片模型 / 图像 I/O / Apple Log）
- [x] 已推送 `main`，GitHub Actions Windows + macOS 双平台绿（CI 也会烘 LUT 并和参考对比）

## 这次交付（M2 · 已完成）

- [x] **Apple Log 输入**：`lut bake --input apple-log`，LUT 定义域就是 Apple Log 码值（D023）
- [x] Apple Log 解码和 colour-science 逐点一致：16-bit 输出误差 ≤ 1 LSB
- [x] `convert --from apple-log`：只解码、不上胶片的 SDR 对照图
- [x] **真实画面样张**：`scripts\demo-m2.cmd` 从你的 `.mov` 抽帧，出 before / 印片 / 负片 三张
- [x] **扫描负片模式**（D024）：`--scan negative`，天空从 0.88–0.91 变成 0.48–0.70（云有层次），暗部死黑 7.7%→2.5%
- [x] **负片为推荐路径**（D025），默认反差 **1.15**（D026，你确认）
- [x] **输出色域压缩**（D027）：OkLCh 色度压缩，Apple Log 印片负值 3,763→0，色相保持
- [x] **DIR 耦合剂**（D028）：Apple Log 默认开启，与上游参考 bake PSNR 80.75 dB
- [x] **HDR 输出**（D029）：PQ / HLG + 10-bit，白色默认 1000 nits；demo 出 HEVC 片段

## 怎么验收（M2）

1. 双击 `scripts\demo-m2.cmd` → 自动打开 `out\m2-negative.png`（负片，默认反差 1.15）和 `out\m2-negative-flat.png`（负片，不加反差）
2. 另外两张：`out\m2-before.png`（只解码）、`out\m2-after.png`（印片）
3. 看四件事：天空/窗户有没有层次、暗部有没有死黑、彩色招牌有没有串色、要留余量还是直接出片

## 实测数据（本机）

| 项 | 结果 |
|---|---|
| 编译器 | MinGW-w64 GCC 16.2.0 (ucrt, posix-seh) |
| 单元测试 | **9/9** 通过（含 GPU 颜色 / 质感 / 融合 parity，无显卡时 skip） |
| 33³ LUT 烘焙 | 108 ms（多线程）；单线程 198 ms |
| 2.6 MP 套 LUT（16-bit） | **17 ms**（多线程）；单线程 96 ms |
| 视频（2.6 MP，HEVC） | 无质感 187 ms/帧；全质感 **358.7 ms/帧**（缓存前 476.5）；4.1 s 素材约 35.5 秒 |
| GPU 光谱（2.6 MP） | 单帧 **47–50 ms**（CPU 1.1 s）；ProRes 视频 1427 → **379 ms/帧** |
| GPU 质感（2.6 MP，全质感） | **245.6 ms/帧**（CPU 382.7）；`demo-gpu-texture` 233.6 vs 386.8（1.66×） |
| 物理点扩散核（D054） | 同帧 box vs physical **57.03 dB**；GPU 视频 **82.2 ms/帧**（+3.8%）；LUT+CPU 质感 231.8 → 251.4 ms/帧；`demo-gpu-texture` CPU 236.7 / GPU **82.6（2.87×）**，编码后 85.6 dB |
| 浮点 EXR（D055） | 2.6 MP：float **30.0 MB** / half **15.0 MB** / PNG16 15.0 MB；读回 **PSNR = inf**；ffmpeg 独立解码 92.6 dB、half 82.8 dB；+3 EV 时 **32.97%** 像素 >1（max 5.34）保留 |
| Metal 光谱（D056） | macOS CI run 80 双绿；真机 M1：设备走 `MTLCopyAllDevices`（默认设备 nil），六组 parity 最差 **2.99e-4**，320×180 CPU vs Metal **115.44 dB**，单帧 10.8 → **7.1 ms**（含 PNG I/O） |
| 融合提交（D049，全质感） | **207.1 ms/帧**（分段 233.6，1.13×）；与分段输出 **逐位相同**；累计相对 D046 1.83× |
| 管道修复（D050） | **85–92 ms/帧**（207.1 → 2.4×）；`pack` 130 → 5 ms；输出逐位相同；H.264 + QSV 77 ms/帧 |
| 读写重叠（D051） | **79.2 ms/帧**；`demo-m3` 226.8；相对 D046 累计 **4.8×**；输出逐位相同 |
| 并行分段（D052） | 30 s 素材 6 段：CPU 路径 **157.98 → 84.19 s（1.88×）**；GPU 路径 53.17 → 48.42 s |
| 长片预览代理（D053） | 30 s 素材代理 8.7 s / 30.6 MB；预览 **960×540 · 100 ms/帧**；重开 48 ms；4 s 素材不变 |
| GPU 质感本身 | 约 **38 ms/帧**（CPU 约 160）；单帧 `lut apply` 309.5 → 214.5 ms |
| UI 预览（D048，1600×898） | **47–64 ms/帧**（旧链 1.7–2.2 s，约 30×）；光谱 GPU 43 ms（融合后 **38 ms**）；首次打开 2–4 s |
| 预览帧缓存 | 99 帧 × 8.6 MB ≈ **853 MB**（默认上限 1024 MB） |
| 分段 / 续渲 | 4 s 素材 5 段；删第 4 段后续渲只花约 5 s，输出仍是 99 帧 / 4.125 s / 原声 / timecode |
| 颗粒时间稳定性 | 帧间差 0.0025（rate 1）→ **0.0014**（rate 3） |
| C++ vs Python 参考 | **PSNR 88.07 dB**，ΔE76 max 0.0065 |
| Apple Log 解码 vs colour-science | max 1.53e-5（16-bit 1 LSB），mean 1.9e-8 |
| 印片 vs 扫描负片（同一帧） | 天空 0.88–0.91 vs 0.48–0.70；暗部 <0.10：7.7% vs 2.5% |
| 负片反差 1.0 / 1.15 / 1.3 | 天空 p5–p95 0.48–0.70 / 0.49–0.75 / 0.49–0.79；暗部 2.5% / 6.8% / 13.1% |
| 可执行文件 | `build\spektra-roll-cli.exe`，静态链接 |

## 怎么验收（M1）

1. 双击 `scripts\verify-m1.cmd` → 最后应出现 `PASS: M1 全部检查通过.`
2. 双击 `scripts\demo-m1.cmd` → 自动打开 `out\m1-before.png` 和 `out\m1-after.png`
3. 看三件事：灰阶暗部是否偏暖、肤色是否自然、红蓝是否过饱和

## 下一步（M5）

1. **D057：macOS 质感上 Metal**（颗粒 / 光晕 / 眩光 / 扩散），融合提交在 Mac 上重新打开
2. **Mac 实测**：`verify-macos.command` 记录 Metal 光谱的 ms/帧与 PSNR
3. **线性 / OCIO 交付**：EXR 现在是 display-encoded，不是场景线性
4. **EXR 压缩**、**代理清理 / 4K 预览**、**macOS Qt 打包**

> 已完成：D047 GPU 质感、D048 预览实时化、D049 融合提交、D050 管道修复、D051 读写重叠、D052 并行分段、D053 预览代理、D054 真实点扩散核、D055 浮点 EXR、D056 Metal 光谱。

## 需要你决定的事

- **D010**：OFX 插件做不做？（建议做，放第二期）
- **D011**：Apple Developer 账号什么时候买？（建议等有 Mac 再说）

> D009 已拍板：**Qt 6**（Qt Widgets），不用 WinUI 3。

## 风险 / 提醒

- macOS 已经本机验收过（MacBook Air M1），但**HDR 画面**只能在 iPhone 16 Pro 或 HDR 显示器上看。
- M1 的 LUT 是 display-encoded sRGB，不是线性/ACES；调色软件里要放在正确的色彩空间节点。
- 极端饱和输入会产生负的 LUT 值；Apple Log 路径已开 OkLCh 压缩，sRGB 路径仍靠钳制（保 parity）。
- 视频速度瓶颈在质感计算与管道，不在编码器；颗粒场缓存已做（D039），分段/续渲/时间码已做（D040）。
- Qt 界面不能只拷 exe：`build-ui` 里由 `windeployqt` 放好的 Qt DLL 和 `platforms\` 要一起带走（D009）。
