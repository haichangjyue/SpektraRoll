# STATUS

**最后更新**：2026-11-01 · 里程碑 **M3 进行中**（视频读写已通）· 项目名 **SpektraRoll**

## 一句话

`spektra-roll-cli video` 现在能直接吃 iPhone 的 **Apple Log** 视频：逐帧套 Portra 400 光谱 LUT，带原声输出 SDR 或 **HDR（PQ/HLG）**，一段 4 秒 99 帧的素材约 30 秒跑完。

仓库：https://github.com/haichangjyue/SpektraRoll
本地：`X:\agent\SpektraRoll`

## M3 进行中：视频读写已通

- [x] `spektra-roll-cli video --lut x.cube --in clip.mov --out graded.mov [--trc pq]`
- [x] 逐帧解码 → 套 LUT → 重新编码，原声保留，HDR 自动带 PQ/HLG 标记
- [x] 自动处理竖拍素材的旋转（-90 元数据）
- [x] 99 帧 2.6 MP 素材约 30 秒（268 ms/帧，单线程）
- [x] 帧区间：`--start 00:00:12 --frames 240`（音画同步）
- [x] ProRes 422 HQ 输出：`--codec prores`
- [ ] 断点续渲 / 时间码轨道
- [ ] 多线程提速（M4）

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

## 这次交付（M2 · 进行中）

- [x] **Apple Log 输入**：`lut bake --input apple-log`，LUT 定义域就是 Apple Log 码值（D023）
- [x] Apple Log 解码和 colour-science 逐点一致：16-bit 输出误差 ≤ 1 LSB
- [x] `convert --from apple-log`：只解码、不上胶片的 SDR 对照图
- [x] **真实画面样张**：`scripts\demo-m2.cmd` 从你的 `.mov` 抽帧，出 before / 印片 / 负片 三张
- [x] **扫描负片模式**（D024）：`--scan negative`，天空从 0.88–0.91 变成 0.48–0.70（云有层次），暗部死黑 7.7%→2.5%
- [x] **负片为推荐路径**（D025），默认反差 **1.15**（D026，你确认）
- [x] **输出色域压缩**（D027）：OkLCh 色度压缩，Apple Log 印片负值 3,763→0，色相保持
- [x] **DIR 耦合剂**（D028）：Apple Log 默认开启，与上游参考 bake PSNR 80.75 dB
- [x] **HDR 输出**（D029）：PQ / HLG + 10-bit，白色默认 1000 nits；demo 出 HEVC 片段
- [ ] 输出色域压缩（D014 欠账）
- [ ] 耦合剂（D015 欠账）
- [ ] HDR 输出：PQ / HLG + Rec.2020 + 10-bit（D021）

## 怎么验收（M2）

1. 双击 `scripts\demo-m2.cmd` → 自动打开 `out\m2-negative.png`（负片，默认反差 1.15）和 `out\m2-negative-flat.png`（负片，不加反差）
2. 另外两张：`out\m2-before.png`（只解码）、`out\m2-after.png`（印片）
3. 看四件事：天空/窗户有没有层次、暗部有没有死黑、彩色招牌有没有串色、要留余量还是直接出片

## 实测数据（本机）

| 项 | 结果 |
|---|---|
| 编译器 | MinGW-w64 GCC 16.2.0 (ucrt, posix-seh) |
| 单元测试 | 4/4 通过 |
| 33³ LUT 烘焙 | **约 245 ms**（单线程） |
| 0.92 MP 套 LUT（16-bit） | **约 31 ms** |
| C++ vs Python 参考 | **PSNR 88.07 dB**，ΔE76 max 0.0065 |
| Apple Log 解码 vs colour-science | max 1.53e-5（16-bit 1 LSB），mean 1.9e-8 |
| 印片 vs 扫描负片（同一帧） | 天空 0.88–0.91 vs 0.48–0.70；暗部 <0.10：7.7% vs 2.5% |
| 负片反差 1.0 / 1.15 / 1.3 | 天空 p5–p95 0.48–0.70 / 0.49–0.75 / 0.49–0.79；暗部 2.5% / 6.8% / 13.1% |
| 可执行文件 | `build\spektra-roll-cli.exe`，静态链接 |

## 怎么验收（M1）

1. 双击 `scripts\verify-m1.cmd` → 最后应出现 `PASS: M1 全部检查通过.`
2. 双击 `scripts\demo-m1.cmd` → 自动打开 `out\m1-before.png` 和 `out\m1-after.png`
3. 看三件事：灰阶暗部是否偏暖、肤色是否自然、红蓝是否过饱和

## 下一步（M2 · 颜色正确）

1. 真实照片样张（demo 前/后对比，测试图保留做数值校验）
2. 输出色域压缩（CAM16-UCS），把负值和超 1 的结果合法地压回色域
3. 耦合剂（couplers），先做 `dir_couplers`
4. Apple Log 输入（按 Apple 公开公式解码到线性宽色域）
5. HDR 输出（PQ / HLG + Rec.2020 + 10-bit，HDR 曝光映射做成参数）
6. 视频编解码（ProRes / FFmpeg）放到 M3；颗粒 / 光晕 / SIMD 顺延到 M4

## 需要你决定的事

- **D009**：Windows UI 用 WinUI 3 还是 Qt？（建议 WinUI 3，但要先装 MSVC）
- **D010**：OFX 插件做不做？（建议做，放第二期）
- **D011**：Apple Developer 账号什么时候买？（建议等有 Mac 再说）
- **D022**：ProRes 读写走独立程序（FFmpeg）还是 Resolve 插件？（M3 之前定）

## 风险 / 提醒

- 本机没有 Mac，macOS 端仍然只有 CI 编译证据，**没有画面证据**。
- M1 的 LUT 是 display-encoded sRGB，不是线性/ACES；调色软件里要放在正确的色彩空间节点。
- 极端饱和输入会产生负的 LUT 值，写图时被钳制；这是 D014 的已知取舍。
- 单线程 31 ms/0.92 MP ≈ 1080p 一帧 0.3 s，视频仍需 SIMD/多线程/GPU，这是 M4 的事。
