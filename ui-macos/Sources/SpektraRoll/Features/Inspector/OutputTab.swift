// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct OutputTab: View {
    @EnvironmentObject private var app: AppState
    @EnvironmentObject private var player: PlayerController

    private let presets = ["ultrafast", "fast", "medium", "slow", "veryslow"]

    private var waitingJobs: Int {
        app.renderJobs.filter { $0.status == .waiting }.count
    }

    private var usesRange: Binding<Bool> {
        Binding(
            get: { app.settings.frames != nil },
            set: { enabled in
                app.settings.frames = enabled
                    ? max(1, app.selectedAsset?.frames ?? 1)
                    : nil
            }
        )
    }

    private var frameCount: Binding<Int> {
        Binding(
            get: { app.settings.frames ?? max(1, app.selectedAsset?.frames ?? 1) },
            set: { app.settings.frames = max(1, $0) }
        )
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            SectionTitle("OUTPUT")

            Panel {
                VStack(alignment: .leading, spacing: 13) {
                    HStack {
                        Text("编码")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Spacer()
                        Picker("编码", selection: $app.settings.codec) {
                            ForEach(VideoCodec.allCases) { codec in
                                Text(codec.title).tag(codec)
                            }
                        }
                        .labelsHidden()
                        .pickerStyle(.menu)
                        .frame(width: 170)
                    }
                    HStack {
                        Text("传输")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Spacer()
                        Picker("传输", selection: $app.settings.trc) {
                            ForEach(TransferFunction.allCases) { trc in
                                Text(trc.title).tag(trc)
                            }
                        }
                        .labelsHidden()
                        .pickerStyle(.segmented)
                        .frame(width: 220)
                    }

                    if app.settings.codec != .prores {
                        HStack {
                            Text("CRF")
                                .font(Theme.caption)
                                .foregroundStyle(Theme.textMuted)
                            Spacer()
                            Stepper("", value: $app.settings.crf, in: 0...51)
                                .labelsHidden()
                                .controlSize(.small)
                            Text("\(app.settings.crf)")
                                .font(Theme.caption)
                                .monospacedDigit()
                                .foregroundStyle(Theme.text)
                                .frame(width: 26, alignment: .trailing)
                        }
                        HStack {
                            Text("预设")
                                .font(Theme.caption)
                                .foregroundStyle(Theme.textMuted)
                            Spacer()
                            Picker("预设", selection: $app.settings.preset) {
                                ForEach(presets, id: \.self) { preset in
                                    Text(preset).tag(preset)
                                }
                            }
                            .labelsHidden()
                            .pickerStyle(.menu)
                            .frame(width: 130)
                        }
                    }
                }
            }

            SectionTitle("文件")
            Panel {
                VStack(alignment: .leading, spacing: 12) {
                    HStack(spacing: 8) {
                        Text("目录")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Text(app.outputDirectory.path)
                            .font(Theme.micro)
                            .foregroundStyle(Theme.text)
                            .lineLimit(1)
                            .truncationMode(.middle)
                        Spacer(minLength: 4)
                        Button("选择") {
                            app.chooseOutputDirectory()
                        }
                        .buttonStyle(PillButtonStyle(filled: false))
                    }
                    HStack(spacing: 8) {
                        Text("文件名")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        TextField("文件名", text: $app.outputName)
                            .textFieldStyle(.plain)
                            .font(Theme.caption)
                            .padding(.horizontal, 9)
                            .frame(height: 28)
                            .background(
                                RoundedRectangle(cornerRadius: 5, style: .continuous)
                                    .fill(Theme.field)
                            )
                            .overlay(
                                RoundedRectangle(cornerRadius: 5, style: .continuous)
                                    .stroke(Theme.border, lineWidth: 1)
                            )
                    }
                }
            }

            SectionTitle("区间")
            Panel {
                VStack(alignment: .leading, spacing: 12) {
                    HStack(spacing: 8) {
                        Text("起点")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        TextField("0", value: $app.settings.start,
                                  format: .number.precision(.fractionLength(0...3)))
                            .textFieldStyle(.plain)
                            .multilineTextAlignment(.trailing)
                            .font(Theme.caption)
                            .padding(.horizontal, 8)
                            .frame(width: 78, height: 26)
                            .background(
                                RoundedRectangle(cornerRadius: 5, style: .continuous)
                                    .fill(Theme.field)
                            )
                            .overlay(
                                RoundedRectangle(cornerRadius: 5, style: .continuous)
                                    .stroke(Theme.border, lineWidth: 1)
                            )
                        Text("秒")
                            .font(Theme.micro)
                            .foregroundStyle(Theme.textDim)
                        Spacer(minLength: 4)
                        Button("当前帧") {
                            app.settings.start = max(0, player.currentTime)
                        }
                        .buttonStyle(CompactPillButtonStyle())
                        Button("整段") {
                            app.settings.start = 0
                            app.settings.frames = nil
                        }
                        .buttonStyle(CompactPillButtonStyle())
                    }

                    ToggleRow(
                        label: "只渲染区间",
                        isOn: usesRange,
                        detail: "关闭 = 从起点到片尾；起点会自动带上相机 timecode。"
                    )

                    if app.settings.frames != nil {
                        HStack(spacing: 8) {
                            Text("帧数")
                                .font(Theme.caption)
                                .foregroundStyle(Theme.textMuted)
                            TextField("1", value: frameCount, format: .number)
                                .textFieldStyle(.plain)
                                .multilineTextAlignment(.trailing)
                                .font(Theme.caption)
                                .padding(.horizontal, 8)
                                .frame(width: 78, height: 26)
                                .background(
                                    RoundedRectangle(cornerRadius: 5, style: .continuous)
                                        .fill(Theme.field)
                                )
                                .overlay(
                                    RoundedRectangle(cornerRadius: 5, style: .continuous)
                                        .stroke(Theme.border, lineWidth: 1)
                                )
                            if let asset = app.selectedAsset, asset.fps > 0 {
                                let seconds = Double(app.settings.frames ?? 0) / asset.fps
                                Text(String(format: "约 %.2f 秒", seconds))
                                    .font(Theme.micro)
                                    .foregroundStyle(Theme.textDim)
                            }
                            Spacer(minLength: 4)
                            Button("到片尾") {
                                guard let asset = app.selectedAsset, asset.fps > 0 else { return }
                                let remaining = max(0, asset.duration - app.settings.start)
                                app.settings.frames = max(1, Int((remaining * asset.fps).rounded()))
                            }
                            .buttonStyle(CompactPillButtonStyle())
                        }
                    }
                }
            }

            if app.selectedAsset != nil {
                VStack(alignment: .leading, spacing: 10) {
                    HStack(spacing: 10) {
                        Button {
                            app.render()
                        } label: {
                            Text(app.isRendering ? "渲染中…" : "渲染")
                                .frame(minWidth: 72)
                        }
                        .buttonStyle(PillButtonStyle())
                        .disabled(app.isRendering || app.isQueueRunning)

                        if app.isRendering {
                            Button("取消") {
                                app.cancelRender()
                            }
                            .buttonStyle(PillButtonStyle(filled: false))
                        }
                        Spacer()
                        Text(app.renderStatus)
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                            .lineLimit(1)
                    }

                    if app.isRendering || app.renderProgress > 0 {
                        ProgressView(value: app.renderProgress)
                            .tint(Theme.primary)
                    }

                    if !app.renderLog.isEmpty {
                        ScrollView {
                            Text(app.renderLog.suffix(80).joined(separator: "\n"))
                                .font(.system(size: 10, design: .monospaced))
                                .foregroundStyle(Theme.textMuted)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .textSelection(.enabled)
                                .padding(10)
                        }
                        .frame(height: 150)
                        .background(
                            RoundedRectangle(cornerRadius: 6, style: .continuous)
                                .fill(Theme.surfaceSunken)
                        )
                        .overlay(
                            RoundedRectangle(cornerRadius: 6, style: .continuous)
                                .stroke(Theme.border, lineWidth: 1)
                        )
                    }
                }
            } else {
                Text("先在左侧导入并选择一个素材。")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textDim)
            }

            SectionTitle("渲染队列", trailing: waitingJobs > 0 ? "\(waitingJobs) 等待" : nil)
            Panel {
                VStack(alignment: .leading, spacing: 12) {
                    HStack(spacing: 8) {
                        Button("加入队列") {
                            app.enqueueRender()
                        }
                        .buttonStyle(PillButtonStyle(filled: false))
                        .disabled(app.isRendering && !app.isQueueRunning)

                        Button(app.isQueueRunning ? "停止队列" : "开始队列") {
                            if app.isQueueRunning {
                                app.stopRenderQueue()
                            } else {
                                app.startRenderQueue()
                            }
                        }
                        .buttonStyle(PillButtonStyle(filled: app.isQueueRunning))
                        .disabled(!app.isQueueRunning && waitingJobs == 0)

                        Button("清空已完成") {
                            app.clearFinishedRenderJobs()
                        }
                        .buttonStyle(CompactPillButtonStyle())
                        Spacer()
                    }

                    if app.renderJobs.isEmpty {
                        Text("队列是空的。把当前参数冻结成一条任务后，可以连续渲染多条。")
                            .font(Theme.micro)
                            .foregroundStyle(Theme.textDim)
                            .fixedSize(horizontal: false, vertical: true)
                    } else {
                        ForEach(app.renderJobs) { job in
                            queueRow(job)
                        }
                    }
                }
            }
        }
        .onChange(of: app.settings.trc) { _, trc in
            if trc != .srgb, app.settings.codec == .h264 {
                app.settings.codec = .hevc
            }
        }
    }

    private func queueRow(_ job: RenderJob) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            HStack(spacing: 8) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(job.assetName)
                        .font(Theme.caption)
                        .foregroundStyle(Theme.text)
                        .lineLimit(1)
                    Text(job.outputURL.lastPathComponent)
                        .font(Theme.micro)
                        .foregroundStyle(Theme.textDim)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
                Spacer(minLength: 4)
                Text(job.status.title)
                    .font(Theme.micro)
                    .foregroundStyle(job.status == .failed ? Theme.warning : Theme.textMuted)
                if job.status != .running {
                    Button {
                        app.removeRenderJob(job.id)
                    } label: {
                        Image(systemName: "xmark")
                            .font(.system(size: 9, weight: .bold))
                    }
                    .buttonStyle(.plain)
                    .foregroundStyle(Theme.textDim)
                }
            }
            if job.status == .running || job.progress > 0 {
                ProgressView(value: job.progress)
                    .tint(Theme.primary)
                    .controlSize(.small)
            }
            if !job.message.isEmpty {
                Text(job.message)
                    .font(Theme.micro)
                    .foregroundStyle(Theme.textDim)
                    .lineLimit(2)
            }
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .background(
            RoundedRectangle(cornerRadius: 6, style: .continuous)
                .fill(Theme.surfaceRaised)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 6, style: .continuous)
                .stroke(Theme.border, lineWidth: 1)
        )
    }
}
