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
- `out\m2-negative.png` —— 扫描负片：高光和暗部都能留住，画面偏平，留给你调色
- `out\m2-negative-contrast.png` —— 同上，加了一点反差（1.15）

**你要判断四件事：**

- 天空和窗户：有没有层次，还是糊成一片白
- 暗部：有没有死黑，细节还在不在
- 彩色招牌和墙面：有没有过饱和或者串色
- 负片 vs 加反差：哪个更像你要的（前者留余量，后者更"出片"）

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
