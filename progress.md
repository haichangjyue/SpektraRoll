# SpektraRoll · 进度交接书

> **这份文件的用途**：让你在新开一个对话时，把这一份丢给 AI，它就能接着干，不需要你重新解释背景。
>
> **最后更新**：2026-11-03（D049–D059 已整理成交接摘要）
> **当前里程碑**：**M5 进行中 · 双平台 GPU + 线性浮点 + ZIP 压缩**（Qt 6 三栏界面 + 渲染队列 + 光谱/GPU 引擎 + GPU 质感 + 物理点扩散核 + 常驻预览 + 融合提交 + 管道/并行 + 预览代理 + float/half/linear EXR + ZIP + Metal 颜色与质感，D041–D059）
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

## 会话 D059 · EXR ZIP 压缩

会话目标：D058 之后按“先降体积、OCIO 单开一轮”的顺序做 **EXR ZIP**。

### 一、已完成的事项

**D059 · ZIP 压缩成为默认**
- EXR 默认 ZIP（16 scanline / chunk），`--exr-raw` 回到不压缩；读取器同时支持 NONE 和 ZIP。
- 压缩用 stb 的 zlib（16-bit PNG 已在用），没有新依赖；压不动的 chunk 按规范回退成原始块。
- 预处理与 OpenEXR `ImfZip` 对齐：偶数字节前半、奇数字节后半，再做逐字节差分。
- 新增 `scripts\bench-macos.command`：在 Mac 上跑 2.6 MP 的 CPU vs Metal（颜色 / 质感）四组计时。

### 二、一条关键决策（详见 DECISIONS D059）

| 编号 | 决策 | 结论 |
|---|---|---|
| D059 | EXR 压缩 | 默认 ZIP、可 `--exr-raw`；逐块回退；不引入新依赖 |

### 三、踩过并修掉的坑

1. **偶数/奇数拆分方向写反**：自己读自己对称，测试全过，但 ffmpeg 解码只有 **7.2 dB**；修正后 91.5 dB。教训：格式实现必须用第二个解码器交叉验证。
2. 压不动时必须存原始块（`packedSize == rawSize`），否则读取端会去 inflate 一份没压缩的数据。

### 四、实测（2160×1214）

| 内容 | 未压缩 | ZIP | 变化 |
|---|---|---|---|
| 已调色帧（identity LUT） | 31.49 MB | **22.73 MB** | −28% |
| 带颗粒的成片 | 31.49 MB | **31.47 MB** | ≈0% |
| 半精度（同一成片） | 15.74 MB | **9.82 MB** | −38% |
| 640×360 合成图表 | 2.77 MB | **38.7 KB** | −98.6% |

写盘耗时 16.0 → 22.6 ms/帧（含 PNG 读、颜色、写盘）；ffmpeg 解码 92.6 dB。**float 的颗粒会吃掉 zlib 的压缩率**，half 明显更好；再想小需要 PIZ/DWA。

### 五、为什么没接着做 OCIO

OCIO 需要引入 OpenColorIO 库和配置文件，Windows / macOS CI 都要装；没装它的环境里写出来的代码无法真跑。这一轮先把不依赖外部库、能立即验证的 ZIP 做完；OCIO 需要单独一轮先定“引入依赖还是只做 OCIO-lite”。

### 六、未完成的待办

- [ ] **Mac 2.6 MP 实测**：双击 `scripts\bench-macos.command`，把四组 ms 发回来
- [ ] **OCIO**：先定依赖策略
- [ ] **PIZ / DWA 压缩**：颗粒重的素材可再小 2–3 倍
- [ ] **代理清理 / 4K 预览**、**macOS Qt 打包**

---

## 会话 D058 · 线性 EXR

会话目标：D057 之后按产品负责人的选择做 **A · 线性 EXR**。上一版 EXR 是 display-encoded，进调色软件容易多套一次传输函数。

### 一、已完成的事项

**D058 · `--exr-linear`**
- `--exr-linear` + `--exr-trc srgb|pq|hlg` + `--peak`：写 EXR 前把显示编码值反解成线性光，**1.0 = 参考白**（sRGB/HLG 是显示白，PQ 是 `--peak` 尼特）。
- 核心 `sr::linearizeImage`：sRGB 解码**不钳制**（超白保留）；PQ 用 `pqDecode/peak`；HLG 用 `hlgDecode`。
- 接入 `lut apply`（单帧 + `--in-dir` 序列）、`convert`、`gen`；输出不是 `.exr` 时直接报错。
- 写盘前打印 `EXR: linear light (...)`，范围统计随之变成线性值。
- 测试：sRGB 0.5 → 0.214、1.5 的码值 → 2.53（超白保留）、PQ 1000 nits → 1.0、HLG 0.25 往返、非法 transfer 报错。
- `scripts\demo-exr.cmd` 新增 `out\exr-frame-linear.exr` 与对应说明。

### 二、一条关键决策（详见 DECISIONS D058）

| 编号 | 决策 | 结论 |
|---|---|---|
| D058 | 线性浮点交付 | 可选 `--exr-linear`；1.0 = 白/峰值；不钳制；不改默认 |

### 三、实测（2160×1214）

| 项 | 结果 |
|---|---|
| 同一帧 range | display 0.0160..0.8870 → linear **0.0012..0.7619** |
| `convert --exposure 3` | display max 5.34 → linear max **50.3**，仍不钳制 |
| 体积 / 耗时 | 与普通 EXR 相同（float 30 MB / half 15 MB） |

### 四、怎么用

```powershell
# 线性 sRGB EXR
.\build\spektra-roll-cli.exe lut apply --lut out\m3-applelog-sdr.cube `
  --in out\exr-frame.png --out out\frame-linear.exr --exr-linear

# HDR PQ（LUT 用 --hdr pq 烘的）：1.0 = 1000 nits
.\build\spektra-roll-cli.exe lut apply --lut out\hdr.cube `
  --in out\frame.png --out out\frame-linear-pq.exr --exr-linear --exr-trc pq --peak 1000

# 序列
.\build\spektra-roll-cli.exe lut apply --lut out\x.cube --in-dir frames `
  --out-dir frames_linear --out-ext exr --exr-linear
```

### 五、未完成的待办

- [ ] **OCIO**：线性 EXR 只是“按线性解读”，还没有 ACES/相机矩阵/显示变换
- [ ] **EXR 压缩**（ZIP/PIZ）
- [ ] **代理清理 / 4K 预览**、**macOS Qt 打包**

---

## 会话 D057 · macOS Metal 质感

会话目标：D056 让 macOS 的颜色上了 Metal，但质感还在 CPU（约 160 ms/帧），融合提交在 Mac 上用不了。这一轮把质感链也搬到 Metal。

### 一、已完成的事项

**D057 · macOS 质感走 Metal**
- 新增 `src/cli/gpu_texture_metal.mm`（Objective-C++，仅 Apple 编译），实现同一个 `GpuTexture` 接口；CMake 在 Apple 上用它替换 `gpu_texture.cpp`。
- 14 个 compute pass 逐行翻成 MSL：光晕/亮度、盒子模糊（一行/一列一个线程、累加顺序不变）、物理 PSF 卷积、降采样/升采样、颗粒场/叠加、三个 composite、copy。
- **整条链一个 command buffer**，最后只等一次；参数与 PSF taps 都用 `setBytes` 内联，避免共享缓冲在链内被覆盖。
- 融合路径直接借用 film 后端的 `MTLBuffer` 当第一遍输入（不拷贝 42 MB），之后 ping-pong 回自己的缓冲，绝不覆盖 film 结果。
- `fastMathEnabled = NO` 与 film 保持一致；无 Metal 设备时回退 CPU。
- `scripts/verify-macos.command` 第 5 步增加 `test_gpu_texture` 的输出（真机上可直接看到每组 parity 的 dB）。

### 二、一条关键决策（详见 DECISIONS D057）

| 编号 | 决策 | 结论 |
|---|---|---|
| D057 | macOS GPU 质感 | 14 pass 全上 Metal；一个 command buffer + `setBytes`；融合提交在 Mac 重新启用 |

### 三、踩过并修掉的坑

1. **`dispatchThreads` 收的是总线程数**，不是 D3D 的 group 数；照搬会把线程数缩小 64 倍。改用 `dispatchThreadgroups:` 并显式给 `(1,64)` / `(64,1)`。
2. PSF taps 放共享缓冲会被同一 command buffer 里的下一个效果覆盖 → 改成 `setBytes` 内联。
3. 融合路径第一遍读 film 的缓冲，交换后不能把它当写入目标，否则覆盖 film 结果。
4. ARC 下不能对 `id` 数组用非 const 引用遍历（编译错误）→ 索引循环。
5. `fastMathEnabled` 在 macOS 15 SDK 标记弃用（仅警告），继续用于兼容旧系统。

### 四、实测 / 状态

| 项 | 结果 |
|---|---|
| macOS CI（run 85） | **9/9 全过**：`gpu_texture` **2.11 s**、`gpu_chain` **2.10 s**（之前 skip） |
| Windows 本机 | 9/9 全过，D3D11 路径未受影响 |
| 真机 M1 | `test_gpu_texture` 九组 **逐位相同**（worst 0.0）；融合 vs CPU **134.80 dB**；320×180 单帧 CPU 11.5 → Metal **5.6 ms** |
| 真机 M1 · 2.6 MP | 颜色都用 spectral-gpu、含 PNG 读写：质感 CPU **253.6 ms** → Metal **176.6 ms**；质感本身约 **125 → 48 ms（2.6×）** |

### 五、未完成的待办

- [ ] **Mac 2.6 MP 实测**：`test_gpu_texture` 的逐组 dB、`video` 的 ms/帧（现在颜色 + 质感都在 GPU）
- [ ] **线性 / OCIO 交付**、**EXR 压缩**、**代理清理 / 4K 预览**

### 六、复现 / 验收（在 Mac 上）

```bash
# 质感 parity 的逐组 dB
./build/test_gpu_texture

# 融合提交 parity
./build/test_gpu_chain

# 速度：真实素材 2 秒
SPEKTRA_VIDEO_DEBUG=1 ./build/spektra-roll-cli video --engine spectral-gpu \
  --input apple-log --exposure 0.7 --scan negative --contrast 1.15 \
  --couplers on --gamut oklch --grain 0.4 --halation 0.45 --glare 0.2 \
  --diffusion 0.1 --in clip.mov --out out/mac-gpu.mov --duration 2 --codec prores

