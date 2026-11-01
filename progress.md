# SpektraRoll · 进度交接书

> **这份文件的用途**：让你在新开一个对话时，把这一份丢给 AI，它就能接着干，不需要你重新解释背景。
>
> **最后更新**：2026-11-01
> **当前里程碑**：M1 已完成并推送；M2 = 颜色正确（真实照片 + Apple Log/HDR，D020/D021）待启动
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
| 单元测试：LUT / 胶片模型 / 图像 I/O，3 个程序全绿 | ✅ |
| 双击验收：`scripts\demo-m1.cmd`、`scripts\verify-m1.cmd` | ✅ |
| parity：PSNR **88.07 dB**，ΔE76 max **0.0065** | ✅ |
| CI：Windows + macOS 都跑 `lut bake` + `lut compare`（D019） | ✅ 两个 job 绿 |
| 冻结参考 LUT：`tests/reference/portra400_endura_33.cube` | ✅ |

### 4. 改名与上线

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

### 5. 实测数据（本机，真实跑出来的）

| 项 | 结果 |
|---|---|
| CPU | Intel i5-10600KF |
| GPU | Intel Arc A750（支持 D3D12 / Vulkan） |
| 内存 | 32 GB |
| 编译器 | MinGW-w64 GCC 16.2.0（便携版，无管理员权限） |
| CMake | 4.4.4（`C:\Program Files\CMake\bin\cmake.exe`） |
| Ninja | 1.13.2（`X:\agent\tools\ninja\ninja.exe`） |
| 单元测试 | 3/3 程序通过（LUT 6 组 + 胶片模型 + 图像 I/O） |
| 33³ LUT 烘焙 | **约 245 ms**（单线程） |
| 0.92 MP 套 33³ LUT（16-bit） | **约 31 ms**（单线程） |
| C++ vs Python 参考 | **PSNR 88.07 dB**，max \\|diff\\| 4.57e-4，ΔE76 max 0.0065 |
| 印片曝光因子 | 0.837773013976（上游含一次 sRGB→sRGB 往返，0.837787256300） |
| 产物 | `build\spektra-roll-cli.exe`，静态链接，无 DLL 依赖 |

**重要推论**：31 ms/0.92 MP 对视频还是太慢（1080p 一帧约 0.3 s），必须上 SIMD / 多线程 / GPU，这是 M4 的任务，不是 bug。

---

## 二、关键决策（完整台账见 `DECISIONS.md`）

| 编号 | 决策 | 结论 | 后果 / 影响 |
|---|---|---|---|
| **D001** | 第一个可运行成果 | **Windows 命令行先跑通** | 没有 Mac，先在这台机器上闭环；macOS UI 推迟 |
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
| **待定 D009** | Windows UI 技术栈 | 建议 **WinUI 3（C++/WinRT）** | 尚未确认；选定后需要装 MSVC（目前只有 MinGW） |
| **待定 D010** | 是否做 OFX 插件 | 建议**做，放第二期** | 尚未确认 |
| **待定 D011** | Apple Developer 什么时候买 | 建议**等有 Mac 再说** | $99/年，现在不花 |
| **待定 D022** | 视频读写形态 | 独立程序（FFmpeg）还是 Resolve 插件 | 和 D010 一起定，M3 之前必须拍板 |

### 还有一些"我代你做的技术决定"（也记录在案）

- 图像读写先用 stb 而不是 OpenImageIO —— 为了 M0 零依赖、编译快
- 用 MinGW 而不是 MSVC —— 因为 winget 装 VS Build Tools 失败（msstore 源 `0x80072efd`）
- 用 `.cmd` 包装 `.ps1` —— 因为 PowerShell 执行策略禁止直接跑脚本
- 静态链接 `-static` —— 让用户拿到一个 exe 就能跑，不用找 DLL

---

## 三、未完成的待办

### 🔴 立即要做（M2 · 颜色正确）

1. **真实照片样张**：demo 里加一张 CC0 真实照片，前/后对比；合成测试图保留做数值校验（D020）
2. **输出色域压缩**：CAM16-UCS + 亮度压缩，把负值和超 1 的结果合法压回色域（D014 的欠账）
3. **耦合剂（couplers）**：先做 `dir_couplers`，用上游同一套参数（D015 的欠账）
4. **Apple Log 输入**：按 Apple 公开公式解码，转到线性宽色域（D021）
5. **HDR 输出**：PQ / HLG + Rec.2020 + 10-bit，HDR 曝光映射做成参数（D021）
6. **LUT 形态**：HDR 用 1D shaper + 3D LUT（或 65³）

