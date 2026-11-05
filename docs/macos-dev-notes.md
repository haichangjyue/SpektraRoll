# macOS 开发/远程验收笔记（D073）

## 当前机器

- MacBook Air M1，macOS 14.7.5，Xcode 16.2，Swift 6.0.3
- Homebrew 在 `/opt/homebrew`；非交互 SSH 的 PATH 只有 `/usr/bin:/bin:/usr/sbin:/sbin`，脚本里要显式：
  ```bash
  export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin
  ```
- 仓库：`/Users/wenyutao/Documents/SpektraRoll`
- cmake 4.4.4、ffmpeg/ffprobe 9.0.2 已安装

## 构建

```bash
cd /Users/wenyutao/Documents/SpektraRoll
export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin
bash scripts/build-macos-ui.sh --no-open
open build-ui-macos/SpektraRoll.app
```

产物：`build-ui-macos/SpektraRoll.app`（约 167 MB，含 ffmpeg/ffprobe、CLI、data、字体）。

## SSH 远程验收的坑

1. **SSH 会话在 `Background` launchd domain**（`launchctl managername` 返回 `Background`）。
   - `open` 启动的 app 会出现在 `gui/501`，但 `screencapture` 从 Background 会话抓不到 GUI 窗口，只会截到桌面壁纸。
   - 不能因此判断 app 没启动；用 app 自己截自己的窗口。
2. **Stage Manager** 开着时，app 窗口可能在另一个 Stage，截屏/激活都不一定切过去。
   - 验收时可临时 `defaults write com.apple.WindowManager GloballyEnabled -bool false; killall WindowManager`，结束后恢复为 `true`。
3. 用 `CGWindowListCopyWindowInfo` 查窗口，不要用 `System Events`（会卡在辅助功能权限）。

## 调试开关（仅环境变量，正常启动不生效）

```bash
open -F \
  --env SPEKTRA_ALL_SPACES=1 \
  --env SPEKTRA_CAPTURE=1 \
  --env SPEKTRA_AUTOLOAD=/tmp/test.mp4 \
  --env SPEKTRA_AUTOPREVIEW=1 \
  build-ui-macos/SpektraRoll.app
```

| 变量 | 作用 |
|---|---|
| `SPEKTRA_ALL_SPACES=1` | 窗口设为浮动、所有 Space 可见、定时前置；用于 SSH 验收 |
| `SPEKTRA_CAPTURE=1` | 启动 6 秒后自截图到 `/tmp/spektra-self-0.png`；并监听 `/tmp/spektra-capture-now` 按需截图 |
| `SPEKTRA_AUTOLOAD=<path>` | 启动后自动导入一个视频并选中 |
| `SPEKTRA_AUTOPREVIEW=1` | 自动打开「胶片效果」渲染当前帧 |
| `SPEKTRA_AUTORENDER=1` | 自动渲染到 `~/Movies/SpektraRoll/spektra-debug-render*.mov` |

调试日志：`/tmp/spektra-debug.log`。

## D075 验收（macOS UI v1.1 P0）

```bash
cd /Users/wenyutao/Documents/SpektraRoll
export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin
scripts/build-macos-ui.sh --no-open
open build-ui-macos/SpektraRoll.app
```

1. 双击 `C0113.MP4`，暂停时应看到输入解码后的 sRGB 静帧（不是 Log 灰片），下方有直方图。
2. 滚轮/双指缩放、拖拽平移、`适合` / `100%`、双击切换。
3. 开 `胶片效果`，拖动 FILM / PRINT / ADVANCED 任意滑块，画面应自动刷新（180 ms 防抖）。
4. 点 `导出` 应跳到 OUTPUT。
5. 已知未做：播放中的实时胶片预览（D074 #3 / P1）；播放中关闭胶片效果仍是 AVPlayer 的 Log 画面。

## 已验证

- `.app` 启动、三栏、字体、导入缩略图、AVPlayer 播放、空格、INFO、胶片效果、渲染。
- 真实 `C0113.MP4`：3840×2160 / 50p / 312 帧 / 00:06；缩略图、播放、INFO、4K 胶片预览（`~/Library/Caches/SpektraRoll/preview/graded.png`）正常。
- 合成 3 秒测试片：渲染输出 640×360 / 3.00 s / H.264 / yuv420p。
- 字体：`NotoSansCJKsc-Regular/Bold.otf` 已打包（OFL）。

## 未完成 / 已知问题

- **OCIO 当前不可用**：
  - 初次 `brew install opencolorio` 被已有的 `/opt/homebrew/opt/cmake` 真实目录挡住（cmake link 冲突）。
  - 已把手动 cmake 备份到 `/opt/homebrew/opt/cmake-manual-4.4.4`，切到 Homebrew cmake 4.4.5；cmake 正常工作。
  - 但 macOS 14 上 Homebrew 没有 OCIO bottle，安装会源码构建 `python@3.15`，clang 报 “does not have support for efficient tail calling”，失败。
  - 首版用内置 10 种 Log 输入。后续可选：MacPorts、自建静态 OCIO、升级 macOS，或固定 python@3.13 的构建。
- 没有时间线拖动/逐帧预览、渲染队列、区间输入。
- `.app` 未公证；ffmpeg/OCIO 仍是 Homebrew 动态依赖，分发给别人前要静态链接/捆绑。
- R3D macOS 不支持（无插件）。