# 设备与两条链
./build/spektra-roll-cli gpu-info
```

---

## 会话 D056 · macOS Metal 光谱

会话目标：产品负责人指出 macOS 的 GPU 进度落后，要求先做 Metal。macOS 目前只有 CPU 光谱（2.6 MP 约 1.08 s/帧），Windows 已经是 50 ms。

### 一、已完成的事项

**D056 · macOS 颜色走 Metal**
- 新增 `src/cli/gpu_film_metal.mm`（Objective-C++，仅 Apple 编译），实现同一个 `GpuFilm` 接口；CLI / 预览 / UI / 测试的调用方式不变。
- 着色器是 D3D11 版 HLSL 的逐行 MSL 翻译，**运行时 `newLibraryWithSource:` 编译**，没有 `.metallib` 步骤、没有新依赖。
- 常量缓冲用和 C++ 完全相同的标量数组布局；输入/输出/常量都是 `MTLResourceStorageModeShared`，`dispatchThreads` + `waitUntilCompleted` 后直接 memcpy。
- CMake：Apple 上启用 OBJCXX、按平台选源文件、链接 Metal + Foundation、`.mm` 用 ARC。
- 无 Metal 设备时 `available()` 返回 false，自动回退 CPU 光谱。
- **`MTLCreateSystemDefaultDevice()` 返回 nil 时**用 `MTLCopyAllDevices()` 兜底取第一块设备；两者都没有才回退。新增 `spektra-roll-cli gpu-info`（macOS 版本/架构、设备列表、默认设备、两条 GPU 链是否可用）。
- 测试：`gpu_film` 在 macOS CI 上**真跑**（17 个样本 vs CPU 参考）；`gpu_chain` 先查 `GpuTexture::available()`，没有 Metal 质感就 skip。
- `scripts/verify-macos.command` 新增第 5 步：先打 `gpu-info` 和 `test_gpu_film` 的输出，再做 CPU 光谱 vs Metal 光谱出图并打 PSNR。

### 二、一条关键决策（详见 DECISIONS D056）

| 编号 | 决策 | 结论 |
|---|---|---|
| D056 | macOS GPU 颜色 | Metal 移植光谱链；fast math 必须关；质感留到 D057 |

### 三、踩过并修掉的坑（重点）

1. **Metal 默认开 fast math**，编译器按“没有 NaN”优化，把 `density != density` 的保护删掉。光谱表里有几条波长密度是 NaN，于是整帧 NaN：普通路径直接把 NaN 输出，负片路径被 `clamp` 成 0。修法：`MTLCompileOptions.fastMathEnabled = NO`。
2. 这个坑 Windows 复现不了，只能靠 macOS CI 的失败输出定位；`test_gpu_film` 现在失败时会打印逐样本 GPU/CPU 值并显式检查 NaN。
3. 融合提交测试在没有 Metal 质感后端时会 attach 失败；先探测 `GpuTexture::available()` 再决定 skip。
4. **真机 M1 上 `MTLCreateSystemDefaultDevice()` 返回 nil**（Metal 3 明明可用）：默认设备 API 在 macOS 上不是“有 GPU 就给”。加 `MTLCopyAllDevices()` 兜底 + `gpu-info` 诊断；如果仍拿不到，`gpu-info` 会说明是没枚举到、还是枚举到但没选上。

### 四、实测 / 状态

| 项 | 结果 |
|---|---|
| macOS CI（run 80） | `gpu_film` 通过；`gpu_texture` / `gpu_chain` skip；Windows 全绿 |
| Windows 本机 | 9/9 测试通过，GPU 测试仍真跑 |
| 真机 M1（macOS 14.7.5） | `MTLCopyAllDevices: 1 → Apple M1`（默认设备仍 nil）；六组 parity 最差 **2.99e-4**；320×180 CPU vs Metal **115.44 dB**；单帧 10.8 → **7.1 ms**（含 PNG I/O） |
| Mac 2.6 MP 速度 | 2160×1214 单帧（含 16-bit PNG 读写）CPU **424.3 ms** → Metal **129.0 ms（3.3×）** |

### 五、未完成的待办

- [x] ~~**D057：Metal 质感**~~ → 已完成（14 pass 全上 Metal，融合提交在 Mac 重新启用）
- [ ] **Mac 实测**：`verify-macos.command` 跑一遍，记录 Metal 光谱的 ms/帧与 PSNR
- [ ] **线性 / OCIO**、**EXR 压缩**、**代理清理 / 4K 预览**（沿用上一轮）

### 六、复现 / 验收（在 Mac 上）

```bash
# 一键：编译 + 测试 + LUT parity + 视频 + CPU vs Metal PSNR
scripts/verify-macos.command

# 单独量 Metal 光谱
./build/spektra-roll-cli lut apply --engine spectral-gpu --scan negative \
  --couplers on --gamut oklch --in out/frame.png --out out/metal.png
SPEKTRA_VIDEO_DEBUG=1 ./build/spektra-roll-cli video --engine spectral-gpu \
  --input apple-log --exposure 0.7 --scan negative --in clip.mov --out out/mac.mov --duration 2
```

---

## 会话 D055 · 浮点 / EXR

会话目标：D054 之后按清单做 **浮点 / EXR**（D017 欠账）。动手前先确认 CI：run **75**（D054 代码）和 **76**（文档）在 Windows + macOS 都绿。

### 一、已完成的事项

**D055 · OpenEXR 浮点交付**
- **自带最小 OpenEXR**（`image.cpp`）：scanline、NONE 压缩、RGB、FLOAT32 / HALF，读写都是自己拼，不引入 OpenImageIO 或新依赖；头里写 sRGB/Rec.709 + D65 的 `chromaticities`。
- **按扩展名决定格式**：`--out *.exr` 一律写 EXR（默认 float，`--exr-half` 写 half），`--bits` 不参与；`--bits 32` 配 `.png` 会明确报错，不静默降级。
- **EXR 也能读**：`loadImage` 认 `.exr`（NONE、FLOAT/HALF、任意通道顺序），`sourceBits` 报 32；32-bit 源写 PNG 时默认 16-bit。
- **序列**：`lut apply --in-dir ... --out-dir ... --out-ext exr` 把一整段 PNG 转成 float 序列。
- **不钳制**：`convert` 写 EXR 时不再把结果夹到 0..1（PNG 仍夹），所以 `--exposure 3` 的超白能留下来。
- **可见的交代**：写 EXR 时打印 `range: min .. max (x% below 0, y% above 1, written unclipped)`。
- 测试：`test_image` 覆盖 float 精确往返、half 精度、越界值保留、16-bit PNG 对照钳制、32-bit PNG 报错；CI 新增 EXR 冒烟（写 → 读回 → 出 PNG），双平台跑。
- 双击验收：`scripts\demo-exr.cmd`。

### 二、一条关键决策（详见 DECISIONS D055）

| 编号 | 决策 | 结论 |
|---|---|---|
| D055 | 浮点交付 | 自带最小 EXR（不压缩、float/half）；`.exr` 即浮点、不钳制；读回与 PNG16 逐位一致 |

### 三、踩过并记下的坑

1. **16-bit PNG 的 `toShort()` 会夹到 0..1**：浮点交付必须绕过量化，直接写 float 样本，否则 +3 EV 的高光全没了。
2. **EXR 的 chlist 每条是固定 16 字节 + 名字**（pixelType 4、pLinear+保留 4、x/ySampling 各 4），少写一个字段读取器就错位。
3. **通道按 B,G,R 存**是惯例，读取器必须按名字映射，不能假设顺序。
4. **不要静默把 `--bits 32` 降级成 16**：用户要的是浮点，宁可报错让他改扩展名。
5. ffmpeg 的 EXR 解码与我们的 16-bit PNG 对比是 **92.6 dB**，不是 inf——这是独立解码器的舍入差异，不是写坏了；自己读回才是 inf。

### 四、实测（2160×1214）

| 场景 | 结果 |
|---|---|
| 文件体积 | float EXR **30.0 MB** / half **15.0 MB** / 16-bit PNG 15.0 MB |
| 写盘耗时（同一帧，含颜色 + 质感） | EXR 295 ms vs PNG16 293 ms |
| EXR 读回 → PNG 与原 PNG | **PSNR = inf**（逐位相同） |
| ffmpeg 独立解码 EXR → PNG | float **92.6 dB**、half **82.8 dB** |
| `convert --exposure 3` | EXR 范围 **0.0000 .. 5.3438**，**32.973%** 像素 >1 保留；PNG 全切到 1 |

### 五、未完成的待办（按建议顺序）

- [ ] **线性 / OCIO**：EXR 现在是 display-encoded，不是场景线性；需要 `--exr-linear` 或接 OCIO。
- [ ] **EXR 压缩**：NONE 体积大，2.6 MP 30 MB/帧；ZIP/PIZ 能小 3–5 倍。
- [ ] **macOS 的 GPU 路线**：Metal；目前 macOS 只有 CPU 光谱 / LUT。
- [ ] **代理清理 / 4K 预览**：`out/ui-proxy/` 会按素材累积；4K 可考虑 1280 px 代理。

### 六、复现 / 验收

```powershell
scripts\demo-exr.cmd            # PNG16 / float EXR / half EXR + 读回比对 + 超曝光

# 单帧浮点
.\build\spektra-roll-cli.exe lut apply --lut out\m3-applelog-sdr.cube `
  --in out\m3-frame.png --out out\frame.exr --grain 0.4 --halation 0.45

# 序列转 EXR
.\build\spektra-roll-cli.exe lut apply --lut out\identity.cube `
  --in-dir out\seq-in --out-dir out\seq-out --out-ext exr

# 读回
.\build\spektra-roll-cli.exe lut apply --lut out\identity.cube `
  --in out\frame.exr --out out\back.png --bits 16
```

---

## 会话 D054 · 真实点扩散核

会话目标：按建议做 **A（真实点扩散核）**。动手前先跑完健康检查：9/9 测试、M1 parity 88.07 dB、`demo-m3` 220.8 ms/帧、`demo-m4` 续渲正常、Qt 界面能编译、预览第二次 45 ms。

### 一、已完成的事项

**D054 · 光晕 / 眩光 / 扩散换成物理点扩散核**
- 参数从"画幅比例"改成"底片微米"：`--halation-spread 225`、`--glare-spread 725`、`--diffusion-spread 122`（有效 σ，µm），`--frame-width-mm 36` 负责换算；1080p / 4K / 8K 渲染出来是同一个物理大小。
- 核形状：窄芯 + 宽裙的双高斯混合，按有效 σ 归一化（总展宽不变，只改形状）；截断在**最宽分量**的 3σ，3σ 之外的裙部能量是同等方差高斯的 **7–10×**。
- 成本：按核半径自动降采样 1–8×，核按同样比例缩小；结构仍是"降采样 → 分离卷积 → 双线性升采样"（D037 的思路）。
- **CPU / GPU 同一组权重**：核由 `sr::buildSpreadKernel` 在 CPU 生成，GPU 用 structured buffer `t3` 上传，HLSL 只做 `-r..+r` 顺序累加，和 `convolveSeparable` 完全同序。
- `--spread-model box` 保留 D033–D037 的"画幅比例 + 盒状模糊"，两条老路径都没删，旧观感随时可回。
- 预览协议 `texture` 后面新增 `model frameWidthMm` 两个数；渲染签名新增 `spread ...` 一行（换模型/尺度不会混着续渲）。
- 新增双击验收 `scripts\demo-spread.cmd`：同一帧 + 同一段 1 秒视频各渲 box / physical 两遍，打印 PSNR、ms/帧，并排对比。
- 测试：`test_texture` 新增物理尺度、归一化、对称、重尾与 box 回退检查；`test_gpu_texture` 九组配置全过。

### 二、一条关键决策（详见 DECISIONS D054）

| 编号 | 决策 | 结论 |
|---|---|---|
| D054 | 真实点扩散核 | 微米单位 + 双高斯重尾；CPU 生成核权重、GPU 只做卷积；box 模型保留 |

### 三、踩过并修掉的坑（下次别再踩）

