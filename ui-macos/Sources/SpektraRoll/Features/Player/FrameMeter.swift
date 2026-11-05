// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

/// D079: a light-weight "配光" (print this frame) meter. SpektraLab's Process
/// button asks the engine to solve the frame's auto-exposure and neutral
/// enlarger filter pack. SpektraRoll's neutral C/M/Y pack is already chosen by
/// the film/paper database, so the missing half is auto-exposure: place the
/// frame's median on 18% linear grey before the film chain.
enum FrameMeter {
    static let targetLinear = 0.18
    static let minEV = -3.0
    static let maxEV = 3.0

    static func exposureEV(from stats: FrameStats) -> Double {
        let linear = srgbToLinear(stats.medianLuma)
        let ev = log2(targetLinear / max(linear, 1e-6))
        return min(maxEV, max(minEV, ev))
    }

    private static func srgbToLinear(_ value: Double) -> Double {
        let clamped = min(1.0, max(0.0, value))
        return clamped <= 0.04045
            ? clamped / 12.92
            : pow((clamped + 0.055) / 1.055, 2.4)
    }
}
