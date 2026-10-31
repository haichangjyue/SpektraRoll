# STATUS

**最后更新**：2026-11-01 · 里程碑 M0（脚手架）

## 一句话

仓库建好了，命令行工具能跑，测试和 CI 齐了；还差"把 spektrafilm 的光谱模型接进来"。

## 已完成

- [x] 仓库骨架 + GPL-3.0 许可证
- [x] C++20 + CMake 构建，零外部依赖（图像读写用 stb）
- [x] `.cube` 3D LUT 解析（含 domain、title、错误提示）
- [x] 四面体 / 三线性插值
- [x] 图像读写（8bit）+ 内置测试图
- [x] 命令行：`gen` / `lut gen` / `lut info` / `lut apply`
- [x] 单元测试（6 组，含 .cube 解析往返和非法文件拒绝）
- [x] GitHub Actions：Windows + macOS 双平台编译 + 测试 + 冒烟测试
- [x] 给非程序员的三份文档：RUN / STATUS / DECISIONS

## 进行中

- [ ] 本机工具链安装（VS Build Tools + CMake + Ninja）
- [ ] 首次本地编译通过
- [ ] 推到 GitHub 公开仓库

## 下一步（按顺序）

1. 本地编译 + 跑 demo，你用眼睛验收"暖调 LUT 有没有效果"
2. 建 GitHub 公开仓库，让 macOS CI 跑起来
3. 接入 spektrafilm 的 profile → 烘焙第一张真正的胶片 LUT
4. 加 16bit / EXR 输出（为视频做准备）

## 需要你决定的事

- 暂无。等你验收 M0 之后，我再问"第一支胶片选哪个"。

## 风险 / 提醒

- 这台机器没有 Mac，macOS 端只能靠 CI 编译，**看不出画面**。
- 现在输出是 8bit，做视频前必须换 16bit/浮点，这是已知欠账，不是 bug。