1. **HLSL 常量缓冲里的数组是 16 字节/元素**，taps 按 4 字节排进去，GPU 读到的是错位权重，parity 直接掉到 **19–48 dB**；改成 `StructuredBuffer<float> tapsIn : register(t3)` 后 **149–178 dB**。
2. 第一版混合核按"有效 σ"归一化后，3σ 外的裙部反而比同方差高斯**更薄**；把截断改成按最宽分量 3σ，才有真正的重尾。
3. 单元测试里 64 px 的画面在 225 µm 下只有 0.4 px 的扩散，等于没效果；物理核的测试要按底片宽度选画面尺寸（640 px 对应 4 px σ）。

### 四、实测（本机，2160×1214）

| 场景 | 结果 |
|---|---|
| 默认值与旧观感的对应 | 225 / 725 / 122 µm ≈ 13.5 / 43.5 / 7.3 px 有效 σ，强度档位不用改 |
| 同帧 box vs physical | **57.03 dB**（差异集中在高光边缘的柔化与外晕） |
| GPU 视频（光谱 GPU + GPU 质感，ProRes） | **82.2 ms/帧**（D051 时 79.2，+3.8%） |
| LUT + CPU 质感视频 | box 231.8 → physical **251.4 ms/帧**（+8.5%） |
| `demo-m3`（LUT + CPU 质感，H.264） | 220.8 → **235.1 ms/帧**（+6.5%） |
| 单帧 `lut apply`（含 PNG I/O） | box 275.7 → physical **297.6 ms** |
| `demo-gpu-texture` | CPU 236.7 / GPU **82.6 ms/帧（2.87×）**，编码后 PSNR **85.6 dB** |
| GPU parity | `test_gpu_texture` **149.3–177.7 dB**（最差 2.98e-7） |
| 预览 | 第二次 **49 ms**（与之前 45 ms 持平） |

### 五、未完成的待办（按建议顺序）

- [x] ~~**浮点 / EXR**（D017 欠账）~~ → D055 已完成（自带 EXR 读写，float/half，不钳制）
- [ ] **macOS 的 GPU 路线**：Metal；目前 macOS 只有 CPU 光谱 / LUT。
- [ ] **代理清理策略 / 4K 预览**：`out/ui-proxy/` 会按素材累积；4K 素材可考虑 1280 px 代理。
- [ ] **核形状仍是近似**：双高斯是工程近似，没有按胶片层厚/折射率算；要更真需要实测的 halation/glare PSF。

### 六、复现 / 验收

```powershell
scripts\demo-spread.cmd            # 左旧 / 右新，打印 PSNR 与 ms/帧

# 同一帧两种模型
.\build\spektra-roll-cli.exe lut apply --lut out\m3-applelog-sdr.cube `
  --in out\m3-frame.png --out out\spread-physical.png `
  --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1
.\build\spektra-roll-cli.exe lut apply ... --spread-model box --out out\spread-box.png

# 自定义物理尺度（微米）
.\build\spektra-roll-cli.exe video --lut out\m3-applelog-sdr.cube --in X:\agent\footage\A001_11011538_C001.mov `
  --out out\spread-custom.mov --halation 0.45 --glare 0.2 --diffusion 0.1 `
  --halation-spread 120 --glare-spread 500 --diffusion-spread 80 --frame-width-mm 24.9
```

---

## 会话 D049–D053 · 融合提交 → 管道 → 并行 → 预览代理

会话目标：产品负责人依次选了 **B（融合提交）→ B2（视频管道）→ 并行分段 → 预览代理**。下面按交付编号整理，所有数字都是本机（i5-10600KF / Arc A750 / 32 GB）实测。

### 一、已完成的事项

**D049 · 颜色 + 质感合并成一次 GPU 提交**
- `GpuFilm::attach()` / `GpuTexture::attach()`：两个后端共用一个 D3D11 设备（所有权仍在 film）。
- `GpuFilm::processToDevice()` + `readback()`：中间结果不回 CPU。
- `GpuTexture::processBuffer()`：对 film 输出 buffer 建 SRV，GPU 内拷贝进 ping-pong 缓冲，只回读一次。
- `tests/test_gpu_chain.cpp`：融合 vs CPU 参考 **124.1 dB**，并检查确定性。
- 视频 **233.6 → 207.1 ms/帧**，与分段路径输出 **逐位相同**；预览 43 → 38 ms。

**D050 · 视频管道修复**
- 分段计时发现 **130 ms/帧** 花在 `float → uint16` 的 `std::lround`；改成乘加并并行两个整帧循环。
- 视频 **207.1 → 85–92 ms/帧**；`demo-m3` 358.7 → 245.5；输出逐位相同。
- 硬件解码测量后**不采用**：QSV+`hwdownload` 21.8 ms/帧 vs 软解 19.5 ms/帧；d3d11va 23 ms/帧。
- 编码器成本终于可见：ProRes 85、H.264+QSV **77**、H.264 软编 104、HEVC 软编 104 ms/帧。

**D051 · 解码/编码与处理重叠**
- `FramePipeReader`（双缓冲）+ `FramePipeWriter`（两槽位），读写线程在 `closePipe` 前 join。
- 视频 **85–92 → 79.2 ms/帧**；`demo-m3` 226.8；相对 D046 的 379 累计 **4.8×**；输出逐位相同。

**D052 · 并行分段**
- `--jobs N`（默认 `0` = `min(3, 核数/4, 段数)`；`1` 回到顺序）；每 worker 独立 `GpuFilm`/`GpuTexture`/颗粒缓存，原子计数器领段。
- 30 秒素材 / 6 段 × 5 秒 / ProRes：**LUT + CPU 质感 157.98 → 84.19 s（1.88×）**；光谱 GPU 53.17 → 48.42 s（1.10×，单卡串行）。
- 输出与顺序渲染 **逐位相同**；并行续渲正常（9 段删 1 补 1）。

**D053 · 长片预览代理**
- 整段进内存会把最长边压到 960 px 以下时，生成 960 px 全 I 帧代理（`out/ui-proxy/<hash>.mov`），`frame N` 按需 `-ss` 取帧；失败回退内存缓存。
- 30 秒素材：代理 **8.7 s / 30.6 MB**，预览 **960×540 · 100 ms/帧**（原先约 640 px）；重开 **48 ms**。
- 4 秒素材不变：内存缓存 1600×898 · 47 ms。
- `frame` 回复带实际宽高；`open` 的 total = 可预览总帧数。

### 二、五条关键决策（详见 DECISIONS D049–D053）

| 编号 | 决策 | 结论 |
|---|---|---|
| D049 | 融合提交 | 两个 GPU 后端共设备、一次上传/回读；失败回退分段 |
| D050 | 管道修复 | 去掉 `std::lround`；**不**做硬件解码（实测更慢） |
| D051 | 读写重叠 | 解码双缓冲 + 编码后台线程，不再阻塞 GPU |
| D052 | 并行分段 | 默认最多 3 个 worker；每个 worker 独立 GPU 对象 |
| D053 | 预览代理 | 只对“装不下/不够清晰”的长片生成代理，短片仍走内存 |

### 三、踩过并修掉的坑（下次别再踩）

1. film 输出 buffer 少了 `BIND_SHADER_RESOURCE`，质感建不了 SRV。
2. film dispatch 后没解绑 UAV，质感读同一 buffer 得到**全黑帧**。
3. `attach()` 后 `initialize()` 误判“已初始化”，着色器没编译 → 访问冲突。
4. 非 Windows 构建里 `#endif` 放进了函数体 → macOS 占位实现括号错位。
5. `std::filesystem::file_time_type::count()` 在 libc++ 上比 `long long` 宽，`std::to_string` 重载歧义 → 只有 macOS 编译失败，需 `static_cast<long long>`。
6. CI 的 macOS 失败先看 job 耗时：1–2 分钟才是真跑；“not acquired by Runner” / “Internal server error” 是 GitHub 的问题，空提交重跑即可。
7. workflow 现在会在构建失败时把 `error:` 行打成 annotation，直接在 run 页面可见。

### 四、未完成的待办（按建议顺序）

- [x] ~~**真实点扩散核**~~ → D054 已完成（微米 + 双高斯重尾核，box 模型保留对照）
- [x] ~~**浮点 / EXR**（D017 欠账）~~ → D055 已完成（自带 EXR，float/half，不钳制）
- [ ] **macOS 的 GPU 路线**：Metal；目前 macOS 只有 CPU 光谱 / LUT。
- [ ] **代理清理策略**：`out/ui-proxy/` 会按素材累积，可加 LRU 或“删除代理”按钮。
- [ ] **4K / 更长片**：预览代理固定 960 px，4K 素材可考虑 1280；分段并行的显存上限需要在更小显存的机器上验证。

### 五、复现 / 验收

```powershell
# 单帧/视频（应约 79 ms/帧，ProRes）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input apple-log --exposure 0.7 `
  --scan negative --contrast 1.15 --couplers on --gamut oklch --grain 0.4 --halation 0.45 `
  --glare 0.2 --diffusion 0.1 --in X:\agent\footage\A001_11011538_C001.mov `
  --duration 2 --codec prores --out out\check.mov

$env:SPEKTRA_VIDEO_DEBUG="1"   # 再跑一次，打印 wait/convert/process/pack

.\build\test_gpu_chain.exe                      # 融合 vs CPU，应 ≥60 dB（124 dB）
.\build-ui\spektra-roll-ui.exe --preview-selftest   # 第二次预览应 <100 ms
```

界面：`scripts\build-ui.cmd` → 拖时间线（短片 47 ms；长片自动建 960 px 代理，100 ms）。

---

## 会话 D048 · 预览实时化

会话目标：按建议先做 **A（预览实时化）**。先把旧预览链拆开量，再决定改法。

### 旧链的时间都花在哪（2.6 MP）

| 环节 | 耗时 |
|---|---|
| ffmpeg 抽 1 帧 → 16-bit PNG | ~1000 ms |
| CLI 启动 + 解析 `.cube`（35,937 行） | ~215 ms |
| 16-bit PNG 读 + 写 | ~600–700 ms（写 ~250 ms） |
| D3D11 设备 + 着色器编译（每次进程） | 150–700 ms |
| 真正的颜色 + 质感 | ~50 ms |

结论：不是算得慢，是**每次预览都在重建整个世界**。

### 已完成

1. **`spektra-roll-cli preview` 常驻进程**（D048）：行协议 `look / open / backend / texture / frame / raw / quit`。
2. **整段帧缓存**：`open` 一次性解码进内存（默认 1024 MB、最长边 1600 px），`frame N` 只做颜色 + 质感 + 8-bit RGBA 打包；换 look 不丢缓存。
3. **去掉了 PNG 往返**：预览输出 8-bit RGBA 原始帧，UI 用 `QImage::Format_RGBA8888` 直接接管。
4. **拖动合并**：时间线拖动每 90 ms 出一帧，松手补最后一帧；超缓存的帧在拖动时跳过，不触发慢路径。
5. **`ffprobe` 提速**：`probeGeometry` 改成一次 `-show_streams`；原来单独查 `side_data=rotation` 在这支素材上要 **3.8 s**，合并后 **42 ms**。
6. **旧路径保留为回退**：服务启动失败 / 超出缓存 / 无 ffmpeg 时仍然能出图。
7. **`--preview-selftest`**：启动界面后跑两次真实预览并打印状态行，脚本可验收。
8. **CI**：新增预览服务端协议冒烟（`look` / `open` / `frame` / `raw` + 输出尺寸），run **57** Windows + macOS 双绿（顺手修了 BSD `wc -c` 补齐空格导致 macOS 断言失败）。

### 实测

| 场景 | 旧 | 新 |
|---|---|---|
| LUT 预览（1600 × 898） | 1.7–2.2 s/帧 | **47–64 ms/帧** |
| 光谱 GPU 预览 | ~2.2 s/帧 | **43 ms/帧** |
| 首次打开素材 | — | 2–4 s（解码整段，一次性，取决于磁盘） |
| 常驻内存 | — | 99 帧 × 8.6 MB ≈ **853 MB** |

### 关键决策（详见 D048）

- 预览是**降分辨率 + 8-bit**，但颜色/质感仍走同一套 C++ 管线；状态栏写明尺寸与耗时。
- 帧缓存放在 CLI 进程里，UI 不碰解码细节；换 look 只重载 LUT/模型。
- 长片超出预算时回退单帧路径并明确提示，不做静默降级。
- `preview` 是内部命令，用户入口仍然是 UI 的预览按钮和 Ctrl+P。

### 未完成 / 下一步

- [ ] **颜色 + 质感合并成一次 GPU 提交**：视频仍是 245 ms/帧，空管道约 207 ms。
- [ ] **预览缓存放到磁盘 / 代理文件**：现在只有内存缓存，长片超出后回退单帧；可生成半分辨率代理。
- [x] ~~**真实点扩散核**~~ → D054 已完成（微米 + 双高斯重尾核）。
- [ ] **CI 上的真实 GPU 覆盖**：`gpu_film` / `gpu_texture` 在公有 runner 上会 skip。
- [ ] **macOS 的 GPU 路线**：Metal 或继续 CPU。
- [x] ~~浮点 / EXR（D017 欠账）~~ → D055 已完成。

### 复现命令

```powershell
# 预览服务端协议（CI 也跑这一段）
printf 'look --lut out/m3-applelog-sdr.cube\nopen "X:\agent\footage\A001_11011538_C001.mov"\nframe 40 "out/p.rgba"\nquit\n' |
  .\build\spektra-roll-cli.exe preview --ffmpeg J:\ffmpeg\bin\ffmpeg.exe --in-matrix bt2020

