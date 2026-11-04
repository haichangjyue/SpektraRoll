# SpektraRoll · UI 功能对照表（D072）

> 目的：Windows Qt UI 和 macOS SwiftUI UI 不再靠“感觉”同步。**功能是否完成，以这张表为准**。
>
> 状态：✅ 已完成 / 🟡 部分（D073：SwiftUI 代码已写，Mac 编译/验收待做）/ ⬜ 计划 / ❌ 本期不做
>
> 最后更新：2026-11-05（D073：macOS SwiftUI 三栏骨架已写）
>
> **注意**：D073 的 SwiftUI 代码还没有在 Mac 上编译过；表中标 🟡 的 macOS 项表示“代码已写、待 Mac/CI 验证”，不是已验收。

## 1. 功能矩阵

| 功能 | CLI | Qt Windows | SwiftUI macOS v1 | 备注 |
|---|---|---|---|---|
| 引擎握手 `capabilities --json` | ✅ | ⬜ 待接入 | 🟡 已写 | D072 Phase 0 已提供 CLI |
| 片库 `catalog --json` | ✅ | ✅（自己扫描 JSON） | 🟡 已写 | SwiftUI 必须走 CLI，不再重写规则 |
| 素材探测（ffprobe） | ✅ | ✅ | 🟡 已写 | 信息条：分辨率/帧率/时长/帧数/timecode |
| 输入类型 9 种 Log | ✅ | ✅ | 🟡 已写 | 与 `ui-schema.json` 的 `input` 对齐 |
| OCIO 配置 + 色彩空间下拉 | ✅ | ✅ | 🟡 已写 | 名字来自 `ocio-info --json` |
| 片库：20 底片 / 8 相纸印片 | ✅ | ✅ | 🟡 已写 | 反转片标记、电影印片标记 |
| 选胶片自动推荐相纸/印片 | ✅ | ✅ | 🟡 已写 | 用 `catalog.targetPrint` |
| 选反转片自动切直扫 | ✅ | ✅ | 🟡 已写 | 停用相纸 |
| LUT 烘焙 + 配方签名 | ✅ | ✅ | 🟡 已写 | UI 烘的 LUT 必须与 CLI 逐位相同 |
| 常驻预览 `preview` | ✅ | ✅ | 🟡 已写 | protocol 2；SwiftUI v1 用暂停单帧，不是常驻 |
| 时间线拖动预览 | ✅ | ✅ | ⬜ | 短片内存缓存；长片 960 px 代理 |
| 适合 / 100% / 缩放平移 | — | ✅ | ⬜ | SwiftUI v1 未做缩放 |
| 直方图 | — | ✅ | ⬜ | 可先由 UI 从 RGBA 计算；后续加 `histogram` 协议 |
| 按住看原片 | ✅ | ✅ | ⬜ | `raw` 命令；SwiftUI v1 未接 |
| 预览尺寸/耗时状态 | ✅ | ✅ | 🟡 已写 | 状态栏文案统一 |
| 单帧渲染 / 整段渲染 | ✅ | ✅ | 🟡 已写 | `video` |
| 区间 / 帧数 / 时长 | ✅ | ✅ | ⬜ | SwiftUI v1 暂未暴露区间 |
| 编码 H.264 / HEVC / ProRes | ✅ | ✅ | 🟡 已写 | HDR 自动 HEVC；R3D 自动 ProRes |
| 音频 / timecode / 旋转 | ✅ | ✅ | 🟡 已写 | 音频只听最终片，不逐段编码 |
| 分段 / 续渲 / 并行 | ✅ | ✅ | ⬜ | SwiftUI v1 未暴露 |
| 渲染进度 JSON | ✅ | ⬜ 待接入 | 🟡 已写 | `--progress json`；Qt 仍在正则解析文本 |
| 取消渲染 | ✅ | ✅ | 🟡 已写 | 杀进程 + 清理临时文件 |
| 渲染日志 | ✅ | ✅ | 🟡 已写 | 文本原样进日志；`{` 行进进度 |
| 队列持久化/恢复 | — | ✅ | ⬜ | v1 不做队列；后续按同一 JSON schema |
| 批量拖入入队 | — | ✅ | ⬜ | v1 不做 |
| 设置持久化 | — | ✅ | 🟡 已写 | `~/Library/Application Support/SpektraRoll` |
| R3D 单条预览/渲染 | ✅ | ✅ | ❌ | macOS v1 无插件 → 明确禁用 |
| R3D 批量入队 | — | ❌ | ❌ | 已有明确提示；后续单独做 |
| 深浅色外观 | — | 仅深色 | 🟡 深色 | macOS v1 只做深色品牌 |
| 无障碍 / VoiceOver | — | 🟡 | ⬜ | SwiftUI 优先补齐 |
| 中英本地化 | — | 仅中文 | 🟡 仅中文 | schema 已带 zh/en 标签 |
| 未签名 `.app` 本地验收 | — | — | 🟡 脚本已写 | 待 Mac 上首次跑 |
| 公证 / DMG | — | — | ⬜ | 需要 Apple Developer Program |
| CI 编译 UI | — | ⬜（当前 CI 不编 Qt） | 🟡 已加 `swift build` | 首次推送后验证 |
| 自动更新 | — | ❌ | ❌ | 首版不做 |

