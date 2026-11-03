# RUN — 四步跑起来

> 这份文档是给你看的，不需要懂代码。
> 所有操作都是**双击**，或者在 PowerShell 里粘贴一行命令。

## 第 1 步：装工具链（只做一次）

双击：

```
X:\agent\SpektraRoll\scripts\dev-install.cmd
```

它会自动下载 CMake / Ninja / MinGW 编译器，**不需要管理员权限**，不用点 UAC。
大概 260 MB，看你网速，等它跑完出现 `Toolchain ready` 就行。

## 第 2 步：编译

双击：

```
X:\agent\SpektraRoll\scripts\build.cmd
```

看到 `OK. Binary:` 就是成功了。

## 第 3 步：看 M1 真正的胶片样张（重点）

双击：

```
X:\agent\SpektraRoll\scripts\demo-m1.cmd
```

它会自己做四件事，然后自动打开两张图：

1. 烘一张真正的 **Kodak Portra 400 + Portra Endura** LUT（33³）
2. 生成一张 16-bit 测试图
3. 把 LUT 套上去
4. 和 Python 参考实现逐点对比，打印 PSNR / ΔE

你会看到：

- `out\m1-before.png` —— 原始测试图（灰阶 + 色块）
- `out\m1-after.png` —— 过完 Portra 400 + Endura 的结果

**你要判断三件事：**

- 上半部的灰阶：暗部是不是偏暖，亮部是不是发灰（不是纯白）
- 第二排左起第 1 块（肤色）：是不是自然，还是发黄 / 发红
- 红色和蓝色色块：有没有过饱和或者串色

觉得不对就告诉我"太黄了 / 太灰了 / 肤色太红"，我来调。

### 看 Apple Log 真实素材（M2）

> 现在输入不止 Apple Log：CLI 的 `--input` 和界面左栏“画面类型”都可以选
> **Sony S-Log3（S-Gamut3.Cine / S-Gamut3）、Canon C-Log3、Panasonic V-Log、
> ARRI LogC3 / LogC4、Blackmagic Film Gen 5、DaVinci Intermediate**（D060/D061）；
> 选任何 Log 都会自动用对应曲线和色域解码。
>
> 素材的 YUV→RGB 矩阵现在**跟着素材自己的标签走**（`--in-matrix auto`），
> 不需要按相机去猜；标签写错时才用 `--in-matrix bt709` / `bt2020` 覆盖。
>
> 想一次看清九种输入：双击 `scripts\demo-log-inputs.cmd`，会给每种输入烘一张
> LUT、出一张图，并打印 CPU 与 GPU 的 PSNR。

双击：

```
X:\agent\SpektraRoll\scripts\demo-m2.cmd
```