# 界面里跑两次真实预览并打印耗时（无需手点）
.\build-ui\spektra-roll-ui.exe --preview-selftest

# 打开调试计时
$env:SPEKTRA_PREVIEW_DEBUG="1"; .\build-ui\spektra-roll-ui.exe --preview-selftest
```

界面验证：`scripts\build-ui.cmd` → 拖时间线，状态栏应显示 `47–64 ms · 整段缓存`。

---

## 会话 D047 · 空间质感上 GPU

会话目标：D046 之后颜色只剩约 50 ms/帧，视频却仍要约 380 ms/帧。先把时间花在哪量清楚，再决定要不要移植。

### 先量了一下（2.6 MP 单帧，`lut apply`，质感逐项开启）

| 配置 | 耗时 | 增量 |
|---|---|---|
| 只套 LUT | 14.7 ms | — |
| + 光晕 | 64.1 ms | +49 |
| + 眩光 | 57.3 ms | +43 |
| + 扩散 | 65.9 ms | +51 |
| + 颗粒（无缓存，两张场） | 138.6 ms | +124 |
| 全部 | 295.5 ms | +281 |

视频里颗粒有 `GrainFieldCache`，每帧约 1/3 个场，所以质感约 **160 ms/帧**；实测 GPU 颜色视频（无质感）207 ms/帧、全质感 366 ms/帧，与上表吻合。

### 已完成

1. **`sr::applyTexture()` 收敛成唯一 CPU 参考链**（`texture.hpp/texture.cpp`）：halation → glare → diffusion → grain，CLI 和 GPU parity 测试都调它，避免两处顺序漂移。
2. **`GpuTexture`（D047，`--texture-engine`）**：11 个 D3D11 compute pass，逐行翻译 CPU 实现。
   - 盒状模糊一行/一列一个线程，**浮点累加顺序与 CPU 完全一致**；
   - 宽模糊复用 CPU 的降采样 → 2 段模糊 → 双线性升采样（眩光 4×，光晕/扩散 2×），小画幅仍走全分辨率 3 段；
   - 颗粒用同一套 uint hash 在 GPU 上重建场，`--grain-rate` 的两场交叉淡化与归一化不变；
   - 无设备/编译失败/运行时错误 → stderr 一次 warning，整条序列退回 CPU。
3. **一致性**：`tests/test_gpu_texture.cpp` 九组配置（单项、raw noise、rate 1/3、demo-m3 全开、小画幅全分辨率分支）与 CPU 逐像素对比 **143–166 dB，最差 4.5e-7**；真实负片帧 **123.1 dB**。无显卡时 exit 77，ctest 记 skipped。
4. **实测**：
   - 视频全质感 **382.7 → 245.6 ms/帧（1.56×）**；两段 2 秒 demo **386.8 → 233.6 ms/帧（1.66×）**；
   - 质感本身 **约 160 → 约 38 ms/帧**；
   - 单帧 `lut apply`（含着色器编译）309.5 → 214.5 ms；
   - 两段编码后视频 PSNR 83.6 dB（差异来自编码器）。
5. **UI**：输出栏新增“GPU 质感（颗粒 / 光晕 / 眩光 / 扩散）”勾选框，默认开、随设置保存；预览、渲染、队列走同一条 `--texture-engine gpu`。
6. **双击验收**：`scripts\demo-gpu-texture.cmd` 用同一条命令渲两遍（CPU / GPU），打印两条 ms/帧、加速比和编码后 PSNR。
7. **CI**：新增 GPU 质感回退冒烟（`--texture-engine gpu` 在无显卡 runner 上必须自动退回 CPU），run **54** Windows + macOS 双绿。
8. 文档同步：`DECISIONS.md` D047、本文件、`STATUS.md`、`README.md`、`RUN.md`、CLI `--help`。

### 关键决策（详见 D047）

- `--texture-engine auto|cpu|gpu`：`auto` 只在 `--engine spectral-gpu` 时用 GPU 质感；`gpu` 也可配 LUT；**永不硬失败**。
- 空间质感仍然**不是** LUT 的一部分，顺序仍是 halation → glare → diffusion → grain。
- 盒状模糊的线程划分为了 **逐位对齐 CPU**，刻意不改成并行前缀和（换了顺序就过不了 parity）。
- GPU 质感不改变 `--grain-rate` 默认 3，也不改任何观感参数。

### 未完成 / 下一步

- [ ] **颜色 + 质感合并成一次 GPU 提交**：现在两段各自上传/回读 42 MB，空管道仍约 207 ms/帧；合并后预计再省 15–30 ms/帧，并消掉一次 16-bit 往返。
- [ ] **预览实时化**：GPU 颜色已够快，但 UI 预览仍是 “ffmpeg 抽帧 → 写 PNG → CLI 进程 → 读回”，整链 0.3–0.5 s；可做常驻进程或低分辨率直出。
- [x] ~~**真实点扩散核**~~ → D054 已完成（微米 + 双高斯重尾核，GPU 与 CPU 同一组权重）。
- [ ] **CI 上的真实 GPU 覆盖**：`gpu_film` 和 `gpu_texture` 在公有 runner 上会 skip。
- [ ] **macOS 的 GPU 路线**：Metal 或继续 CPU（当前选择）。
- [ ] **队列多任务/多段并行**：GPU 已释放 CPU。
- [x] ~~浮点 / EXR（D017 欠账）~~ → D055 已完成。

### 复现命令

```powershell
# 质感在 GPU 上（配合光谱 GPU 时 auto 自动开启）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input apple-log --exposure 0.7 `
  --scan negative --contrast 1.15 --couplers on --gamut oklch `
  --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1 `
  --in X:\agent\footage\A001_11011538_C001.mov --out out\gpu.mov --frames 24 --codec prores

# 强制 CPU 参考链做对比
.\build\spektra-roll-cli.exe video ... --texture-engine cpu

# parity 测试（无显卡时自动 skip）
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir build -R gpu_texture --output-on-failure
```

界面验证：`scripts\build-ui.cmd` → 勾上/取消“GPU 质感” → 预览一帧 / 渲染整段，比较速度。

---

## 会话 D044–D046 · 颜色改成物理计算并上 GPU

会话目标：按你的要求，**能物理算的地方不要再靠 LUT 采样**，并且用满本机（i5-10600KF / 32 GB / Arc A750）。

### 已完成

1. **可选 CPU 光谱引擎**（D044，`--engine spectral`）：每个像素直接跑 `FilmModel::transformFrom`，负片/印片/耦合剂/OkLCh/HDR/Apple Log/曝光/反差全部进物理链。2.6 MP 单帧 1081 ms。
2. **D3D11 探针**（D045，`tools/gpu-probe/`）：Arc A750 上 81 波长扫描 0.63 ms，确认值得移植。
3. **GPU 光谱引擎**（D046，`--engine spectral-gpu`）：HLSL 逐行翻译 CPU 参考链，常量缓冲 22.9 KB（曲线 / 密度 / CMF / 光源 / 矩阵 / 标量），structured buffer 进出，读回解包。
   - 2.6 MP 单帧 **1038–1158 ms → 47–50 ms**（约 21–23×，含 42 MB 上传 + 42 MB 回读）
   - 2.6 MP 视频（ProRes 422 HQ）**1427 → 379 ms/帧**（约 3.8×；瓶颈现在是纹理叠加与编码）
4. **一致性验证**：与 CPU 光谱逐像素比对，负片 119.1 dB、印片 inf（逐位相同）、PQ 82.9 dB、HLG 90.1 dB、`--strength 0.6` 121.3 dB、视频 86.1 dB。
5. **`tests/test_gpu_film.cpp`**：六组配置（sRGB/Apple Log、印片/负片、耦合剂、PQ、strength 混合）与 CPU 参考比对，最差偏差 3e-4；无 D3D11 设备时 exit 77，ctest 记为 skipped。
6. **强制回退**：无设备 / 着色器编译失败 / 运行时出错 → stderr 打 warning，自动退回 CPU 光谱（实测回退输出与 CPU 逐位相同）。
7. **UI 接入**：引擎下拉新增“光谱 GPU（物理，快）”，预览、批量渲染、队列、分段/续渲/时间码都走同一条 CLI；队列恢复能识别新引擎值。
8. **CI**：新增“物理引擎冒烟”（`spectral` + `spectral-gpu` 各跑一张 16-bit 小图，无显卡自动回退）；修正非 Windows 下的占位实现后 **Windows + macOS 双绿**（run 41440266503）。
9. 文档同步：`DECISIONS.md` D046、`README.md`、`STATUS.md`、本文件。

### 关键决策（详见 D044–D046）

- 默认仍然是 **LUT**；物理引擎是可选路径，界面上明确标注“慢（CPU）/ 快（GPU）”。
- **CPU 光谱是唯一参考**，GPU 必须先过 ≥60 dB 才允许接入（实际 82–119 dB）。
- GPU 只做**颜色**；颗粒/光晕/眩光/扩散仍在 CPU 按原顺序叠加。
- GPU 路径 **Windows-only（D3D11）**，其它平台/失败一律回退 CPU，不允许硬失败。
- 每个会话结束时更新 progress/STATUS/DECISIONS，且交付物必须能双击验收。

### 未完成 / 下一步

- [ ] **把空间质感搬到 GPU**：379 ms/帧里颜色只占约 50 ms，剩下是颗粒与模糊；这是目前最大的单点收益。
- [ ] **预览实时化**：GPU 后颜色已够快，但 UI 预览仍走 “ffmpeg 抽帧 → 写 PNG → CLI → 读回”，整链约 0.3–0.5 s；可做低分辨率直出/常驻进程来接近拖动实时。
- [ ] **CI 上的真实 GPU 覆盖**：公有 runner 一般没有可用 D3D11 设备，`gpu_film` 会被 skip；需要在有 GPU 的机器上跑一次（或加 WARP 检查）才能保证着色器编译不退化。
- [ ] **macOS 的 GPU 路线**：Metal 或继续 CPU 光谱（当前选择），若要 macOS 也快需要单独一层。
- [ ] 队列**多任务/多段并行**：GPU 已释放 CPU，可以并行渲染多个任务。
- [x] ~~真实点扩散核~~ → D054 已完成（微米 + 重尾核）。

### 复现命令

```powershell
# 单帧 GPU 光谱（约 50 ms）
.\build\spektra-roll-cli.exe lut apply --engine spectral-gpu --input apple-log --scan negative `
  --couplers on --gamut oklch --exposure 0.7 --in out\m3-frame.png --out out\gpu.png

# 视频（约 379 ms/帧，ProRes）
.\build\spektra-roll-cli.exe video --engine spectral-gpu --input apple-log --exposure 0.7 `
  --scan negative --contrast 1.15 --couplers on --gamut oklch `
  --in X:\agent\footage\A001_11011538_C001.mov --out out\gpu.mov --frames 3 --codec prores

# 一致性测试（无显卡时自动 skip）
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir build --output-on-failure
```

