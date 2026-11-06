# SpektraRoll 0.1.0 · 预览版 1（macOS）

面向动态影像的开源胶片与印片模拟器。这是第一个公开预览版，**未上架 App Store**，从 DMG 直接安装。

## 系统要求

- **Apple Silicon（arm64）** Mac；Intel 版本后续再做
- **macOS 14.0 或更新**
- 约 400 MB 可用磁盘（App 本体约 174 MB，另加预览代理缓存）

## 安装

1. 打开 `SpektraRoll-0.1.0-preview1-arm64.dmg`
2. 把 **SpektraRoll** 拖进 **Applications**
3. 第一次启动：在「应用程序」里**右键 SpektraRoll → 打开 → 打开**

> 这个预览版是 **ad-hoc 签名、未公证**。直接双击会被 Gatekeeper 拦下，右键「打开」即可。
> 如果仍然被拦，在终端执行一次：
>
> ```bash
> xattr -dr com.apple.quarantine /Applications/SpektraRoll.app
> ```

## 第一次使用

1. 双击左侧空白区域，导入视频（`.mp4` / `.mov`，R3D 见下方限制）
2. 双击缩略图载入播放器
3. 右侧 **FILM / PRINT** 选胶片与相纸，打开「胶片效果」看当前帧
4. 首次播放时会在后台生成低分辨率代理，稍等几秒后自动切到胶片预览
5. **OUTPUT** 页设置输出、区间与渲染队列，点「导出」或「加入队列」

## 这个版本包含什么

- 20 支底片（含 Vision3 电影负片、Verita、4 支反转片）、8 种相纸 / 印片
- CPU 光谱引擎 + Metal GPU 光谱 / 质感
- 输入：sRGB、S-Log3、C-Log3、V-Log、ARRI LogC3/LogC4、BMD Gen5、DaVinci、Apple Log，以及 OpenColorIO 输入
- 输出：H.264 / HEVC / ProRes、PNG / 16-bit PNG / EXR（float / half / linear）
- 时间线拖动与逐帧预览、渲染队列、区间渲染、配光按钮
- 预览代理缓存（默认上限 5 GB，可在 ADVANCED 修改）
- 中文界面、深色主题、Mac 系统信息面板

## 已知限制

- **R3D 不可用**：官方包不含 RED SDK，不提供 R3D 解码；R3D 只限用户本地自建（BYO-SDK）
- **代理是近似**：播放代理用固定 LUT 近似光谱结果，最终渲染仍走你选择的引擎
- **首次代理生成**：4K50 6 秒素材约 10–15 秒（M1），期间会暂停播放
- **队列与渲染共用一个 CLI 进程**：停止队列会取消当前任务；已完成的 `.parts` 可用 `--resume` 续渲
- **没有自动更新**：新版本手动下载 DMG
- **仅 Apple Silicon**：arm64 thin，不含 Intel 切片
- **无遥测**：所有状态、缓存都在本机

## 反馈

- 问题与建议：<https://github.com/haichangjyue/SpektraRoll/issues>
- 源码：<https://github.com/haichangjyue/SpektraRoll>
- 许可证：GPL-3.0-or-later；第三方组件见 `THIRD_PARTY_NOTICES.md`

## 校验下载

```bash
shasum -a 256 -c SHA256.txt
```

Kodak、Fujifilm、Ilford 等名称是各自所有者的商标，仅用于标识所模拟的胶片材料。本项目与这些公司无关联。
