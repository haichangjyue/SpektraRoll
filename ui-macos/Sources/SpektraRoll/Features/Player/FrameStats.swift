// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import Foundation

/// D074 #1: display-referred histogram plus the exposure references the
/// product owner asked for (dynamic range, black/white points, clipping).
/// The frame is downsampled to a small RGBA bitmap first, so this stays cheap
/// even for a 4K still.
struct FrameStats {
    static let binCount = 256

    let red: [Double]
    let green: [Double]
    let blue: [Double]
    let luma: [Double]
    let total: Int
    let lowPoint: Double
    let highPoint: Double
    /// Stops between the 0.1% and 99.9% luma percentiles, measured in linear
    /// light. Display-referred sRGB is decoded before the ratio is taken.
    let dynamicRange: Double
    let highlightClip: Double
    let shadowClip: Double

    var peak: Double {
        max(red.max() ?? 0, max(green.max() ?? 0, max(blue.max() ?? 0, 1)))
    }

    static func compute(from image: NSImage) -> FrameStats? {
        guard let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
            return nil
        }
        let sourceWidth = cgImage.width
        let sourceHeight = cgImage.height
        guard sourceWidth > 0, sourceHeight > 0 else { return nil }

        let maxSide = 720.0
        let scale = min(1.0, maxSide / Double(max(sourceWidth, sourceHeight)))
        let width = max(1, Int((Double(sourceWidth) * scale).rounded()))
        let height = max(1, Int((Double(sourceHeight) * scale).rounded()))
        let bytesPerRow = width * 4
        var pixels = [UInt8](repeating: 0, count: bytesPerRow * height)
        let colorSpace = CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB()
        let drawn = pixels.withUnsafeMutableBytes { buffer -> Bool in
            guard let context = CGContext(
                data: buffer.baseAddress,
                width: width,
                height: height,
                bitsPerComponent: 8,
                bytesPerRow: bytesPerRow,
                space: colorSpace,
                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
            ) else { return false }
            context.interpolationQuality = .low
            context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))
            return true
        }
        guard drawn else { return nil }

        var redBins = [Double](repeating: 0, count: binCount)
        var greenBins = [Double](repeating: 0, count: binCount)
        var blueBins = [Double](repeating: 0, count: binCount)
        var lumaBins = [Double](repeating: 0, count: binCount)
        var lumaValues = [Double]()
        lumaValues.reserveCapacity(width * height)
        var highlightCount = 0
        var shadowCount = 0
        var sampleCount = 0

        for y in 0..<height {
            let row = y * bytesPerRow
            for x in 0..<width {
                let offset = row + x * 4
                let r = Double(pixels[offset]) / 255.0
                let g = Double(pixels[offset + 1]) / 255.0
                let b = Double(pixels[offset + 2]) / 255.0
                let lumaValue = 0.2126 * r + 0.7152 * g + 0.0722 * b

                redBins[bin(r)] += 1
                greenBins[bin(g)] += 1
                blueBins[bin(b)] += 1
                lumaBins[bin(lumaValue)] += 1
                lumaValues.append(lumaValue)

                if max(r, g, b) >= 0.996 { highlightCount += 1 }
                if max(r, g, b) <= 0.004 { shadowCount += 1 }
                sampleCount += 1
            }
        }
        guard sampleCount > 0 else { return nil }

        lumaValues.sort()
        let low = percentile(lumaValues, 0.001)
        let high = percentile(lumaValues, 0.999)
        let lowLinear = max(srgbToLinear(low), 1.0 / 1000.0)
        let highLinear = max(srgbToLinear(high), lowLinear * 1.0001)
        let range = min(20.0, max(0.0, log2(highLinear / lowLinear)))

        return FrameStats(
            red: redBins,
            green: greenBins,
            blue: blueBins,
            luma: lumaBins,
            total: sampleCount,
            lowPoint: low,
            highPoint: high,
            dynamicRange: range,
            highlightClip: Double(highlightCount) / Double(sampleCount),
            shadowClip: Double(shadowCount) / Double(sampleCount)
        )
    }

    private static func bin(_ value: Double) -> Int {
        min(binCount - 1, max(0, Int((value * Double(binCount - 1)).rounded())))
    }

    private static func percentile(_ sorted: [Double], _ fraction: Double) -> Double {
        guard !sorted.isEmpty else { return 0 }
        let index = Int((Double(sorted.count - 1) * fraction).rounded())
        return sorted[min(sorted.count - 1, max(0, index))]
    }

    private static func srgbToLinear(_ value: Double) -> Double {
        let clamped = min(1.0, max(0.0, value))
        return clamped <= 0.04045
            ? clamped / 12.92
            : pow((clamped + 0.055) / 1.055, 2.4)
    }
}
