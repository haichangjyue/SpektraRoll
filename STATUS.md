# STATUS

**最后更新**：2026-11-01 · 里程碑 **M0 完成** · 项目名 **SpektraRoll**

## 一句话

仓库能编译、测试全绿、双击就能看到效果；下一步把 spektrafilm 的光谱模型接进来，出第一张真正的胶片 LUT。

仓库：https://github.com/haichangjyue/SpektraRoll
本地：`X:\agent\SpektraRoll`

## M0 交付清单

- [x] 仓库骨架 + GPL-3.0 许可证 + 线尾统一
- [x] C++20 + CMake + Ninja，零外部依赖（stb 单头文件）
- [x] `.cube` 解析：TITLE / LUT_3D_SIZE / DOMAIN_MIN/MAX / 注释 / 错误定位
- [x] 四面体 + 三线性插值，越界钳制
- [x] PNG 读写 + 内置测试图（灰阶 + 12 色块）
- [x] 命令行：`gen` / `lut gen` / `lut info` / `lut apply`（含 `--strength`）
- [x] 单元测试 6 组，**全部通过**
- [x] GitHub Actions：Windows + macOS 双平台编译 / 测试 / 冒烟
- [x] 无管理员权限的便携工具链（CMake / Ninja / MinGW 16.2）
- [x] 双击即用的 `.cmd` 脚本（装环境 / 编译 / 看效果）
- [x] 文档：RUN / DECISIONS / architecture / licensing

## 实测数据（本机）

| 项 | 结果 |
|---|---|
| 编译器 | MinGW-w64 GCC 16.2.0 (ucrt, posix-seh) |
| CMake | 4.4.4 |
| 单元测试 | 6/6 通过 |
| 1.44 MP 套 33³ LUT | **49.3 ms**（单线程，未优化） |
| 可执行文件 | `build\spektra-roll-cli.exe`，静态链接，无 DLL 依赖 |

## 下一步（M1）

1. 把 spektrafilm 的 profile（Portra 400 + Endura）读进来
2. 实现光谱 → 密度的最小链路，先不做耦合剂和颗粒
3. 烘焙第一张真正的胶片 LUT，和上游 Python 输出对比（parity）
4. 换成 16bit / EXR 输出

## 需要你决定的事

- **D008**：第一支胶片选哪个？我建议 **Kodak Portra 400 + Portra Endura**（最经典、数据最全）
- **D009**：产品名继续叫 `SpektraRoll` 吗？还是现在就想一个正式名字

## 风险 / 提醒

- 本机没有 Mac，macOS 端目前只有 CI 编译证据，**没有画面证据**。
- 现在是 8bit 且没有色彩管理，这是 M0 的刻意取舍，M1 会补。
- 单线程 49 ms/1.44MP 意味着 1080p 一帧约 0.4 s；视频必须上 SIMD/多线程/GPU，这是 M2 的事。
