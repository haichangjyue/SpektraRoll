# RUN — 三步跑起来

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

## 第 3 步：看结果

双击：

```
X:\agent\SpektraRoll\scripts\demo.cmd
```

它会自动生成两张图并打开文件夹：

- `out\chart.png` —— 原始测试图（灰阶 + 色块）
- `out\chart_warm.png` —— 套了暖调 LUT 之后（注意看右边白色是不是偏暖了）

**你要做的判断只有一个：第二张有没有"胶片味"，还是死板的三原色。**
觉得不对就告诉我"太淡了 / 太黄了"，我来调。

## 出问题了怎么办

把窗口截图发给我，或者说一句"跑挂了"。不需要你自己修。

## 给技术朋友看的命令（你可以忽略）

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
.\build\spektra-roll-cli.exe --help
```
