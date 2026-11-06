// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

/// Shown once on first launch (D083). The app is an early alpha, so the
/// warning is deliberately explicit; the small print carries the project
/// links and attribution.
struct AlphaNoticeSheet: View {
    let onDismiss: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            HStack(spacing: 10) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundStyle(Theme.warning)
                Text("早期测试版本")
                    .font(Theme.font(20, weight: .bold))
                    .foregroundStyle(Theme.text)
            }

            Text("这是 SpektraRoll \(AppVersion.display) 的早期测试版本。许多功能仍处于实验阶段，未来版本可能有较大改动，请勿用于正式生产。")
                .font(Theme.body)
                .foregroundStyle(Theme.textMuted)
                .fixedSize(horizontal: false, vertical: true)

            Hairline()

            VStack(alignment: .leading, spacing: 5) {
                Text("SpektraRoll 是基于 spektrafilm 的开源胶片与印片模拟器。")
                Text("作者：\(AppVersion.author) · 许可证：GPL-3.0-or-later")
            }
            .font(Theme.micro)
            .foregroundStyle(Theme.textDim)
            .fixedSize(horizontal: false, vertical: true)

            VStack(alignment: .leading, spacing: 4) {
                Link("github.com/haichangjyue/SpektraRoll", destination: AppVersion.sourceURL)
                Link("github.com/andreavolpato/spektrafilm", destination: AppVersion.upstreamURL)
            }
            .font(Theme.micro)
            .tint(Theme.info)

            HStack {
                Spacer()
                Button("我知道了", action: onDismiss)
                    .buttonStyle(PillButtonStyle())
                    .keyboardShortcut(.defaultAction)
            }
        }
        .padding(28)
        .frame(width: 520)
        .background(Theme.backgroundElevated)
    }
}