界面验证：`scripts\build-ui.cmd` → 引擎选“光谱 GPU（物理，快）”→ 预览一帧 / 渲染整段。

---

## M3 阶段（早期）

一次会话把项目从"M2 尾声"推到"M3 完成"，按时间顺序：

1. 修好 HDR 视频：原来 demo 是单帧循环 4 秒，改成 **99 帧整段 + 原声**
2. **M3 视频读写**（D022）：`spektra-roll-cli video` 直接吃 `.mov`，ffmpeg 解码 → 逐帧套 LUT → 重新编码，音画同步
3. **帧区间 + ProRes**（D031）：`--start/--frames/--duration`、`--codec prores`（422 HQ）
4. **macOS 修复**（issue #1）：POSIX `popen` 不接受 `"rb"/"wb"`，已修，CI 两个平台都跑视频冒烟
5. **多线程**（D032）：套 LUT 96 → 17 ms
6. **胶片质感**：颗粒 + 光晕（D033）→ 调轻（D034）→ 眩光 + 扩散（D035）→ 颗粒时间连续（D036）
7. **速度**：宽模糊降采样（D037）、`--crf/--preset`、`--encoder qsv`（D038）
8. **颗粒场缓存**（D039）：一个噪声场跨帧复用，全质感 **476.5 → 358.7 ms/帧**，整段 47.2 → 35.5 秒
9. **分段渲染 + 断点续渲 + 时间码**（D040）：`--segment` / `--resume`，中断后只补缺失段；输出保留相机 timecode
10. **Qt 6 桌面界面首版**（D009）：选素材 → 烘 LUT → 调质感 → 预览一帧 → 渲染整段；后续加上时间线、原片对比、拖放和**渲染队列**

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
| 颗粒场缓存（`GrainFieldCache`，LRU 两个场） | ✅ 476.5→**358.7 ms/帧**，整段 47.2→35.5 s |
| macOS `popen` 修复（issue #1） | ✅ CI 两平台都跑视频冒烟 |
| `scripts\demo-m3.cmd`（干净帧 / 质感帧 / 整段视频） | ✅ |

### 6. M4 分段渲染 + 断点续渲 + 时间码（已完成）

| 项 | 状态 |
|---|---|
| `--segment SEC`：拆成 `<out>.parts/part-NNNNN.mov` | ✅ 每段只写视频，边界用全片帧号保持颗粒连续 |
| `--resume`：跳过已完成的段 | ✅ 删掉第 4 段后重跑，只渲染 5 s 的 1 段 |
| 音频只从源文件复制一次 | ✅ 分段不再逐段编码 AAC，无接缝 |
| `--keep-parts` | ✅ 保留分段便于检查 |
| 时间码：读源 `timecode`，`--start` 时按帧偏移 | ✅ 15:38:35:00 → `--start 1` 得 15:38:36:00 |
| `--timecode` 覆盖 / `--no-timecode` 关闭 | ✅ |
| 分段 vs 整段逐帧对比 | ✅ PSNR 约 46 dB，边界无跳变 |
| CI 两平台跑分段 + 时间码冒烟 | ✅ Windows + macOS 绿（run 37） |
| `scripts\demo-m4.cmd`（渲染 → 模拟崩溃 → 续渲） | ✅ |

### 7. 改名与上线

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

### 8. 实测数据（本机，真实跑出来的）

| 项 | 结果 |
|---|---|
| CPU | Intel i5-10600KF |
| GPU | Intel Arc A750（支持 D3D12 / Vulkan） |
| 内存 | 32 GB |
| 编译器 | MinGW-w64 GCC 16.2.0（便携版，无管理员权限） |
| CMake | 4.4.4（`C:\Program Files\CMake\bin\cmake.exe`） |
| Ninja | 1.13.2（`X:\agent\tools\ninja\ninja.exe`） |
| 单元测试 | **9/9** 程序通过（LUT、胶片模型、图像 I/O、Apple Log/传输、色域、质感、GPU 颜色 / 质感 / 融合 parity） |
| 33³ LUT 烘焙 | **约 108 ms**（多线程，含写文件）；单线程 198 ms |
| 2.6 MP 套 33³ LUT（16-bit） | **约 17 ms**（多线程）；单线程 96 ms |
| 视频渲染（2.6 MP，HEVC） | 无质感 **187 ms/帧**；全质感 **358.7 ms/帧**（场缓存前 476.5）；4.1 s 素材约 35.5 秒 |
| 光谱引擎（2.6 MP，12 线程） | 单帧 **1081 ms**；视频 **1258.9 ms/帧**；99 帧约 2 分钟 |
| 光谱 vs 33³ LUT（同一帧） | PSNR **62.19 dB**（R/G/B 63.5 / 62.9 / 60.7），LUT 采样误差很小 |
| GPU 扫描探针（Arc A750，D3D11） | 2.6 MP × 81 样本 **0.63 ms**（CPU 光谱整帧 1081 ms） |
| **GPU 光谱引擎（2.6 MP 单帧）** | **47–50 ms**（含 42 MB 上传 + 42 MB 回读）；CPU 光谱 1038–1158 ms |
| **GPU 光谱引擎（2.6 MP 视频）** | **379 ms/帧**（ProRes 422 HQ，含编码与管道）；CPU 光谱 1427 ms/帧 |
| **GPU 质感（2.6 MP 视频，全质感）** | **245.6 ms/帧**（CPU 质感 382.7）；`demo-gpu-texture` 233.6 vs 386.8（1.66×） |
| **GPU 质感本身** | 约 **38 ms/帧**（CPU 约 160）；单帧 `lut apply` 309.5 → 214.5 ms |
| **GPU 质感 vs CPU 参考** | 九组配置 **143–166 dB**（最差 4.5e-7）；真实负片帧 **123.1 dB** |
| **UI 预览（D048，1600×898）** | **47–64 ms/帧**（旧链 1.7–2.2 s，约 30×）；光谱 GPU 43 ms；首次打开解码 2–4 s |
| **融合提交（D049）** | 视频全质感 **207.1 ms/帧**（分段 233.6）；与分段输出 **逐位相同**；预览光谱 GPU **38 ms/帧** |
| **管道修复（D050）** | 视频 **207.1 → 85–92 ms/帧**（2.4×）；输出逐位相同；`pack` 从 130 → 5 ms |
| **读写重叠（D051）** | 视频 **85–92 → 79.2 ms/帧**；`demo-m3` 245.5 → 226.8；相对 D046 累计 **4.8×**；输出逐位相同 |
| **并行分段（D052）** | CPU 路径 **157.98 → 84.19 s（1.88×）**；GPU 路径 1.10×（单卡）；输出逐位相同；并行续渲正常 |
| **长片预览代理（D053）** | 30 秒素材代理 8.7 s / 30.6 MB；预览 **960×540 · 100 ms/帧**（原先约 640 px）；重开 48 ms；4 秒素材不变 |
| **物理点扩散核（D054）** | 同帧 box vs physical **57.03 dB**；GPU 视频 **82.2 ms/帧**（+3.8%）；LUT+CPU 质感 231.8 → 251.4 ms/帧；`demo-gpu-texture` CPU 236.7 / GPU **82.6（2.87×）**，编码后 85.6 dB；parity 149–178 dB |
| **浮点 EXR（D055）** | 2.6 MP：float **30.0 MB** / half **15.0 MB** / PNG16 15.0 MB；读写耗时与 PNG 相当；读回 **PSNR = inf**；ffmpeg 独立解码 92.6 dB（half 82.8）；+3 EV 时 **32.97%** 像素 >1（max 5.34）保留 |
| **Metal 光谱（D056）** | macOS CI run 80 双绿；真机 M1（macOS 14.7.5）：`MTLCopyAllDevices` 找到 Apple M1、默认设备仍 nil；六组 parity 最差 **2.99e-4**，320×180 CPU vs Metal **115.44 dB**；2.6 MP 单帧 **424.3 → 129.0 ms（3.3×）**（含 PNG I/O） |
| **Metal 质感（D057）** | macOS CI run 85 **9/9 全过**：`gpu_texture` 2.11 s、`gpu_chain` 2.10 s；真机 M1：九组 **逐位相同**（worst 0.0），融合 **134.80 dB**；2.6 MP 质感 **253.6 → 176.6 ms**（含颜色与 I/O，质感本身约 125 → 48 ms） |
| **线性 EXR（D058）** | 同帧 display 0.0160..0.8870 → linear **0.0012..0.7619**；+3 EV 线性最大 **50.3**；体积/耗时与普通 EXR 相同 |
| **EXR ZIP（D059）** | 已调色帧 31.49 → **22.73 MB（−28%）**；带颗粒 ≈0%；half **9.82 MB（−38%）**；合成图表 −98.6%；ffmpeg 解码 92.6 dB |
| **预览帧缓存** | 99 帧 × 8.6 MB ≈ **853 MB**（默认上限 1024 MB） |
| **GPU vs CPU 光谱 PSNR** | 负片 119.1 dB、印片 inf、PQ 82.9 dB、HLG 90.1 dB、strength 0.6 时 121.3 dB、视频 86.1 dB |
| 颗粒场缓存 | 每 3 帧只生成 1 个新场（原来每帧 2 个）；占用约 21 MB |
| 分段渲染 | 4 s 素材 / 1 s 段 = 5 段；合并后 99 帧 / 4.125 s / 原声 / timecode 15:38:35:00 |
| 断点续渲 | 删掉第 4 段后重跑：3 段跳过，只渲 1 段（约 5 s，整段约 35 s） |
| 分段 vs 整段 | 逐帧 PSNR 约 46 dB（差异来自强制关键帧），边界无跳变 |
| Qt UI 预览 | 2160×1214 @ 1.5 s，抽取 + 套 LUT + 质感约 1 s |
| Qt UI 整段渲染 | 99 帧 / 329.2 ms/帧 / 32.6 s（与 CLI 同一路径） |
| Qt UI 渲染队列 | 2 条任务顺序完成（48 帧 00:21、12 帧 00:08）；进度条与 ETA 正常；重启后队列恢复 |
| 颗粒时间稳定性 | 帧间差 0.0025（rate 1）→ **0.0014**（rate 3） |
| C++ vs Python 参考 | **PSNR 88.07 dB**，max \\|diff\\| 4.57e-4，ΔE76 max 0.0065 |
| Apple Log 解码 vs colour-science | 2.6 MP 帧，max \\|diff\\| **1.53e-5**（= 16-bit 1 LSB），mean 1.9e-8 |
| Apple Log 素材 | 2160x1214，24p，HEVC 10-bit 4:2:2，BT.2020 |
| 扫描负片 vs 印片（同一帧） | 天空 0.48–0.70 / 0.88–0.91；暗部 <0.10 由 7.7% 降到 2.5% |
| 负片反差 1.0 / 1.15 / 1.3 | 天空 p5–p95 0.48–0.70 / 0.49–0.75 / 0.49–0.79；暗部 2.5% / 6.8% / 13.1% |
| 印片曝光因子 | 0.837773013976（上游含一次 sRGB→sRGB 往返，0.837787256300） |
| 产物 | `build\spektra-roll-cli.exe`，静态链接，无 DLL 依赖 |

