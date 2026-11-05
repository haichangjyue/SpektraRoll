// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import SwiftUI

/// D074 #1: compact histogram + exposure references under the video, above
/// the player controls.
struct HistogramView: View {
    let stats: FrameStats

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 12) {
                Text("直方图")
                    .font(Theme.micro)
                    .foregroundStyle(Theme.textMuted)
                Spacer(minLength: 8)
                metric("宽容度", String(format: "%.1f 档", stats.dynamicRange))
                metric("亮度", String(format: "%.2f–%.2f", stats.lowPoint, stats.highPoint))
                metric("高光裁切", String(format: "%.1f%%", stats.highlightClip * 100))
                metric("暗部裁切", String(format: "%.1f%%", stats.shadowClip * 100))
            }

            Canvas { context, size in
                let peak = stats.peak
                // 25 / 50 / 75% guides.
                for fraction in [CGFloat(0.25), 0.5, 0.75] {
                    var line = Path()
                    let x = size.width * fraction
                    line.move(to: CGPoint(x: x, y: 0))
                    line.addLine(to: CGPoint(x: x, y: size.height))
                    context.stroke(line, with: .color(Theme.divider.opacity(0.55)), lineWidth: 0.5)
                }

                context.blendMode = .screen
                draw(stats.red, color: Theme.error, peak: peak, in: &context, size: size)
                draw(stats.green, color: Theme.success, peak: peak, in: &context, size: size)
                draw(stats.blue, color: Theme.info, peak: peak, in: &context, size: size)
                context.blendMode = .normal

                context.fill(
                    areaPath(stats.luma, peak: peak, size: size),
                    with: .color(Theme.primary.opacity(0.08))
                )
            }
            .frame(height: 34)
            .background(
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Theme.surfaceSunken)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .stroke(Theme.border, lineWidth: 1)
            )
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .background(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .fill(Theme.backgroundElevated.opacity(0.94))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(Theme.border, lineWidth: 1)
        )
    }

    private func metric(_ label: String, _ value: String) -> some View {
        HStack(spacing: 3) {
            Text(label)
                .foregroundStyle(Theme.textDim)
            Text(value)
                .foregroundStyle(Theme.text)
                .monospacedDigit()
        }
        .font(Theme.micro)
    }

    private func draw(_ bins: [Double], color: Color, peak: Double,
                      in context: inout GraphicsContext, size: CGSize) {
        let path = areaPath(bins, peak: peak, size: size)
        context.fill(path, with: .color(color.opacity(0.16)))
        context.stroke(path, with: .color(color.opacity(0.9)), lineWidth: 1)
    }

    private func areaPath(_ bins: [Double], peak: Double, size: CGSize) -> Path {
        var path = Path()
        path.move(to: CGPoint(x: 0, y: size.height))
        guard bins.count > 1, peak > 0 else {
            path.addLine(to: CGPoint(x: size.width, y: size.height))
            path.closeSubpath()
            return path
        }
        for index in bins.indices {
            let x = size.width * CGFloat(index) / CGFloat(bins.count - 1)
            let level = min(1.0, bins[index] / peak)
            let y = size.height * (1.0 - CGFloat(level))
            path.addLine(to: CGPoint(x: x, y: y))
        }
        path.addLine(to: CGPoint(x: size.width, y: size.height))
        path.closeSubpath()
        return path
    }
}
