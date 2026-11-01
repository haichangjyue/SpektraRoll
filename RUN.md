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

## 用图形界面（Qt 6）

双击：

```
X:\agent\SpektraRoll\scripts\build-ui.cmd
```

第一次会编译界面并复制 Qt 运行库（约 1–2 分钟），然后自动打开窗口。以后双击都会重新编译再打开。

界面里从左到右、从上到下：

1. **视频素材**、**画面类型**（Apple Log / 普通 SDR）、**ffmpeg** 路径
2. **LUT**：点“烘焙”按当前曝光/负片/反差重新生成
3. **胶片质感**：颗粒、颗粒稳定度、光晕、眩光、扩散
4. **渲染**：输出文件、编码、起始、帧数、分段长度、断点续渲
5. 右下两个按钮：**预览一帧**、**渲染整段**

预览会抽当前“预览时间”的一帧，套 LUT 和质感后显示在右边；渲染的日志和进度条在下面。取消后已完成的段会保留，下次勾选“断点续渲”即可接着跑。

界面只是壳：真正干活的是同一个 `spektra-roll-cli`，所以画面和命令行完全一致。

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