**重要推论**：套 LUT 已经压到 17 ms/2.6 MP；颗粒场缓存把质感从 290 ms 降到约 172 ms。现在整帧的剩余开销主要落在光晕/眩光/扩散三个宽模糊和管道/格式转换上，编码器仍不是瓶颈。

### 9. M5 Qt 6 桌面界面（首版，D009）

| 项 | 状态 |
|---|---|
| Qt 6.8.3 + MinGW 13.1 便携安装（`X:\agent\tools\qt`，不进仓库） | ✅ `aqtinstall` 安装 |
| `spektra-roll-ui` 目标（`-DSR_BUILD_QT_UI=ON`） | ✅ 与 CLI 共用 `sr_core` / 同一套命令 |
| 素材、LUT、曝光、负片/印片、反差、SDR/HDR 选择 | ✅ |
| 颗粒 / 光晕 / 眩光 / 扩散 / 颗粒稳定度 | ✅ |
| **预览一帧**（ffmpeg 抽帧 → 写 PNG → `lut apply` → 窗口显示） | ✅ 2160×1214 @ 1.5 s 实测（**已被 D048 取代**） |
| **预览常驻进程**（D048）：整段解码进内存，`frame` 只跑颜色 + 质感 | ✅ **47–64 ms/帧**（旧链 1.7–2.2 s）；首次打开 2–4 s |
| **时间线拖动**：拖到任意帧，松开刷新预览；显示 `mm:ss / 总长 · 帧 n/N` | ✅ |
| **素材信息条**：分辨率、帧率、时长、帧数、相机 timecode | ✅ 自动 ffprobe，含旋转 |
| **按住看原片**：按住显示未套胶片的解码画面，松开回到成片 | ✅ |
| **拖放素材**到窗口即可载入 | ✅ |
| 快捷键 `Ctrl+P` 预览 / `Ctrl+R` 渲染 / `Ctrl+B` 烘焙 | ✅ |
| **输出文件夹**按钮 | ✅ |
| **烘焙 LUT**（Apple Log/SDR，HDR 自动带 PQ/HLG） | ✅ |
| **渲染整段**（编码、起始、帧数、分段、续渲） | ✅ 日志、进度条、取消 |
| **渲染队列**：冻结参数入队、自动避免重名输出、逐条顺序渲染 | ✅ 每条显示 等待/渲染中/完成/失败/已取消 |
| 队列操作：开始、停止、移除、清空已完成、打开输出 | ✅ 停止后未完成任务保留为“等待” |
| 每行**进度条 + 用时/剩余时间** | ✅ 每秒刷新，按已完成帧数估算 ETA |
| 队列中每条任务使用 **LUT 快照**，之后改参数不会影响已入队任务 | ✅ `out/ui-queue/lut-*.cube` |
| **队列持久化**：`out/ui-queue/queue.json`，重启恢复；中断任务回到“等待” | ✅ 实测重启后 2 条任务恢复 |
| **多文件拖入 / 批量加入**：一次多个视频，按素材名自动生成任务 | ✅ |
| **界面 v2**（D041）：三栏 + 顶栏、SpektraLab 灰/琥珀令牌、Section + SliderRow | ✅ 截图更新 |
| 参数栏改成“标签 + 数值 + 细滑块”，队列切换时右栏自动加宽 | ✅ |
| **直方图叠加**（画布右上角 RGB） | ✅ |
| **适合 / 100% 缩放 + 滚轮缩放 + 拖拽平移**（Ctrl+0/1、双击） | ✅ |
| **冲印流程分组**（D042）：显影前（曝光/底片/DIR/颗粒/光晕）与显影后（相纸/扫描/反差/色域/眩光/扩散） | ✅ |
| 新增 `--couplers` / `--gamut` 开关并写进 LUT 配方签名 | ✅ |
| **胶片 / 相纸选择**（D043）：11 支彩色负片 × 6 种相纸，自动选 target_print | ✅ 实测 C200 + Crystal、Portra 800 + Endura |
| **可选物理光谱引擎**（D044）：`--engine spectral`，逐像素跑物理链，不查 LUT | ✅ 2.6 MP 单帧 1081 ms；视频 1259 ms/帧 |
| UI 输出栏“引擎：LUT / 光谱”，队列/分段两种都支持 | ✅ |
| **GPU 光谱探针**（D045）：D3D11 compute，2.6 MP × 81 样本 | ✅ **0.63 ms**（Arc A750，feature level 11_0） |
| **GPU 光谱引擎**（D046）：`--engine spectral-gpu` / UI“光谱 GPU（物理，快）” | ✅ 2.6 MP 单帧 **47–50 ms**、视频 **379 ms/帧**；与 CPU 参考 82–119 dB；无设备自动回退 |
| **GPU 质感**（D047）：`--texture-engine auto/cpu/gpu` / UI“GPU 质感”勾选框 | ✅ 视频 **382.7 → 245.6 ms/帧**；与 CPU 参考 143–166 dB；无设备自动回退 |
| 参考分析文档 `docs/ui-analysis-spektralab.md` | ✅ 含布局、令牌、组件与映射表 |
| 渲染完成询问是否打开文件夹 | ✅ |
| 关闭时保存路径/参数/窗口位置，下次恢复 | ✅ |
| `scripts\build-ui.cmd`：配置 → 编译 → windeployqt → 启动 | ✅ |
| CI 默认不建 UI（`SR_BUILD_QT_UI=OFF`），run 39 双平台绿 | ✅ |
| 截图 `docs/ui-preview.png`、`docs/ui-queue.png` | ✅ |

界面原则：**UI 不复制颜色/质感逻辑**，预览和渲染都调用 `spektra-roll-cli`；Qt 动态链接并随 exe 部署，符合 GPL/LGPL。

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
| **D039** | 颗粒场缓存 | **LRU 保留最近两个场，按 seed/尺寸/半径/序号复用** | 全质感 476.5→358.7 ms/帧；输出与无缓存逐位一致 |
| **D040** | 分段渲染 / 续渲 / 时间码 | **`--segment` 写视频-only 分段，`--resume` 复用带标记的段，音频最后复制一次，timecode 按帧偏移** | 中断只重渲缺失段；99 帧输出保留 15:38:35:00，分段边界无跳变 |
| **D041** | 界面 v2 结构 | **参照 SpektraLab：三栏 + 顶栏、SpektraLab 灰/琥珀令牌、Section + SliderRow** | 画布成为最大区域；不复制其源码/图标；分析见 `docs/ui-analysis-spektralab.md` |
| **D042** | 冲印流程分组 | **右栏改“显影前 / 显影后 / 队列 / 日志”；底片参数与印片/扫描参数分开；暴露 DIR 耦合剂与色域压缩** | 参数顺序与电影流程一致；LUT 仍是单张烘焙，不做运行时两段渲染 |
| **D043** | 胶片 / 相纸选择 | **导入上游全部 profile；界面列出 11 支彩色负片 × 6 种相纸，自动选 target_print** | 界面可用组合从 1 对变成 66 对；反转片/T 型/电影印片暂不列出 |
| **D044** | 可选物理光谱引擎 | **`--engine spectral`：逐像素跑 FilmModel，不走 LUT；UI 引擎可选** | 2.6 MP 单帧 1081 ms，视频 1259 ms/帧；短片可全物理，长片仍用 LUT |
| **D045** | GPU 光谱可行性 | **D3D11 探针：2.6 MP 的 81 样本扫描 0.63 ms；下一步移植完整链** | Arc A750 潜力足够；GPU 路径 Windows-only，CPU 回退保留 |
| **D046** | GPU 光谱引擎 | **`--engine spectral-gpu`：HLSL 逐行翻译 CPU 参考链（D3D11 compute），常量缓冲一次上传，失败/无设备自动回退 CPU 光谱** | 2.6 MP 单帧 47–50 ms、视频 379 ms/帧；与 CPU 参考 82–119 dB，CI 有 `gpu_film` parity 测试；UI 列出“光谱 GPU” |
| **D047** | 空间质感上 GPU | **`--texture-engine auto/cpu/gpu`：halation / glare / diffusion / grain 逐 pass 翻译成 D3D11 compute；模糊保持 CPU 的累加顺序，自动回退** | 视频 382.7 → 245.6 ms/帧（1.56×）；质感本身约 160 → 38 ms；与 CPU 参考 143–166 dB；`test_gpu_texture` 九组配置 |
| **D048** | 预览实时化 | **`preview` 常驻进程：整段解码进内存（≤1024 MB、最长边 1600），`frame` 只跑颜色 + 质感并输出 8-bit RGBA；UI 拖动 90 ms 合并** | 预览 **1.7–2.2 s → 47–64 ms/帧**；首次打开 2.3–2.7 s；`ffprobe -show_streams` 一次取全（rotation 查询从 3.8 s 降到 42 ms） |
| **D049** | 融合 GPU 提交 | **颜色与质感共用一个 D3D11 设备：film 结果留在显存，质感以 SRV 直接读，只上传/回读一次** | 视频 233.6 → **207.1 ms/帧**；与分段路径 **逐位相同**；`test_gpu_chain` 与 CPU 参考 124 dB |
| **D050** | 视频管道修复 | **RGB48 量化去掉 `std::lround`（130 ms/帧），两个整帧循环并行；硬件解码实测更慢，不采用** | 视频 207.1 → **85–92 ms/帧**（2.4×）；`demo-m3` 358.7 → 245.5；输出逐位相同 |
| **D051** | 读写线程重叠 | **解码双缓冲 + 编码两槽位后台线程，读写不再阻塞 GPU 处理** | 视频 85–92 → **79.2 ms/帧**；`demo-m3` 226.8；分段/续渲不变；输出逐位相同 |
| **D052** | 并行分段渲染 | **`--jobs N`：每 worker 独立 GPU 对象/颗粒缓存，原子计数器领段；默认 min(3, 核数/4, 段数)** | CPU 路径 30 秒素材 **157.98 → 84.19 s（1.88×）**；GPU 路径 1.10×（单卡）；输出逐位相同 |
| **D053** | 长片预览代理 | **整段进内存会把最长边压到 960 px 以下时，生成 960 px 全 I 帧代理（`out/ui-proxy/`），按需取帧；失败回退内存缓存** | 30 秒素材 960×540 **100 ms/帧**（原先约 640 px）；重开 48 ms；4 秒素材不变 |
| **D054** | 真实点扩散核 | **光晕/眩光/扩散改用底片微米 + 双高斯重尾核（窄芯 + 宽裙），按 `--frame-width-mm` 换算；CPU 生成权重、GPU structured buffer 上传；`--spread-model box` 保留旧盒状模糊** | 同帧 box vs physical **57.03 dB**；GPU 视频 **82.2 ms/帧**；LUT+CPU 质感 +8.5%；parity **149–178 dB**；`test_texture` 新增物理核检查 |
| **D055** | 浮点交付 | **自带最小 OpenEXR（scanline、NONE、RGB、float/half）；`.exr` 即浮点且不钳制；支持 EXR 读入与 `--out-ext exr` 序列；`--bits 32` 配 PNG 报错** | float 30 MB / half 15 MB（2.6 MP）；读回 **PSNR = inf**；+3 EV 时 33% 像素 >1 保留；CI 双平台 EXR 冒烟 |
| **D056** | macOS GPU 颜色 | **Metal 移植光谱链（运行时编译，shared buffer）；无设备回退 CPU；质感留到 D057** | macOS CI `gpu_film` 真跑并通过；**fast math 必须关**，否则 NaN 保护被优化掉、整帧 NaN |
| **D057** | macOS GPU 质感 | **14 个 pass 翻成 MSL；一个 command buffer + `setBytes` 内联参数/taps；融合路径借用 film 的 MTLBuffer** | macOS CI 9/9（`gpu_texture` 2.11 s、`gpu_chain` 2.10 s）；真机逐位相同、融合 134.8 dB |
| **D058** | 线性浮点交付 | **`--exr-linear` + `--exr-trc srgb/pq/hlg` + `--peak`；1.0 = 白/峰值；不钳制；不改默认** | display 0.016..0.887 → linear 0.0012..0.762；+3 EV 线性最大 50.3 |
| **D059** | EXR 压缩 | **默认 ZIP（16 行/chunk），`--exr-raw` 关闭；stb zlib；压不动回退原始块；读取器支持 NONE+ZIP** | 已调色帧 −28%、带颗粒 ≈0%、half −38%；ffmpeg 交叉验证 92.6 dB |
| **D009** | Windows / 桌面 UI 技术栈 | **Qt 6（Qt Widgets）**（你已拍板） | 不装 MSVC，用 Qt 官方 MinGW；同一套 UI 以后可编到 macOS；动态链接 Qt 与 GPL-3.0 兼容 |
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

