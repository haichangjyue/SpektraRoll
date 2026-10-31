# RUN — 三步跑起来

> 这份文档是给你看的，不需要懂代码。所有命令都在 **PowerShell** 里执行，窗口路径要停在仓库根目录 `X:\agent\spektravideo`。

## 第一次（只做一次）

```powershell
cd X:\agent\spektravideo
.\scripts\dev-install.ps1
```

会弹一次 UAC 授权（安装 Visual Studio 的 C++ 编译器），点"是"。
装完**关掉这个窗口，重新开一个**（让 PATH 生效）。

## 每次编译

```powershell
cd X:\agent\spektravideo
.\scripts\build.ps1
```

看到绿色 `OK. Binary:` 就是成功了。

## 看结果

```powershell
cd X:\agent\spektravideo
.\scripts\demo.ps1
```

它会自动生成两张图并打开文件夹：

- `out\chart.png` —— 原始测试图（灰阶 + 色块）
- `out\chart_warm.png` —— 套了暖调 LUT 之后

**你要做的判断只有一个：第二张看起来是不是"有味道"，而不是死板的三原色。**

## 出问题了怎么办

把报错窗口截图发给我，或者说一句"跑挂了"。不需要你自己修。
