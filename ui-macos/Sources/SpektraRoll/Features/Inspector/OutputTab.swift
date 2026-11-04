// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct OutputTab: View {
    @EnvironmentObject private var app: AppState

    private let presets = ["ultrafast", "fast", "medium", "slow", "veryslow"]

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
                        .disabled(app.isRendering)

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
        }
        .onChange(of: app.settings.trc) { _, trc in
            if trc != .srgb, app.settings.codec == .h264 {
                app.settings.codec = .hevc
            }
        }
    }
}
