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
| `SPEKTRA_AUTOPLAY=1` | 启动 3.5 秒后自动播放（配合 `AUTOLOAD`/`AUTOPREVIEW` 测播放代理） |
| `SPEKTRA_AUTOGRADE=1` | 启动后自动执行一次「配光」（需配合 `AUTOLOAD`） |
| `SPEKTRA_PLAYER_DEBUG=1` | 播放源切换写入 `/tmp/spektra-player.log`（original/display/film） |
| `SPEKTRA_TAB=info|film|print|advanced|output` | 启动时直接打开指定检查器 tab |
| `SPEKTRA_PROXY_LIMIT_GB=5` | 本次启动临时覆盖代理缓存上限（测试用） |
| `SPEKTRA_AUTORENDER=1` | 自动渲染到 `~/Movies/SpektraRoll/spektra-debug-render*.mov` |
| `SPEKTRA_SEEK=<秒>` | 启动 4 秒后暂停并跳到该时间，刷新静帧（D082 时间线验收） |
| `SPEKTRA_QUEUE_SELFTEST=1` | 自动入队 2 条小任务并运行队列，结果写 `/tmp/spektra-queue.log`（D082） |
| `SPEKTRA_PROXY_DIR=<path>` | 启动时把代理缓存迁移到该目录并持久化（D082 缓存迁移测试） |

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

## D076 验收（播放代理）

```bash
launchctl setenv SPEKTRA_AUTOLOAD /Users/wenyutao/Movies/C0113.MP4
launchctl setenv SPEKTRA_AUTOPREVIEW 1
launchctl setenv SPEKTRA_AUTOPLAY 1
launchctl setenv SPEKTRA_PLAYER_DEBUG 1
open -n build-ui-macos/SpektraRoll.app
```

- 观察 `~/Library/Caches/SpektraRoll/preview-cache/`：先出现 `*.mov`（显示代理），播放后出现胶片代理。
- `pgrep -fl spektra-roll-cli` 应能看到 `--display --max-side 960` 或 `--max-side 960` 的代理进程。
- `cat /tmp/spektra-player.log` 应看到 `switch display ...`，胶片代理就绪后再看到 `switch film ...`。
- 验收完清除调试变量：

```bash
launchctl unsetenv SPEKTRA_AUTOLOAD
launchctl unsetenv SPEKTRA_AUTOPREVIEW
launchctl unsetenv SPEKTRA_AUTOPLAY
launchctl unsetenv SPEKTRA_PLAYER_DEBUG
```

## D077 验收（缓存上限 / 暂停播放 / 系统信息）

**缓存上限**：在 `~/Library/Caches/SpektraRoll/preview-cache/` 放几个 2 MB 假 `.mov`，用

```bash
open -F --env SPEKTRA_PROXY_LIMIT_GB=0.005 --env SPEKTRA_TAB=advanced -n build-ui-macos/SpektraRoll.app
```

启动后旧假代理应被删除，ADVANCED 显示「当前占用 / 5 GB」。

**生成代理不播放**：

```bash
open -F --env SPEKTRA_AUTOLOAD=/Users/wenyutao/Movies/C0113.MP4 \
  --env SPEKTRA_AUTOPREVIEW=1 --env SPEKTRA_AUTOPLAY=1 \
  --env SPEKTRA_PLAYER_DEBUG=1 -n build-ui-macos/SpektraRoll.app
cat /tmp/spektra-player.log
```

应依次看到 `block film proxy` → `switch film ... play=false` → `unblock film proxy play=true`，生成期间不出现 `switch display`。

**系统信息**：INFO 底部应出现「Mac」面板；用 `SPEKTRA_TAB=info` 截图检查。

**缓存目录**：ADVANCED「预览代理」→「选择」改目录；新代理写入新目录，旧文件不迁移。

## D079 验收（配光）

```bash
open -F --env SPEKTRA_AUTOLOAD=/Users/wenyutao/Movies/C0113.MP4 \
  --env SPEKTRA_AUTOGRADE=1 --env SPEKTRA_TAB=advanced -n build-ui-macos/SpektraRoll.app
```

- 配光按钮在 `导出` 右侧；完成后状态栏应显示 `配光：+0.66 EV（中性滤色片自动）`。
- ADVANCED 曝光滑块应同步到 `+0.66 EV`，胶片效果自动打开并刷新。
- 调试日志/CLI 参数里 `--exposure 0.6626` 可确认测光结果。

## D080 验收（OpenColorIO）

```bash
scripts/build-ocio-macos.sh          # 静态 OCIO 2.6.0 -> ~/Documents/ocio-2.6.0-static
scripts/build-macos-ui.sh --no-open  # 自动检测并静态链接
```

```bash
build-ui-macos/SpektraRoll.app/Contents/Helpers/spektra-roll-cli capabilities --json
# "build": {"ocio": true}, "ocio": {"available": true, "version": "2.6.0"}

build-ui-macos/SpektraRoll.app/Contents/Helpers/spektra-roll-cli lut bake \
  --ocio-config data/ocio/studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio \
  --ocio-input "S-Log3 S-Gamut3.Cine" --size 17 --out /tmp/ocio-test.cube
otool -L build-ui-macos/SpektraRoll.app/Contents/Helpers/spektra-roll-cli | grep -i -E 'ocio|imath|yaml|pystring'
# 应没有这些 Homebrew dylib 依赖
```

- `SPEKTRA_OCIO=1` 启动时自动启用 OCIO 并选 `S-Log3 S-Gamut3.Cine`，用于截图验收。
- `--engine spectral-gpu` + OCIO 已接通（D081）：OCIO 在 CPU 上批量解码后交给 Metal 光谱着色器；`--exr-linear` 遇到 OCIO 显示变换时写显示前线性 Rec.709（D081）。
- 默认前缀可用 `OCIO_PREFIX=...` 覆盖。

## 已验证

- `.app` 启动、三栏、字体、导入缩略图、AVPlayer 播放、空格、INFO、胶片效果、渲染。
- 真实 `C0113.MP4`：3840×2160 / 50p / 312 帧 / 00:06；缩略图、播放、INFO、4K 胶片预览（`~/Library/Caches/SpektraRoll/preview/graded.png`）正常。
- 合成 3 秒测试片：渲染输出 640×360 / 3.00 s / H.264 / yuv420p。
- 字体：`NotoSansCJKsc-Regular/Bold.otf` 已打包（OFL）。

## 未完成 / 已知问题

- **OCIO 已可用（D080）**：静态 OCIO 2.6.0 前缀（`~/Documents/ocio-2.6.0-static`），`scripts/build-ocio-macos.sh` 可重建；bundle 的 CLI `ocio.available=true`，`ocio-info` / `lut bake --ocio-input` / UI OCIO 下拉已验证。
  - D081 起：`spectral-gpu` 的 OCIO 已接通，`--exr-linear` 的 OCIO 显示变换已改为写显示前线性。
  - Homebrew 的 `opencolorio` 在 macOS 14 仍会尝试构建 `python@3.15`，所以用静态脚本，不走 Homebrew 包。
- D082 起已有时间线拖动/逐帧预览、渲染队列、区间输入和 Y/M 滤色片偏移；队列与最终渲染共用一个 CLI 进程。
- 播放代理是 LUT 近似（360–720 px），不是逐像素光谱；暂停后的静帧仍是全质量。
- `.app` 未公证；ffmpeg 仍是 Homebrew 动态依赖，OCIO 已静态链接；分发前要处理 ffmpeg 捆绑。
- R3D macOS 不支持（无插件）。