它会用 `ffmpeg` 从 `X:\agent\footage\` 里的 `.mov` 抽一帧，然后出四张图：

- `out\m2-before.png` —— 只解码，不上胶片（对照）
- `out\m2-after.png` —— 印片：Kodak Portra 400 + Endura（对比强，像洗出来的照片）
- `out\m2-negative.png` —— 扫描负片：默认反差 1.15，高光和暗部都能留住
- `out\m2-negative-flat.png` —— 同上但不加反差（1.0），余量全留给你

**你要判断四件事：**

- 天空和窗户：有没有层次，还是糊成一片白
- 暗部：有没有死黑，细节还在不在
- 彩色招牌和墙面：有没有过饱和或者串色
- 负片 vs 不加反差：默认 1.15 更"出片"，1.0 留全部余量

### HDR 版本（M2 最后一项）

同一帧还会出两个 HDR 文件：

- `out\m2-hdr-pq.png` —— 16-bit PQ 静帧（画面白 = 1000 nits）
- `out\m2-hdr-pq.mov` —— **整段 4.1 秒原片**：99 帧、24p、HEVC 10-bit、PQ + BT.2020、带原声

**重要**：MacBook Air 的屏幕是 SDR 的，在这台电脑上看它和普通视频一样；
用 AirDrop 传到 iPhone 16 Pro（XDR 屏）上才能看到真正的高光。

觉得不对就告诉我"太黄了 / 太暗了 / 高光糊了"，我来调曝光或曲线。

### 想确认"它没坏"就双击这个

```
X:\agent\SpektraRoll\scripts\verify-m1.cmd
```

它会重新编译、跑单元测试、重新烘 LUT、和冻结的参考 LUT 对比。
最后一行是 `PASS: M1 全部检查通过.` 就说明一切正常。

## 第 4 步：第一次推送到 GitHub（只做一次）

双击：

```
X:\agent\SpektraRoll\scripts\push.cmd
```

会弹一个浏览器窗口，用你的 GitHub 账号（`haichangjyue`）登录并授权。
之后代码就出现在 https://github.com/haichangjyue/SpektraRoll

以后每次我说"可以推了"，你双击这个文件就行。

## 看胶片质感（颗粒 + 光晕）

双击：

```
X:\agent\SpektraRoll\scripts\demo-m3.cmd
```

它会出三样东西：

- `out\m3-clean.png` —— 不带质感的同一帧
- `out\m3-texture.png` —— 加了颗粒、光晕、眩光、扩散（默认 0.4 / 0.45 / 0.2 / 0.1）
- `out\m3-texture.mov` —— 整段视频，带声音，颗粒逐帧变化

**你要判断三件事**（图片建议放大到 100% 看）：

- 颗粒是不是"细而不脏"，暗部和亮部应该比中灰少
- 播放视频时颗粒有没有在动（不是一张贴图）
- 高光边缘有没有一点点暖色晕开（不是发光泛滥）

参数可以改：`demo-m3.cmd -Grain 0.2 -Glare 0.1`，0 就是关掉。四个都有：`-Grain`、`-Halation`、`-Glare`、`-Diffusion`；颗粒的稳定度用 `-GrainRate`（默认 3，越大越稳，1 = 每帧全新颗粒）。

## 看分段渲染 + 断点续渲 + 时间码

双击：

```
X:\agent\SpektraRoll\scripts\demo-m4.cmd
```

它会自动做四步：

1. 烘一张 SDR 负片 LUT
2. 把 2 秒素材拆成 1 秒一段渲染（`out\m4-resume.mov.parts\`）
3. **故意删掉最后一段**，模拟渲染中途崩溃
4. 用 `--resume` 重跑同一条命令

**你要看两件事**：

- 第二次运行的日志里，第一段显示 `already done`，只渲染了被删掉的那段（几秒钟，不是整段重来）
- 最后打印的 `timecode` 应该是 `15:38:35:00`（片段原本的相机时间码），不是空的

长片用同样的办法：加 `--segment 10 --resume`，10 秒一段；中断后重跑同一条命令即可。`--keep-parts` 可以保留分段文件，`--no-timecode` 可以不要时间码。

分段默认会**同时跑 3 段**（`--jobs` 自动，最多 3）。LUT / CPU 引擎的渲染能接近线性加速（实测 30 秒素材 1.88×）；光谱 GPU 因为只有一张显卡，收益较小（1.10×）。显存紧张时加 `--jobs 1` 回到顺序渲染。

## 看 GPU 质感（颗粒 / 光晕 / 眩光 / 扩散）

双击：

```
X:\agent\SpektraRoll\scripts\demo-gpu-texture.cmd
```

它会用同一条命令把 2 秒素材渲染两遍：一遍质感在 CPU（参考），一遍在显卡（D3D11 / Metal）。跑完会打印：

- `CPU texture: xxx ms/frame`
- `GPU texture: xxx ms/frame`
- `Speed-up: x.xx×`（物理核后本机约 **2.9×**）
- 两段成片的 PSNR（本机约 **85.6 dB**，差异来自编码器）

**你要看两件事**：两段画面应该看不出区别；GPU 那条应该明显更快。想改参数：`demo-gpu-texture.cmd -Grain 0.2 -Glare 0.1`。

没有 D3D11 显卡（比如 Mac）时，命令会自动退回 CPU，不会失败。

macOS 上颜色和质感现在都走 **Metal**（D056/D057），和 Windows 的 D3D11 路径一致；没有可用 GPU 时会自动用 CPU 链，不会报错。Mac 上的验证请双击 `scripts\verify-macos.command`，它会打印设备信息、三组 GPU parity 和 CPU vs Metal 的 PSNR。

## 看空间扩散（旧盒状模糊 vs 新物理核）

双击：

```
X:\agent\SpektraRoll\scripts\demo-spread.cmd
```

光晕 / 眩光 / 扩散现在按**底片上的微米**计算（默认 225 / 725 / 122 µm，36 mm 画幅），核形状是"窄芯 + 宽裙"，不再是画幅比例的盒状模糊。这个脚本会把同一帧和同一段 1 秒视频各渲两遍：

- 左边 = 旧的盒状模糊，右边 = 新的物理核，并排放在 `out\spread-compare.png`
- 打印两张图的 PSNR（本机 **57 dB**，差异集中在高光边缘的柔化与外晕）
- 打印两个模型的 ms/帧

**你要看**：高光边缘是不是"贴边的暖芯 + 很淡很宽的外晕"；暗部有没有被眩光弄脏。想调距离就用 `demo-spread.cmd -HalationSpread 120 -GlareSpread 500`；想完全回到旧观感就加 `--spread-model box`（脚本已经各跑一遍）。

## 交付浮点 / EXR

双击：

```
X:\agent\SpektraRoll\scripts\demo-exr.cmd
```

它把同一帧渲染成 **16-bit PNG**、**32-bit float EXR**、**half EXR** 三种文件，然后：

- 打印三种文件的体积（本机约 15 MB / 30 MB / 15 MB）
- 用同一个 CLI 把 EXR 读回来再写 PNG，和原 PNG 比较（本机 **PSNR = inf**，逐位相同）
- 再做一次 **+3 EV** 的转换：EXR 里有 **33%** 的像素超过 1（最亮 5.34）被保留，PNG 那版全部切到 1

自己用的时候：

- `--out 名字.exr` 就是浮点输出；加 `--exr-half` 写 half
- EXR 默认 **ZIP 压缩**（实拍素材小 20–30%，合成图表几乎不值钱）；`--exr-raw` 关闭
- 加 `--exr-linear` 写**线性光**（1.0 = 白；PQ 素材配 `--exr-trc pq --peak 1000`，1.0 = 峰值尼特）
- 一整段序列：`lut apply --lut x.cube --in-dir frames --out-dir frames_exr --out-ext exr [--exr-linear]`
- EXR 也能当输入：`lut apply --lut x.cube --in frame.exr --out frame.png --bits 16`

**要注意**：默认 EXR 存的是显示编码值（sRGB 或 PQ/HLG 码值），进 DaVinci / Nuke 要把输入变换设成对应的 sRGB 或 PQ；只有加了 `--exr-linear` 的 EXR 才是线性光，按线性解读、不要再套 sRGB 变换。演示脚本会同时生成两份，可以直接对比。

## 九种相机 Log 输入

双击：

```
X:\agent\SpektraRoll\scripts\demo-log-inputs.cmd
```

它给九种输入各烘一张扫描负片 LUT、各出一张图，然后打印一张表：

| input | 说明 |
|---|---|
| apple-log | Apple Log（iPhone，BT.2020） |
| s-log3 | Sony S-Log3（S-Gamut3.Cine） |
| s-log3-sgamut3 | Sony S-Log3（S-Gamut3，宽的） |
| c-log3 | Canon C-Log3（Cinema Gamut） |
| v-log | Panasonic V-Log（V-Gamut） |
| logc3 | ARRI LogC3（Wide Gamut 3） |
| logc4 | ARRI LogC4（Wide Gamut 4） |
| bmd-film | Blackmagic Film Gen 5 |
| davinci | DaVinci Intermediate（DWG） |

表里是"烘 LUT 用了多少毫秒"和"GPU 光谱与 CPU 光谱的 PSNR"（本机 118–120 dB）。

**要注意**：演示用的这一帧是 iPhone 的 Apple Log，所以只有 apple-log 那行是"正确"的画面，其它几行是故意用错曲线解码的——它们只证明每种输入都能烘表、能出图、GPU 和 CPU 一致。真拿到 ARRI / Blackmagic / Sony 素材时，在界面左栏"画面类型"里选对应那一项就行。

自己用的时候：

- 曲线已经和 colour-science 0.4.7 的 101 点逐点对照过；18% 中灰的码值：logc3 0.3910、logc4 0.2784、bmd-film 0.3836、davinci 0.3360、s-log3 0.4106、c-log3 0.3434、v-log 0.4233、apple-log 0.4883
- 容器的 YUV→RGB 矩阵**跟着素材标签走**（`--in-matrix auto`），不用按相机猜；标签写错才用 `--in-matrix bt709` / `bt2020` 覆盖
- 还没做自动识别 Log 格式：曲线无法从像素可靠推断，仍然要手动选

## 用图形界面（Qt 6）

双击：

```
X:\agent\SpektraRoll\scripts\build-ui.cmd
```

第一次会编译界面并复制 Qt 运行库（约 1–2 分钟），然后自动打开窗口。以后双击都会重新编译再打开。

界面是三栏，和 SpektraLab 同一套骨架：

- **左栏**：素材（视频、画面类型、ffmpeg）、胶片与相纸（LUT、烘焙）、输出（文件、编码、起始、帧数、分段）
- **中间**：画布；下面是素材信息条、时间线、状态条（进度条）
- **右栏**：按冲印流程分四页：
  - **显影前**：曝光、**胶片选择**（11 支彩色负片）、DIR 耦合剂、颗粒/稳定度/光晕
  - **显影后**：**相纸选择**（6 种）、扫描方式、反差、OkLCh 色域压缩、眩光/扩散、预览时间
  - **队列**、**日志**
- **顶栏**：预览一帧、按住看原片、渲染整段、加入队列、取消、输出文件夹；右上角是文件名和 FIT 徽章

几个细节：

- 选中素材后，信息条显示分辨率、帧率、时长、帧数、相机 timecode
- **时间线**可以直接拖：拖动过程中约每 90 ms 刷新一帧，松开再补最后一帧；右边显示 `00:03.33 / 00:04.13 · 帧 80/99`
- 状态栏会写预览耗时，正常情况下是 **47–64 ms**（`· 整段缓存`）。第一次打开一支素材会先用 2 秒左右把整段解码进内存，之后换曝光、质感都不再解码
- 长片（整段进内存会把画面压到 960 px 以下）会自动生成一次 **960 px 代理**放在 `out/ui-proxy/`，之后按需取帧、约 **100 ms/帧**；第一次会多等几秒，以后秒开
- **按住看原片**：按住按钮显示未套胶片的画面，松开回到成片
- **滚轮缩放 / 拖拽平移**；`Ctrl+0` 适合窗口、`Ctrl+1` 100%、双击画布切换
- 画布右上角是当前预览的 **RGB 直方图**
- 把视频文件**拖进窗口**也能载入
- `Ctrl+P` 预览、`Ctrl+R` 渲染、`Ctrl+B` 烘焙 LUT

### 引擎：LUT、物理光谱还是 GPU

左栏“输出”里有 **引擎**：

- **LUT（快）**：默认；每帧查 33³ 表，4 秒 99 帧约 35 秒
- **光谱（物理，慢）**：逐像素跑完整底片/相纸物理链，不查 LUT；2.6 MP 约 **1.1 秒/帧**，4 秒 99 帧约 **2 分钟**
- **光谱 GPU（物理，快）**：同一条物理链在显卡上跑（Windows 用 D3D11，Mac 用 Metal），2.6 MP 约 **50 毫秒/帧**（Windows）；没有可用 GPU 时自动退回 CPU 光谱

引擎下面还有一个 **GPU 质感** 勾选框（默认开），控制颗粒、光晕、眩光、扩散是否也放到显卡上。2.6 MP 全质感视频从约 237 ms/帧降到约 **83 ms/帧**；取消勾选就用 CPU 参考链，两条链的画面区别小到测不出来（PSNR 150 dB 以上）。

光谱模式不需要 LUT，"烘焙"按钮会变灰；队列、分段、续渲、时间码都照常。短片/单帧想完全物理就选光谱，长片/4K 仍建议 LUT。

### 渲染队列

1. 选好素材、LUT、质感和输出，点 **加入队列**（会把当前参数和 LUT 冻结成一条任务）
2. 想排第二条就改输出路径或参数，再点一次；输出重名会自动加 `-2`、`-3`
3. 也可以一次把**多个视频拖进窗口**，或点 **批量加入** 多选文件，会按素材名自动排成多条任务
4. 切到右边的 **队列** 页，点 **开始队列**
5. 每条任务显示 等待 / 渲染中 / 完成 / 失败 / 已取消，带**进度条**和**用时 / 剩余时间**；可以 **停止**、**移除**、**清空完成**、**打开输出**

队列会自动保存到 `out\ui-queue\queue.json`：关掉界面再打开，未完成的任务会回到“等待”，点开始就能接着跑。已经渲染的文件当然也还在。

界面只是壳：真正干活的是同一个 `spektra-roll-cli`，所以画面和命令行完全一致。

想验证预览速度，不用手点：在仓库目录运行

```
build-ui\spektra-roll-ui.exe --preview-selftest
```

它会跑两次真实预览并打印状态行，第二次应该在 100 ms 以内。

## 在 Mac 上验证（可选）

你的 MacBook Air M1 也能跑这个程序。把仓库拷到 Mac 上，双击：

```
SpektraRoll/scripts/verify-macos.command
SpektraRoll/scripts/bench-macos.command     # 可选：2.6 MP 的 CPU vs Metal 计时
```

它会编译、跑单元测试、烘一张 Portra 400 LUT 并和参考对比；如果装了 `ffmpeg`，
还会顺带测一遍视频路径。最后会跑一次 **CPU 光谱 vs Metal 光谱** 的对比（D056），
打印 PSNR——如果这台 Mac 没有可用 Metal 设备，会看到 CPU 回退的提示。第一次双击如果被系统拦下，右键 → 打开即可。

需要先装两样东西（都是免费的）：

```
xcode-select --install     # Xcode 命令行工具
brew install cmake ffmpeg  # 没有 brew 的话先装 brew
```

## 出问题了怎么办

把窗口截图发给我，或者说一句"跑挂了"。不需要你自己修。

## 给技术朋友看的命令（你可以忽略）

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

# 烘 M1 LUT 并和参考实现对比
.\build\spektra-roll-cli.exe lut bake --out out\m1.cube
.\build\spektra-roll-cli.exe lut compare --a tests\reference\portra400_endura_33.cube --b out\m1.cube

# Apple Log 输入（LUT 定义域是 Apple Log 码值，曝光 +0.7 EV）
.\build\spektra-roll-cli.exe lut bake --input apple-log --exposure 0.7 --out out\applelog.cube
.\build\spektra-roll-cli.exe convert --in frame.png --out frame_sdr.png --from apple-log --exposure 0.7
.\build\spektra-roll-cli.exe lut apply --lut out\applelog.cube --in frame.png --out frame_portra.png

# 扫描负片（高光/暗部都留住，画面偏平）
.\build\spektra-roll-cli.exe lut bake --input apple-log --exposure 0.7 --scan negative --out out\negative.cube
.\build\spektra-roll-cli.exe lut apply --lut out\negative.cube --in frame.png --out frame_negative.png
```