1. **OCIO（先定依赖策略）**：A) 引入 OpenColorIO（Windows/macOS CI 都装） B) OCIO-lite（只做我们要的输入/显示变换）；线性 EXR 已有，但不是完整色彩管理。
2. **PIZ / DWA 压缩**：颗粒重的素材 ZIP 收益接近 0，PIZ/DWA 能再小 2–3 倍。
3. **代理清理 / 4K 预览**：`out/ui-proxy/` 会按素材累积；4K 素材可考虑 1280 px 代理。
4. **macOS Qt 打包**：现在 Mac 只跑 CLI，界面还没打包。

> 已完成：D047 GPU 质感、D048 预览实时化、D049 融合提交、D050 管道修复、D051 读写重叠、D052 并行分段、D053 预览代理、D054 真实点扩散核、D055 浮点 EXR、D056 Metal 光谱、D057 Metal 质感、D058 线性 EXR、D059 EXR ZIP。

### 🟡 中期路线（M4–M5）

| 阶段 | 内容 |
|---|---|
| **M4** | ✅ 颗粒场缓存、✅ 分段/续渲/时间码、✅ GPU 颜色/质感、✅ 融合提交、✅ 并行分段、✅ 预览代理、✅ 真实点扩散核、✅ 浮点/线性 EXR、✅ EXR ZIP；待做：OCIO、PIZ/DWA |
| **M5** | ✅ Qt 6 桌面界面主体、✅ 预览实时化、✅ macOS Metal 光谱、✅ macOS Metal 质感；待做：OFX 插件、渲染农场命令行、macOS Qt 打包 |

### 🟢 还没开始、但必须记住的欠账

- [x] ~~macOS 本机验收~~ → 已在 MacBook Air M1 上跑通（编译、测试、LUT parity、视频路径）
- [x] ~~没有时间维度~~ → `video` 命令已能吃 `.mov`、带原声、支持帧区间
- [x] ~~没有确定性颗粒种子~~ → `--seed` + `--grain-rate`，同 seed/帧逐位可复现
- [x] ~~长片中断要重头~~ → `--segment` + `--resume`，只补缺失段
- [x] ~~输出丢时间码~~ → 自动复制源 timecode，`--timecode` / `--no-timecode` 可覆盖
- [x] ~~颜色只能在 CPU 上物理算~~ → `--engine spectral-gpu`（D046），2.6 MP 单帧 50 ms；无显卡自动回退
- [x] ~~空间质感还没上 GPU~~ → `--texture-engine gpu`（D047），视频 382.7 → 245.6 ms/帧；与 CPU 参考 143–166 dB
- [x] ~~颜色与质感仍是两次 GPU 提交~~ → D049 融合提交，207.1 ms/帧；D050/D051 修完管道到 **79.2 ms/帧**
- [ ] **CI 上没有真实 GPU** → `gpu_film` / `gpu_texture` / `gpu_chain` 在公有 runner 上会 skip，着色器退化只能在本机或真机 CI 发现
- [x] ~~**macOS 没有 GPU 物理路径**~~ → D056 Metal 光谱、D057 Metal 质感，两条链都在 GPU
- [ ] **没有色彩管理** → 没有 OCIO、没有 ACES 输入变换；Apple Log 是手写解码，不是 OCIO 配置
- [x] ~~**浮点 / EXR**~~ → D055 已完成（float / half EXR，读回逐位一致，不钳制）
- [x] ~~**空间效果不是物理核**~~ → D054 微米 + 双高斯重尾核；box 模型保留对照
- [x] ~~没有视频解码的硬件加速~~ → 实测 QSV 解码+下载 21.8 ms/帧 vs 软解 19.5 ms/帧，**不值得做**；QSV 编码在管道修好后有意义（H.264 77 vs 104 ms/帧）
- [x] ~~长片预览只能缩成缩略图~~ → D053 960 px 全 I 帧代理，30 秒素材 960×540 · 100 ms/帧
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
Qt      X:\agent\tools\qt\6.8.3\mingw_64          Qt 6.8.3 (Widgets)
Qt GCC  X:\agent\tools\qt\Tools\mingw1310_64      13.1.0，只给 UI 构建用
```

### ⚠️ 几个已知陷阱

1. **`X:\agent\tools\mingw64\bin` 里有 `cmake.exe` 和 `ninja.exe` 会"抢名字"**
   → 脚本里必须写绝对路径 `C:\Program Files\CMake\bin\cmake.exe` 和 `X:\agent\tools\ninja\ninja.exe`，不能靠 `Get-Command cmake`。

2. **PowerShell 执行策略禁止跑 `.ps1`**
   → 所有给用户的入口都是 `.cmd`，内部用 `powershell -NoProfile -ExecutionPolicy Bypass -File`。不要直接叫用户跑 `.ps1`。

3. **MinGW 编译的 exe 默认依赖运行时 DLL，直接双击会报 0xc0000135**
   → CLI 和测试目标已经加 `-static`；**Qt UI 例外**，它必须动态链接 Qt，靠 `windeployqt` 把 DLL 放到 exe 旁边。

4. **ffprobe 7+ 遇到 timecode 轨会把同一个 stream 打印两遍**（`stream_groups` 和 `streams` 各一次）
   → 所有解析 ffprobe 文本输出的地方都要只取第一行（`firstLine()`），CI 里的断言也要 `| head -n 1`。这是 2026-11-02 CI 变红的直接原因。

5. **Qt 自带的 MinGW 需要把它的 `bin` 放进 PATH**
   → `g++.exe` 本身能启动，但它调用的 `cc1plus.exe` 在 libexec 里，找不到同目录的 `libstdc++-6.dll` 会静默返回 0xC0000135。`build-ui.ps1` 已经先 `$env:PATH = "$mingw\bin;$qt\bin;..."`。

6. **macOS 的 libc++ 比 libstdc++ 严**
   → `std::filesystem::file_time_type::count()` 比 `long long` 宽，直接 `std::to_string` 会重载歧义；先 `static_cast<long long>`。CI 现在会在构建失败时把 `error:` 行打成 annotation。

7. **调试开关与环境**
   → `SPEKTRA_VIDEO_DEBUG=1` 打印视频分段耗时；`SPEKTRA_PREVIEW_DEBUG=1` 打印预览服务端的 look/open/frame 耗时；预览代理在 `out/ui-proxy/`（可随时删，会按素材重建）。

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
scripts\demo-m4.cmd             分段渲染 + 模拟崩溃 + 断点续渲 + timecode
scripts\demo-gpu-texture.cmd     GPU 质感对比（CPU vs GPU 的 ms/帧与 PSNR）
scripts\demo-spread.cmd          空间扩散对比（旧盒状模糊 vs 新物理核，并排图 + ms/帧）
scripts\demo-exr.cmd             浮点交付（PNG16 / float EXR / half EXR + 读回比对）
scripts\build-ui.cmd            编译 Qt 6 界面、部署 DLL 并启动（首次约 1–2 分钟）
scripts\verify-m1.cmd           一键验收：编译 + 测试 + 烘 LUT + 和参考对比
scripts\demo.cmd                M0 的合成 LUT 冒烟测试（不是胶片）
scripts\push.cmd                登录 GitHub 并推送
```

macOS 上双击 `scripts\verify-macos.command`（需要 `xcode-select --install` 和 `brew install cmake ffmpeg`）。

### 验证是否还健康

