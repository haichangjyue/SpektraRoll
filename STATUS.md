# STATUS

**最后更新**：2026-11-01 · 里程碑 **M1 完成** · 项目名 **SpektraRoll**

## 一句话

现在能烘出一张**真正的柯达 Portra 400 + Portra Endura LUT**，双击就能看前后对比；和上游 Python 逐点对比 PSNR 88 dB，人眼完全看不出差别。

仓库：https://github.com/haichangjyue/SpektraRoll
本地：`X:\agent\SpektraRoll`

## 这次交付（M1）

- [x] 导入 spektrafilm 的 Portra 400 / Portra Endura profile（CC BY-SA 4.0，已写署名）
- [x] C++ 实现最小光谱链：Mallett 上采样 → 灵敏度积分 → 特征曲线 → 印片 → 扫描
- [x] 印片机中性滤色片按上游数据库查表（0 / 51.568 / 52.534）
- [x] 烘焙真实 LUT：`lut bake`，33³ float32 `.cube`
- [x] parity harness：`lut compare`（PSNR + CIE76 ΔE），并冻结参考 LUT
- [x] 16-bit PNG 读写：`gen --bits 16`，`lut apply` 自动保持位深
- [x] 双击验收：`scripts\demo-m1.cmd`、`scripts\verify-m1.cmd`
- [x] 单元测试 3 个程序全绿（LUT / 胶片模型 / 图像 I/O）

## 实测数据（本机）

| 项 | 结果 |
|---|---|
| 编译器 | MinGW-w64 GCC 16.2.0 (ucrt, posix-seh) |
| 单元测试 | 3/3 通过 |
| 33³ LUT 烘焙 | **约 245 ms**（单线程） |
| 0.92 MP 套 LUT（16-bit） | **约 31 ms** |
| C++ vs Python 参考 | **PSNR 88.07 dB**，ΔE76 max 0.0065 |
| 可执行文件 | `build\spektra-roll-cli.exe`，静态链接 |

## 怎么验收

1. 双击 `scripts\verify-m1.cmd` → 最后应出现 `PASS: M1 全部检查通过.`
2. 双击 `scripts\demo-m1.cmd` → 自动打开 `out\m1-before.png` 和 `out\m1-after.png`
3. 看三件事：灰阶暗部是否偏暖、肤色是否自然、红蓝是否过饱和

## 下一步（M2）

1. 输出色域压缩（CAM16-UCS），把负值和超 1 的结果合法地压回 sRGB
2. 耦合剂（couplers），先做 `dir_couplers`
3. 颗粒 / 光晕 / 扩散，确定性种子 `seed = f(timecode, layer, sublayer)`
4. SIMD / 多线程 / GPU，把 0.92 MP 的 31 ms 压到视频可用
5. hanatos2025 上采样作为可选项

## 需要你决定的事

- **D009**：Windows UI 用 WinUI 3 还是 Qt？（建议 WinUI 3，但要先装 MSVC）
- **D010**：OFX 插件做不做？（建议做，放第二期）
- **D011**：Apple Developer 账号什么时候买？（建议等有 Mac 再说）

## 风险 / 提醒

- 本机没有 Mac，macOS 端仍然只有 CI 编译证据，**没有画面证据**。
- M1 的 LUT 是 display-encoded sRGB，不是线性/ACES；调色软件里要放在正确的色彩空间节点。
- 极端饱和输入会产生负的 LUT 值，写图时被钳制；这是 D014 的已知取舍。
- 单线程 31 ms/0.92 MP ≈ 1080p 一帧 0.3 s，视频仍需 SIMD/多线程/GPU，这是 M2 的事。
