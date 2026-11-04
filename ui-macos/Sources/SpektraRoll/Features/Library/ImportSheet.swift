// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct ImportSheet: View {
    @EnvironmentObject private var app: AppState
    @Environment(\.dismiss) private var dismiss

    let draft: ImportDraft

    @State private var mode: ImportMode = .transform
    @State private var input: InputTransform = .sLog3
    @State private var ocioInput: String = ""

    init(draft: ImportDraft) {
        self.draft = draft
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            VStack(alignment: .leading, spacing: 5) {
                Text("导入素材")
                    .font(Theme.heading)
                    .foregroundStyle(Theme.text)
                Text("\(draft.urls.count) 个文件 · 输入方式会应用到全部素材")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textMuted)
            }

            VStack(alignment: .leading, spacing: 7) {
                Text("输入方式")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textMuted)
                Picker("输入方式", selection: $mode) {
                    ForEach(ImportMode.allCases) { mode in
                        Text(mode.title).tag(mode)
                    }
                }
                .labelsHidden()
                .pickerStyle(.segmented)
            }

            if mode == .transform {
                VStack(alignment: .leading, spacing: 7) {
                    Text("画面类型")
                        .font(Theme.caption)
                        .foregroundStyle(Theme.textMuted)
                    Picker("画面类型", selection: $input) {
                        ForEach(InputTransform.allCases) { transform in
                            Text("\(transform.title) · \(transform.detail)").tag(transform)
                        }
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
            } else if app.ocioAvailable {
                VStack(alignment: .leading, spacing: 7) {
                    Text("色彩空间")
                        .font(Theme.caption)
                        .foregroundStyle(Theme.textMuted)
                    Picker("色彩空间", selection: $ocioInput) {
                        ForEach(app.ocioColorSpaces, id: \.self) { space in
                            Text(space).tag(space)
                        }
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    Text("配置文件由后台自动选择：\(app.ocioInfo?.config ?? "ACES 2.0 studio")")
                        .font(Theme.micro)
                        .foregroundStyle(Theme.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
            } else {
                Text("这个构建没有 OpenColorIO；请选择“画面类型”。")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.warning)
            }

            HStack {
                Spacer()
                Button("取消") {
                    dismiss()
                }
                .buttonStyle(PillButtonStyle(filled: false))
                Button("导入") {
                    app.confirmImport(draft, mode: mode, input: input, ocioInput: ocioInput)
                    dismiss()
                }
                .buttonStyle(PillButtonStyle())
                .disabled(mode == .ocio && (!app.ocioAvailable || app.ocioColorSpaces.isEmpty))
            }
        }
        .padding(24)
        .frame(width: 480)
        .background(Theme.backgroundElevated)
        .onAppear {
            input = app.settings.input
            if let first = app.ocioColorSpaces.first(where: { $0.contains("S-Log3") })
                ?? app.ocioColorSpaces.first {
                ocioInput = first
            }
        }
    }
}
