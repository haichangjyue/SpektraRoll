// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct PillButtonStyle: ButtonStyle {
    var filled: Bool = true

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(Theme.font(13, weight: .semibold))
            .foregroundStyle(filled ? Theme.primaryText : Theme.text)
            .padding(.horizontal, 16)
            .frame(height: 32)
            .background(
                Capsule(style: .continuous)
                    .fill(filled ? Theme.primary : Theme.surfaceRaised)
            )
            .overlay(
                Capsule(style: .continuous)
                    .stroke(filled ? Color.clear : Theme.border, lineWidth: 1)
            )
            .opacity(configuration.isPressed ? 0.78 : 1.0)
            .scaleEffect(configuration.isPressed ? 0.985 : 1.0)
            .animation(.easeOut(duration: 0.12), value: configuration.isPressed)
    }
}

struct SectionTitle: View {
    let text: String
    var trailing: String? = nil

    init(_ text: String, trailing: String? = nil) {
        self.text = text
        self.trailing = trailing
    }

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text(text)
                .font(Theme.font(14, weight: .bold))
                .foregroundStyle(Theme.text)
            Spacer(minLength: 0)
            if let trailing {
                Text(trailing)
                    .font(Theme.micro)
                    .foregroundStyle(Theme.textDim)
                    .monospacedDigit()
            }
        }
    }
}

struct Hairline: View {
    var body: some View {
        Rectangle()
            .fill(Theme.divider)
            .frame(height: 1)
    }
}

struct SliderRow: View {
    let label: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    var step: Double = 0.01
    var format: String = "%.2f"
    var unit: String = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            HStack(spacing: 6) {
                Text(label)
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textMuted)
                Spacer(minLength: 0)
                Text(String(format: format, value) + unit)
                    .font(Theme.caption)
                    .monospacedDigit()
                    .foregroundStyle(Theme.text)
            }
            Slider(value: $value, in: range, step: step)
                .tint(Theme.primary)
                .controlSize(.small)
        }
    }
}

struct ToggleRow: View {
    let label: String
    @Binding var isOn: Bool
    var detail: String? = nil

    var body: some View {
        Toggle(isOn: $isOn) {
            VStack(alignment: .leading, spacing: 2) {
                Text(label)
                    .font(Theme.caption)
                    .foregroundStyle(Theme.text)
                if let detail {
                    Text(detail)
                        .font(Theme.micro)
                        .foregroundStyle(Theme.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
        .toggleStyle(.switch)
        .tint(Theme.primary)
        .controlSize(.small)
    }
}

struct TagBadge: View {
    let text: String

    var body: some View {
        Text(text)
            .font(Theme.font(9, weight: .semibold))
            .foregroundStyle(Theme.textMuted)
            .padding(.horizontal, 6)
            .padding(.vertical, 2)
            .background(Capsule().fill(Theme.surfaceRaised))
            .overlay(Capsule().stroke(Theme.border, lineWidth: 1))
    }
}

struct EmptyStateView: View {
    let title: String
    let subtitle: String
    var icon: String = "square.dashed"

    var body: some View {
        VStack(spacing: 10) {
            Image(systemName: icon)
                .font(.system(size: 22, weight: .light))
                .foregroundStyle(Theme.textDim)
            Text(title)
                .font(Theme.font(13, weight: .semibold))
                .foregroundStyle(Theme.textMuted)
            Text(subtitle)
                .font(Theme.micro)
                .foregroundStyle(Theme.textDim)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(20)
        .frame(maxWidth: .infinity)
        .background(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .fill(Theme.surfaceSunken)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .strokeBorder(style: StrokeStyle(lineWidth: 1, dash: [5, 4]))
                .foregroundStyle(Theme.border)
        )
    }
}

struct Panel<Content: View>: View {
    private let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        content
            .padding(16)
            .background(
                RoundedRectangle(cornerRadius: 8, style: .continuous)
                    .fill(Theme.backgroundElevated)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 8, style: .continuous)
                    .stroke(Theme.border, lineWidth: 1)
            )
    }
}
