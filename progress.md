# SpektraRoll · 进度交接书

> **这份文件的用途**：让你在新开一个对话时，把这一份丢给 AI，它就能接着干，不需要你重新解释背景。
>
> **最后更新**：2026-11-01
> **当前里程碑**：M0 已完成并验收，M1 待启动
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

### 3. 改名与上线

- [x] 产品名定案 **SpektraRoll**，全仓库同步改名
  - 本地目录 `X:\agent\spektravideo` → `X:\agent\SpektraRoll`
  - CMake 工程名 `project(SpektraRoll)`
  - 二进制 `spektravideo-cli` → `spektra-roll-cli`
  - C++ 命名空间 `sv::` → `sr::`，头文件目录 `src/core/include/sr/`
  - CI、脚本、文档全部同步
- [x] GitHub 公开仓库建立并推送成功
- [x] GitHub 登录凭据问题解决（`scripts\push.cmd` 一键登录 + 推送）
- [x] GitHub Actions 双平台首次跑通，**Windows 和 macOS 都绿**
- [x] git 历史 5 个 commit，工作区干净，已全部推送到 `main`

### 4. 实测数据（本机，真实跑出来的）

| 项 | 结果 |
|---|---|
| CPU | Intel i5-10600KF |
| GPU | Intel Arc A750（支持 D3D12 / Vulkan） |
| 内存 | 32 GB |
| 编译器 | MinGW-w64 GCC 16.2.0（便携版，无管理员权限） |
| CMake | 4.4.4（`C:\Program Files\CMake\bin\cmake.exe`） |
| Ninja | 1.13.2（`X:\agent\tools\ninja\ninja.exe`） |
| 单元测试 | 6/6 通过 |
| 1.44 MP 套 33³ LUT | **49.3 ms**（单线程，未优化）≈ 1080p 一帧 0.4 s |
| 产物 | `build\spektra-roll-cli.exe`，静态链接，无 DLL 依赖 |

**重要推论**：49 ms/1.44MP 意味着视频处理必须上 SIMD / 多线程 / GPU，这是 M2 的任务，不是 bug。

---

## 二、关键决策（完整台账见 `DECISIONS.md`）

| 编号 | 决策 | 结论 | 后果 / 影响 |
|---|---|---|---|
| **D001** | 第一个可运行成果 | **Windows 命令行先跑通** | 没有 Mac，先在这台机器上闭环；macOS UI 推迟 |
| **D002** | 代码托管 | **现在建公开 GitHub 仓库** | 公开仓库的 macOS CI runner 免费，正好补上没有 Mac 的缺口 |
| **D003** | 许可证 | **GPL-3.0-or-later** | 不能上 Mac App Store；OFX 插件也必须 GPL；profile 数据仍 CC BY-SA 4.0 |
| **D004** | 图像 I/O（M0） | **stb_image / stb_image_write（8bit）** | 只能 8bit，无色彩管理。**这是欠账，M1/M2 要换 OpenImageIO** |
| **D005** | 颜色变换实现形态 | **颜色烘成 3D LUT（33³ float32、四面体插值），空间效果走 GPU pass** | 视频能实时的前提；LUT 分辨率低于 33³ 会在暗部出色带 |
| **D006** | 构建系统 | **CMake ≥3.20 + Ninja + C++20** | Windows 用 MSVC，macOS 用 Clang，CI 两边一致 |
| **D007** | 产品名 / 仓库名 | **SpektraRoll**（你已拍板，正式生效） | 二进制 `spektra-roll-cli`，命名空间 `sr`，头文件 `sr/` |
| **待定 D008** | 第一支胶片 | 建议 **Kodak Portra 400 + Portra Endura** | 尚未确认，**M1 被这个卡住** |
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

### 🔴 立即要做（M1，需要你先拍 D008）

1. **等你确认 D008：第一支胶片**
   - A. Kodak Portra 400 + Portra Endura（推荐：数据最全，最容易判断"像不像"）
   - B. Kodak Vision3 250D + 2383（电影感更强，更贴合视频定位，但链路复杂）
   - C. 两套都做（工作量约 ×1.5，好处是能验证引擎真的在区分不同胶片）

2. **M1 技术任务**（D008 定了就能开工）
   - [ ] 引入 spektrafilm 的 profile 数据（JSON，CC BY-SA 4.0，需保留署名）
   - [ ] 实现最小光谱 → 密度链路：光谱灵敏度积分 + 特征曲线 + 印片曝光 + 扫描
   - [ ] **先不做**耦合剂（couplers）、颗粒、光晕、扩散（这些是 M2+）
   - [ ] 烘焙第一张"真正的胶片 LUT"（33³ float32、`.cube`）
   - [ ] 建 parity harness：与上游 Python 输出逐像素对比（PSNR / ΔE）
   - [ ] 把 8bit 图像 I/O 换成 16bit / EXR（引入 OpenImageIO）

