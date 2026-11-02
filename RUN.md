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

## 看 GPU 质感（颗粒 / 光晕 / 眩光 / 扩散）

双击：

```
X:\agent\SpektraRoll\scripts\demo-gpu-texture.cmd
```

它会用同一条命令把 2 秒素材渲染两遍：一遍质感在 CPU（参考），一遍在显卡（D3D11）。跑完会打印：

- `CPU texture: xxx ms/frame`
- `GPU texture: xxx ms/frame`
- `Speed-up: x.xx×`（本机约 1.6×）
- 两段成片的 PSNR（本机约 83 dB，差异来自编码器）

**你要看两件事**：两段画面应该看不出区别；GPU 那条应该明显更快。想改参数：`demo-gpu-texture.cmd -Grain 0.2 -Glare 0.1`。

没有 D3D11 显卡（比如 Mac）时，命令会自动退回 CPU，不会失败。

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
- **按住看原片**：按住按钮显示未套胶片的画面，松开回到成片
- **滚轮缩放 / 拖拽平移**；`Ctrl+0` 适合窗口、`Ctrl+1` 100%、双击画布切换
- 画布右上角是当前预览的 **RGB 直方图**
- 把视频文件**拖进窗口**也能载入
- `Ctrl+P` 预览、`Ctrl+R` 渲染、`Ctrl+B` 烘焙 LUT

### 引擎：LUT、物理光谱还是 GPU

左栏“输出”里有 **引擎**：

- **LUT（快）**：默认；每帧查 33³ 表，4 秒 99 帧约 35 秒
- **光谱（物理，慢）**：逐像素跑完整底片/相纸物理链，不查 LUT；2.6 MP 约 **1.1 秒/帧**，4 秒 99 帧约 **2 分钟**
- **光谱 GPU（物理，快）**：同一条物理链在显卡上跑，2.6 MP 约 **50 毫秒/帧**；没有 D3D11 显卡时自动退回 CPU 光谱

引擎下面还有一个 **GPU 质感** 勾选框（默认开），控制颗粒、光晕、眩光、扩散是否也放到显卡上。2.6 MP 全质感视频从约 380 ms/帧降到约 **245 ms/帧**；取消勾选就用 CPU 参考链，两条链的画面区别小到测不出来（PSNR 120 dB 以上）。

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
```

它会编译、跑单元测试、烘一张 Portra 400 LUT 并和参考对比；如果装了 `ffmpeg`，
还会顺带测一遍视频路径。第一次双击如果被系统拦下，右键 → 打开即可。

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