## 2. v1 范围（macOS）

**做：**
- 打开/探测素材、片库、参数面板、烘焙、预览、渲染、队列、OCIO、日志/错误、设置。
- 与 Qt 相同的 CLI 参数和默认值（以 `data/ui-schema.json` 为准）。
- 未签名 `.app`，双击可运行；不要求 Mac App Store。

**不做：**
- macOS R3D 插件（单独项目）；UI 显示“R3D 仅 Windows”。
- 裁切、印片机 Y/M/预闪/光源、扫描仪 unsharp、空间物理模型。
- Mac App Store / 沙盒 / 自动更新 / 直接链接 C++ 核心。
- 4K 实时预览。

## 3. 设计一致性规则

1. **设计源是 `design/tokens.json`，不是任一 UI 的代码。**
2. **品牌一致 + 平台自适应**：颜色、间距、字号、圆角、布局网格一致；窗口装饰、菜单、文件对话框、滚动条用平台原生。
3. **允许 1–2 px 差异**：字体渲染、控件高度、系统阴影不可能完全一致；不要为此写死像素。
4. **不做像素级克隆**：窗口交通灯、macOS 菜单栏、Windows 标题栏各自保留。
5. **图标统一一套 SVG**（Lucide/Phosphor，MIT/Apache），不要混用平台图标字体。
6. **字体用系统字体**：Windows Segoe UI，macOS SF Pro；除非品牌字体有明确授权并随包分发。

## 4. 防漂移五道闸

### 4.1 参数唯一来源
`data/ui-schema.json` 定义 id、标签、类型、默认值、范围、CLI flag、显示条件。两个 UI 都读取它；新增参数先改 schema，再改 UI。

### 4.2 引擎契约
- `capabilities --json`：能做什么。
- `catalog --json`：片库是什么。
- `--progress json`：渲染到哪了。
- `preview` protocol 2：预览怎么走。
- UI 不解析人类文本（除了日志展示）。

### 4.3 Golden argv
同一份设置 JSON，两个 UI 生成的 CLI 参数数组必须一致。建议测试向量：

```json
{
  "engine": "spectral-gpu",
  "input": "s-log3",
  "film": "kodak_vision3_250d",
  "paper": "kodak_2383",
  "scan": "print",
  "exposure": 0.7,
  "contrast": 1.0,
  "couplers": true,
  "gamut": "off",
  "grain": 0.4,
  "halation": 0.45,
  "glare": 0.2,
  "diffusion": 0.1,
  "spreadModel": "physical",
  "trc": "srgb",
  "codec": "prores"
}
```

期望 argv（顺序可不同，集合必须相同）：

```text
--engine spectral-gpu --input s-log3
--film data/profiles/kodak_vision3_250d.json
--paper data/profiles/kodak_2383.json
--scan print --exposure 0.7 --contrast 1 --couplers on --gamut off
--grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1
--spread-model physical --trc srgb --codec prores
```

### 4.4 队列/预设格式
两个 UI 共用一个 JSON schema；macOS 存的队列，Windows 能打开（反之亦然）。最小字段：

```json
{
  "version": 1,
  "items": [
    {
      "id": "uuid",
      "input": "/abs/clip.mov",
      "output": "/abs/out.mov",
      "settings": { "parameter ids from ui-schema": "values" },
      "argv": ["--engine", "lut", "..."],
      "state": "queued|running|done|failed|cancelled",
      "progress": 0.0,
      "startedAt": "ISO-8601",
      "finishedAt": "ISO-8601",
      "error": ""
    }
  ]
}
```

> `argv` 是冻结后的最终参数；`settings` 用于 UI 回显。恢复队列时优先用 `argv`，避免默认值变化导致“入队后被改参”。

### 4.5 CI 双 UI
- Windows job：编 Qt UI（至少 `cmake --build`），跑 `--stocks-selftest` / `--preview-selftest`。
- macOS job：编 SwiftUI app，跑 `--selftest`（引擎 + 2 帧渲染），检查 bundle 无 Homebrew 依赖。
- 任一 UI 编译失败 → CI 红。当前 CI 还没编 Qt，这是 D072 要补的债。

## 5. 完成定义（DoD）

一个新功能只有满足以下之一才算完成：

- **跨平台功能**：CLI + Qt + SwiftUI 都实现，且两边验收脚本通过；或
- **平台限定功能**：在本表明确标 `❌` 并写明原因（例如 R3D 在 macOS v1 不支持）。

没有“默认只在 Windows 做”的第三种状态。

## 6. 里程碑

| 阶段 | 内容 | 验收 |
|---|---|---|
| M5 | macOS SwiftUI v1 | 双击 `.app`：选素材 → 预览 → 烘 LUT → 渲染 → 队列 |
| M5.5 | 设计冻结 + 规格抽取 | `design/tokens.json` 定稿；两种窗口尺寸截图；`ui-parity.md` 更新 |
| M6 | Windows UI v3（Qt） | 按 macOS 设计规格重做；并排截图；队列/预设互通 |
| M6.5 | 一致性审计 | 截图 diff；golden argv；CI 双 UI 编译 |