3. **M1 每步都要你能验收**
   - 标准：双击一个 `.cmd` → 出一张"胶片样张" → 你用眼睛判断像不像

### 🟡 中期（M2–M5）

| 阶段 | 内容 |
|---|---|
| **M2** | GPU 颜色 + 空间效果（颗粒 / 光晕 / 扩散 / 眩光）、16bit I/O、SIMD/多线程 |
| **M3** | macOS SwiftUI 应用（需要 Mac 或远程验收人，否则只能 CI 编译，看不到画面） |
| **M4** | Windows WinUI 3 应用（**需要先装 MSVC**，MinGW 做不了 WinUI 3） |
| **M5** | OFX 插件（DaVinci Resolve 等）+ 渲染农场命令行（帧区间 / 断点续渲） |

### 🟢 还没开始、但必须记住的欠账

- [ ] **没有 Mac** → macOS 端目前只有"能编译"的证据，没有"画面对不对"的证据
- [ ] **8bit 输出** → 做视频前必须换 16bit / 浮点，否则一调色就断层
- [ ] **没有色彩管理** → 没有 OCIO、没有传递函数、没有 ACES 输入变换
- [ ] **没有时间维度** → 现在的 CLI 只处理单张图，还不能读视频 / 图像序列
- [ ] **没有确定性颗粒种子** → 计划 `seed = f(timecode, layer, sublayer)`，M2 实现
- [ ] **Windows 代码签名** → 可选，Azure Trusted Signing 约 $10/月，不签用户会看到"未知发布者"
- [ ] **法律风险未处理** → 上游 README 声称"直接受其方法启发的作品也算衍生"，若将来想闭源必须先找 Andrea Volpato 谈双授权

---

## 四、环境与运维备忘（重要，别重复踩坑）

### 本机工具链位置

```
CMake   C:\Program Files\CMake\bin\cmake.exe      (4.4.4)
Ninja   X:\agent\tools\ninja\ninja.exe            (1.13.2)
GCC     X:\agent\tools\mingw64\bin\g++.exe        (16.2.0 ucrt posix-seh)
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

### 你（产品负责人）只需要双击的四个文件

```
scripts\dev-install.cmd   装工具链（只做一次，不需要管理员权限）
scripts\build.cmd         编译 + 跑测试
scripts\demo.cmd          出样张并打开文件夹
scripts\push.cmd          登录 GitHub 并推送
```

### 验证 M0 是否还健康

```powershell
cd X:\agent\SpektraRoll
.\build\spektra-roll-cli.exe version        # 应输出 spektra-roll-cli 0.1.0
ctest --test-dir build --output-on-failure # 应 100% 通过
```

---

## 五、仓库结构速查

```
X:\agent\SpektraRoll\
├── src/core/include/sr/     公开头文件（lut3d.hpp, image.hpp）
├── src/core/src/            实现（lut3d.cpp, image.cpp）
├── src/cli/main.cpp         命令行入口
├── tests/test_lut3d.cpp     6 组测试
├── third_party/stb/         stb_image / stb_image_write（单头文件）
├── scripts/                 .ps1 + .cmd 双份，.cmd 给用户双击
├── .github/workflows/       build.yml：Windows + macOS 矩阵
├── docs/architecture.md     目标架构、LUT 拆分、确定性规则、M0–M5 路线图
├── docs/licensing.md        GPL / CC BY-SA / LUT / 商标 / 编解码专利约束
├── README.md                对外介绍
├── RUN.md                   给非程序员的操作手册
├── STATUS.md                一页纸进度
├── DECISIONS.md             决策台账 D001–D011
├── progress.md              本文件
└── LICENSE                  GPL-3.0
```

---

## 六、下游 AI 的"别做的事"清单

- ❌ **不要推翻 D001–D007**，那些是已经执行并验收过的
- ❌ **不要问用户技术细节问题**，只能问"选 A 还是 B"
- ❌ **不要在没确认 D008 的情况下开写光谱模型**，选错胶片等于白做
- ❌ **不要把 8bit 当成最终方案**，它是 M0 的临时取舍
- ❌ **不要承诺"视觉验收"**，没有 Mac 就看不到 macOS 画面
- ❌ **不要偷懒改品牌**，SpektraRoll 已定案，改一次要动二进制名/CI/文档

---

## 七、下一次会话的理想开场

你说一句：

> "读 progress.md，我们从 M1 开始。D008 我选 A（Portra 400 + Portra Endura）。"

AI 就应该直接开始：拉上游 profile → 写光谱积分 → 烘第一张真 LUT → 建 parity 对比 → 给你一个能双击看样张的东西。