**M2 前置条件**：你提供的 iPhone Apple Log / ProRes 素材，放到 `X:\agent\footage\`，告诉我文件名即可。

### 🟡 中期（M2–M5）

| 阶段 | 内容 |
|---|---|
| **M2** | 真实照片样张、色域压缩、耦合剂、Apple Log 输入、HDR 输出（D020/D021） |
| **M3** | FFmpeg / ProRes 视频读写、时间维度、帧区间批处理；同时拍板 D022（独立程序 vs Resolve 插件） |
| **M4** | 颗粒 / 光晕 / 扩散 / 眩光、确定性种子、SIMD / 多线程 / GPU、浮点 / EXR |
| **M5** | Windows WinUI 3（**需要先装 MSVC**）、macOS SwiftUI（需要 Mac）、OFX 插件、渲染农场命令行 |

### 🟢 还没开始、但必须记住的欠账

- [ ] **没有 Mac** → macOS 端目前只有"能编译"的证据，没有"画面对不对"的证据
- [x] ~~8bit 输出~~ → M1 已支持 16-bit PNG；浮点 / EXR 仍未做
- [ ] **没有色彩管理** → 没有 OCIO、没有 ACES 输入变换；现在只有 display-encoded sRGB，Apple Log/HDR 是 M2 的任务（D021）
- [ ] **没有时间维度** → 现在的 CLI 只处理单张图，还不能读视频 / 图像序列
- [ ] **没有确定性颗粒种子** → 计划 `seed = f(timecode, layer, sublayer)`，M4 实现
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

### 你（产品负责人）只需要双击的六个文件

```
scripts\dev-install.cmd   装工具链（只做一次，不需要管理员权限）
scripts\build.cmd         编译 + 跑测试
scripts\demo-m1.cmd       M1 真正的胶片样张（前后对比，自动打开两张图）
scripts\verify-m1.cmd     一键验收：编译 + 测试 + 烘 LUT + 和参考对比
scripts\demo.cmd          M0 的合成 LUT 冒烟测试（不是胶片）
scripts\push.cmd          登录 GitHub 并推送
```

### 验证 M1 是否还健康

```powershell
cd X:\agent\SpektraRoll
.\build\spektra-roll-cli.exe version        # 应输出 spektra-roll-cli 0.1.0
ctest --test-dir build --output-on-failure # 应 3/3 通过
.\build\spektra-roll-cli.exe lut bake --out out\check.cube
.\build\spektra-roll-cli.exe lut compare --a tests\reference\portra400_endura_33.cube --b out\check.cube
# 应输出 PSNR ≈ 88 dB, PASS
```

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

### M2 素材（Apple Log / ProRes）

```
X:\agent\footage\            你提供的 iPhone 素材放这里（仓库外，不进 git）
```

M2 的色彩验收需要真素材：至少一段 Apple Log（10-bit HEVC 或 ProRes），
再尽量有同场景的普通 sRGB 片段做对照。

---

## 五、仓库结构速查

```
X:\agent\SpektraRoll\
├── src/core/include/sr/     公开头文件（lut3d, image, json, profile, film_model, metrics, spectral_data）
├── src/core/src/            实现 + 生成的 spectral_tables.cpp
├── src/cli/main.cpp         命令行入口（gen / bake / apply / compare / info / version）
├── tests/                   3 个测试程序 + reference/ 冻结参考 LUT
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
├── DECISIONS.md             决策台账 D001–D021
├── progress.md              本文件
└── LICENSE                  GPL-3.0
```

---

## 六、下游 AI 的“别做的事”清单

- ❌ **不要推翻 D001–D021**，那些是已经执行并验收/记录过的
- ❌ **不要问用户技术细节问题**，只能问“选 A 还是 B”
- ❌ **不要在没跑 `verify-m1.cmd` 的情况下改光谱链**，改完 PSNR 掉了要立刻知道
- ❌ **不要把 display-encoded sRGB 的 LUT 说成线性/ACES LUT**（D013）
- ❌ **不要把 16-bit PNG 当成终点**，浮点 / EXR 仍是欠账（D017）
- ❌ **不要承诺“视觉验收”**，没有 Mac 就看不到 macOS 画面
- ❌ **不要偷懒改品牌**，SpektraRoll 已定案，改一次要动二进制名/CI/文档

---

## 七、下一次会话的理想开场

你说一句：

> “读 progress.md，我们从 M2 开始。”

AI 就应该先跑一遍 `scripts\verify-m1.cmd` 确认基线没坏，然后按 D020 的顺序做：

1. 真实照片样张（demo 换成照片，测试图保留做数值校验）
2. 输出色域压缩 → 耦合剂
3. Apple Log 输入 → HDR（PQ/HLG）输出

每一步都要重新跑 `lut compare`，PSNR 不能从 88 dB 掉下来。
