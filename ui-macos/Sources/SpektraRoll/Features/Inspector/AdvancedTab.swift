// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct AdvancedTab: View {
    @EnvironmentObject private var app: AppState

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            SectionTitle("ADVANCED")

            panelRow("引擎") {
                Picker("引擎", selection: $app.settings.engine) {
                    ForEach(RenderEngine.allCases) { engine in
                        Text(engine.title).tag(engine)
                    }
                }
                .labelsHidden()
                .pickerStyle(.menu)
                .frame(width: 180)
            }

            Panel {
                VStack(alignment: .leading, spacing: 13) {
                    SliderRow(label: "曝光", value: $app.settings.exposure,
                              range: -3...3, step: 0.05, format: "%+.2f", unit: " EV")
                    ToggleRow(
                        label: "DIR 耦合剂",
                        isOn: $app.settings.couplers,
                        detail: "Apple Log 默认开启；负片/印片按需要切换。"
                    )
                    SliderRow(label: "强度", value: $app.settings.strength,
                              range: 0...1, step: 0.01)
                }
            }

            SectionTitle("GPU / LUT")
            Panel {
                VStack(alignment: .leading, spacing: 13) {
                    HStack {
                        Text("GPU 质感")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Spacer()
                        Picker("GPU 质感", selection: $app.settings.textureEngine) {
                            ForEach(TextureEngine.allCases) { engine in
                                Text(engine.title).tag(engine)
                            }
                        }
                        .labelsHidden()
                        .pickerStyle(.menu)
                        .frame(width: 130)
                    }
                    HStack {
                        Text("LUT 插值")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Spacer()
                        Picker("LUT 插值", selection: $app.settings.interp) {
                            ForEach(LutInterpolation.allCases) { interp in
                                Text(interp.title).tag(interp)
                            }
                        }
                        .labelsHidden()
                        .pickerStyle(.menu)
                        .frame(width: 130)
                    }
                    HStack {
                        Text("线程")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Spacer()
                        Stepper("", value: $app.settings.threads, in: 0...64)
                            .labelsHidden()
                            .controlSize(.small)
                        Text(app.settings.threads == 0 ? "自动" : "\(app.settings.threads)")
                            .font(Theme.caption)
                            .monospacedDigit()
                            .foregroundStyle(Theme.text)
                            .frame(width: 36, alignment: .trailing)
                    }
                }
            }

            SectionTitle("预览代理")
            Panel {
                VStack(alignment: .leading, spacing: 12) {
                    HStack(spacing: 8) {
                        Text("磁盘上限")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Spacer()
                        TextField("5", value: $app.proxyCacheLimitGB,
                                  format: .number.precision(.fractionLength(0...2)))
                            .textFieldStyle(.plain)
                            .multilineTextAlignment(.trailing)
                            .font(Theme.caption)
                            .padding(.horizontal, 8)
                            .frame(width: 66, height: 26)
                            .background(
                                RoundedRectangle(cornerRadius: 5, style: .continuous)
                                    .fill(Theme.field)
                            )
                            .overlay(
                                RoundedRectangle(cornerRadius: 5, style: .continuous)
                                    .stroke(Theme.border, lineWidth: 1)
                            )
                            .onSubmit {
                                app.proxyCacheLimitChanged()
                            }
                        Text("GB")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                    }

                    HStack(spacing: 8) {
                        Text("缓存位置")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Text(app.proxyCacheDirectory.path)
                            .font(Theme.micro)
                            .foregroundStyle(Theme.text)
                            .lineLimit(1)
                            .truncationMode(.middle)
                        Spacer(minLength: 4)
                        Button("选择") {
                            app.chooseProxyCacheDirectory()
                        }
                        .buttonStyle(PillButtonStyle(filled: false))
                    }

                    HStack(spacing: 8) {
                        Text("当前占用")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Text(app.proxyCacheUsageText)
                            .font(Theme.micro)
                            .foregroundStyle(Theme.text)
                            .monospacedDigit()
                        Spacer(minLength: 4)
                        Button("清理") {
                            app.clearProxyCache()
                        }
                        .buttonStyle(PillButtonStyle(filled: false))
                    }

                    Text("超过上限时自动删除最旧的代理；0 = 不限制。")
                        .font(Theme.micro)
                        .foregroundStyle(Theme.textDim)
                }
            }

            SectionTitle("色彩管理")
            Panel {
                VStack(alignment: .leading, spacing: 10) {
                    ToggleRow(
                        label: "使用 OCIO 输入",
                        isOn: $app.settings.ocioEnabled,
                        detail: app.ocioAvailable
                            ? "配置由后台自动选择：\(app.ocioInfo?.config ?? "ACES 2.0 studio")"
                            : "这个构建没有 OpenColorIO。"
                    )
                    .disabled(!app.ocioAvailable)

                    if app.settings.ocioEnabled, app.ocioAvailable {
                        HStack {
                            Text("输入空间")
                                .font(Theme.caption)
                                .foregroundStyle(Theme.textMuted)
                            Spacer()
                            Picker("输入空间", selection: $app.settings.ocioInput) {
                                ForEach(app.ocioColorSpaces, id: \.self) { space in
                                    Text(space).tag(space)
                                }
                            }
                            .labelsHidden()
                            .pickerStyle(.menu)
                            .frame(width: 190)
                        }
                        InfoRow(label: "线性空间", value: app.ocioInfo?.linearSpace ?? "—")
                    }
                }
            }
        }
        .onChange(of: app.proxyCacheLimitGB) { _, _ in
            app.proxyCacheLimitChanged()
        }
    }

    private func panelRow<Trailing: View>(_ label: String,
                                          @ViewBuilder trailing: () -> Trailing) -> some View {
        HStack {
            Text(label)
                .font(Theme.caption)
                .foregroundStyle(Theme.textMuted)
            Spacer()
            trailing()
        }
    }
}