```powershell
cd X:\agent\SpektraRoll
.\build\spektra-roll-cli.exe version        # 应输出 spektra-roll-cli 0.1.0
ctest --test-dir build --output-on-failure # 应 9/9 通过（gpu_film / gpu_texture / gpu_chain 无显卡时 skip）
.\build\spektra-roll-cli.exe lut bake --out out\check.cube
.\build\spektra-roll-cli.exe lut compare --a tests\reference\portra400_endura_33.cube --b out\check.cube
# 应输出 PSNR ≈ 88 dB, PASS
.\build-ui\spektra-roll-ui.exe --preview-selftest   # 第二次预览应 < 100 ms
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
- 长片：`--segment 10 --resume` 分成 10 秒一段；中断后重跑同一条命令只补缺失段
- 时间码：默认复制源 `timecode`；`--timecode 01:00:00:00` 覆盖，`--no-timecode` 关闭

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
├── src/cli/main.cpp         命令行入口（gen / convert / video / preview / lut bake|apply|compare|info / version）
├── src/cli/gpu_film.{hpp,cpp}  D3D11 光谱后端（D046），非 Windows 编译成回退占位实现
├── src/cli/gpu_texture.{hpp,cpp} D3D11 质感后端（D047），非 Windows 编译成回退占位实现
├── src/ui-qt/main.cpp       Qt 6 桌面界面（D009），调用 CLI，不复制管线
├── tests/                   9 个测试程序 + reference/ 冻结参考 LUT（三个 GPU 测试无显卡时 skip）
├── data/profiles/           上游 profile JSON（CC BY-SA 4.0）+ NOTICE.md
├── tools/parity/            Python 参考链、上游交叉检查、说明
├── third_party/stb/         stb_image / stb_image_write（单头文件）
├── scripts/                 .ps1 + .cmd 双份，.cmd 给用户双击
├── .github/workflows/       build.yml：Windows + macOS 矩阵
├── docs/architecture.md     目标架构、M1 链路、确定性规则、M0–M5 路线图
├── docs/ui-analysis-spektralab.md  SpektraLab 界面组成分析与映射
├── docs/licensing.md        GPL / CC BY-SA / LUT / 商标 / 编解码专利约束
├── README.md                对外介绍
├── RUN.md                   给非程序员的操作手册
├── STATUS.md                一页纸进度
├── DECISIONS.md             决策台账 D001–D046
├── progress.md              本文件
└── LICENSE                  GPL-3.0
```

---

## 六、下游 AI 的“别做的事”清单

- ❌ **不要推翻 D001–D046**，那些是已经执行并验收/记录过的
- ❌ **不要问用户技术细节问题**，只能问“选 A 还是 B”
- ❌ **不要在没跑 `verify-m1.cmd` 的情况下改光谱链**，改完 PSNR 掉了要立刻知道
- ❌ **不要把 display-encoded sRGB 的 LUT 说成线性/ACES LUT**（D013）
- ❌ **不要以为写了 EXR 就是线性素材**：D055 的 EXR 存的是 display-encoded 值（sRGB / PQ 码值），只是没有钳制；进调色软件要设对应输入变换
- ❌ **不要在 EXR 路径上恢复 0..1 钳制**：浮点交付的意义就是把超白和负值留下来；PNG 才夹
- ❌ **不要静默把 `--bits 32` 降级**：`.png` 装不下浮点，必须报错让用户改成 `.exr`
- ❌ **不要把 EXR 常量当成“随便什么压缩都行”**：当前只写/读 NONE 压缩的 scanline；接 ZIP/PIZ 要同时改读和写并加测试
- ❌ **不要说“没有 Mac”**，产品负责人有 MacBook Air M1；macOS 相关先让他双击 `scripts\verify-macos.command` 拿结果
- ❌ **不要只在一个平台验证改动的路径**（issue #1 的教训）：CI 的 macOS 视频冒烟现在开着，别关掉
- ❌ **不要把颗粒的 `--grain-rate` 默认改回 1**，那是数字沸腾（D036）
- ❌ **不要在逐帧循环里绕过 `GrainFieldCache` 直接调 `applyGrain`**，那会把每帧两个噪声场重新算一遍（D039）
- ❌ **不要在分段渲染里逐段编码音频**：音频只从源文件复制一次（D040），否则 AAC 接缝和 A/V 漂移会回来
- ❌ **不要把分段里的颗粒帧号改回每段从 0 开始**，那会在段边界闪一下（D040）
- ❌ **不要在没有基准数字的情况下改质感默认值**，颗粒/眩光都是按你的反馈调过的（D034/D036）
- ❌ **不要在 Qt UI 里重新实现颜色/质感管线**：预览和渲染都调 `spektra-roll-cli`，否则 UI 和 CI 会漂移（D009）
- ❌ **不要把 Qt 静态链接或只发一个 exe**：Qt 必须动态链接并用 `windeployqt` 带 DLL（LGPL 合规）
- ❌ **不要把 `X:\agent\tools\qt` 提交进仓库**：它只在本机工具链里
- ❌ **不要直接把 SpektraLab 的 QML 源码/图标搬进来**：只借鉴布局和令牌，来源与许可见 `docs/ui-analysis-spektralab.md`（D041）
- ❌ **不要把参数再堆回左栏**：左栏是材料，参数在右栏；画布必须保持最大
- ❌ **不要偷懒改品牌**，SpektraRoll 已定案，改一次要动二进制名/CI/文档
- ❌ **不要删掉 GPU 着色器里的 `density != density` 判空**：有几条波长的 film density 是 NaN，CPU 参考同样跳过它们；少这个保护整条光谱和会变 NaN，输出直接全黑（D046）
- ❌ **不要把 `--engine spectral-gpu` 改成硬失败**：没有 D3D11 设备时必须回退 CPU 光谱，macOS 才能继续用（D045/D046）
- ❌ **不要在 CLI / UI 里另写一份质感顺序**：`sr::applyTexture()` 是唯一 CPU 参考链，GPU parity 测试也拿它当基准（D047）
- ❌ **不要把盒状模糊改成并行前缀和或换累加顺序**：GPU 现在逐位对齐 CPU 靠的就是“一行一列顺序累加”，换了就过不了 parity（D047；物理核时代实测 149–178 dB，见 D054）
- ❌ **不要把 `--texture-engine gpu` 改成硬失败**：编译失败、无设备、运行时错误都要回退 CPU 并只 warning 一次（D047）
- ❌ **不要在 Metal 里用默认的 fast math**：`MTLCompileOptions.fastMathEnabled` 默认 YES，会把 `density != density` 的 NaN 保护优化掉，光谱里几条 NaN 波长直接让整帧变 NaN（D056）；D3D11 复现不了
- ❌ **不要把 D3D 的 group 数直接传给 Metal 的 `dispatchThreads`**：它收的是总线程数，会少派 64 倍的线程；用 `dispatchThreadgroups:` 或自己乘好（D057）
- ❌ **不要在同一个 command buffer 里用共享缓冲传 PSF taps**：链内后一个效果会覆盖前一个的权重；taps 和参数都要用 `setBytes` 内联（D057）
- ❌ **不要在融合路径里把 film 的 MTLBuffer 当作 ping-pong 的写目标**：第一遍之后必须换回自己的缓冲，否则覆盖 film 结果（D057）
- ❌ **不要把 Metal 的 `Params` 改成 `float4` 数组**：现在用 `float[4]` 是为了和 C++ 结构逐字节一致；换 `float4` 会引入 16 字节对齐规则，错位后整帧都是垃圾（D056）
- ❌ **不要在 GPU 质感里省略颗粒的两场交叉淡化或 `norm`**：那是 D036 防闪烁的根，改了颗粒会重新沸腾（D047）
- ❌ **不要把 `--texture-engine` 漏出分段签名**：否则 CPU/GPU 质感的分段会被混着续渲（D047）
- ❌ **不要在 UI 预览里恢复“每帧一个 ffmpeg + 一个 CLI 进程”**：那条链 1.7–2.2 s/帧，现在由 `preview` 常驻进程 + 整段缓存接管（D048）
- ❌ **不要用 `ffprobe -show_entries side_data=rotation` 做几何探测**：这支素材要 3.8 s；一次 `-show_streams` 42 ms 就够（D048）
- ❌ **不要在拖动时间线时走单帧慢路径**：超出缓存的帧在拖动中直接跳过，松手才允许回退（D048）
- ❌ **不要把预览缓存设成无上限**：默认 1024 MB / 最长边 1600，长片会吃掉内存（D048）
- ❌ **不要把 `std::filesystem::file_time_type::count()` 直接塞进 `std::to_string`**：libc++ 的 rep 比 `long long` 宽，重载会歧义，只有 macOS 编译失败（D053）；先 `static_cast<long long>`
- ❌ **不要把 PSF 的 taps 塞进 D3D11 常量缓冲**：HLSL 的常量缓冲数组是 **16 字节/元素**，按 4 字节排会读到错位权重（parity 掉到 19–48 dB）；用 structured buffer + `kernelRadius`（D054）
- ❌ **不要在 HLSL 里重新算 `exp` 生成核权重**：CPU `sr::buildSpreadKernel` 是唯一来源，GPU 必须上传同一组 float 才能过 150 dB 的 parity（D054）
- ❌ **不要把光晕/眩光/扩散的尺寸改回"画幅比例"**：现在按底片微米算，`--frame-width-mm` 决定换算；改回比例会让 1080p 和 4K 观感不一致（D054）
- ❌ **不要删掉 `--spread-model box`**：那是旧观感的回退路径，也是新旧核对比的唯一途径（D054）
- ❌ **不要在物理核的测试里用 64 px 小图**：225 µm 在 64 px 宽上只有 0.4 px，等于没效果；按 `width / frameWidthMm` 选尺寸（640 px ≈ 4 px σ）（D054）

---

## 七、下一次会话的理想开场

你说一句：

> “读 progress.md，继续。”

AI 就应该：

1. 先跑 `ctest --test-dir build --output-on-failure`：9 个测试，其中 `gpu_film` / `gpu_texture` / `gpu_chain` 无显卡时会 skip（正常）
2. 再跑 `scripts\verify-m1.cmd -NoOpen` 确认 M1 parity 还是 88.07 dB
3. 再跑 `scripts\demo-m3.cmd -NoOpen` 确认视频 + 质感链路没坏（约 40 秒）
4. 再跑 `scripts\demo-m4.cmd -NoOpen` 确认分段 + 续渲 + timecode（约 20 秒）
5. 跑 `scripts\build-ui.cmd -NoLaunch` 确认 Qt 界面还能编译部署
6. 颜色路径改过就补一次 GPU 校验：`lut apply --engine spectral` 与 `--engine spectral-gpu` 出图做 PSNR（应 ≥60 dB，本机实测 ≥82 dB）
7. 质感路径改过就跑 `scripts\demo-gpu-texture.cmd -NoOpen`：CPU vs GPU 两条 ms/帧、加速比与编码后 PSNR（物理核后本机约 2.9×、85.6 dB）
8. 空间扩散改过就跑 `scripts\demo-spread.cmd -NoOpen`：box vs physical 的 PSNR 与 ms/帧；GPU 视频应约 82 ms/帧
9. 浮点 / 图像 I/O 改过就跑 `scripts\demo-exr.cmd -NoOpen`：EXR 读回应与 PNG16 逐位一致（PSNR = inf），+3 EV 那行应显示约 33% 的像素 >1
10. 预览路径改过就跑 `.\build-ui\spektra-roll-ui.exe --preview-selftest`：第二次预览应 <100 ms（短片 47 ms；长片走 960 px 代理约 100 ms）
11. 视频管道改过就设 `SPEKTRA_VIDEO_DEBUG=1` 跑一次 2 秒渲染：应看到 `wait / convert / process / pack` 四项，总时长约 79–82 ms/帧
12. 分段改过就删掉 `.parts` 里的中间段再 `--resume` 一次，确认只补缺失段（并行下同样成立）
13. macOS / Metal 改过就在 Mac 上跑 `scripts/verify-macos.command`：`gpu_film` / `gpu_texture` / `gpu_chain` 都应真跑（不是 skip），第 5 步会打印设备信息、三组 parity 与 CPU vs Metal 的 PSNR
14. 看 §三 的"下一步"（当前最该做：Mac 2.6 MP 实测，然后线性 / OCIO）

约束：每次改完都要重跑 `lut compare`（不能掉到 88 dB 以下）和 9 个测试程序。
