// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

extension String {
    /// Stable across launches (unlike Swift's seeded `hashValue`), used for LUT
    /// cache names.
    var fnv1aHex: String {
        var hash: UInt64 = 1469598103934665603
        for byte in utf8 {
            hash ^= UInt64(byte)
            hash = hash &* 1099511628211
        }
        return String(hash, radix: 16)
    }
}

extension LookSettings {
    func inputArguments(ocioConfig: URL?) -> [String] {
        if ocioEnabled, let ocioConfig, !ocioInput.isEmpty {
            return ["--ocio-config", ocioConfig.path, "--ocio-input", ocioInput]
        }
        return ["--input", input.rawValue]
    }

    func filmArguments(filmPath: URL, paperPath: URL?, filterPath: URL?) -> [String] {
        var args = ["--film", filmPath.path]
        if scan != .slide, let paperPath {
            args += ["--paper", paperPath.path]
        }
        if let filterPath {
            args += ["--filters", filterPath.path]
        }
        args += ["--scan", scan.rawValue]
        return args
    }

    var lookArguments: [String] {
        [
            "--exposure", Self.number(exposure),
            "--contrast", Self.number(contrast),
            "--couplers", couplers ? "on" : "off",
            "--gamut", gamut ? "oklch" : "off"
        ]
    }

    /// `lut bake` and `lut apply --engine spectral` take `--hdr`; `video`
    /// takes `--trc` instead.
    var bakeHdrArguments: [String] {
        trc == .srgb ? [] : ["--hdr", trc.rawValue]
    }

    var textureArguments: [String] {
        [
            "--grain", Self.number(grain),
            "--grain-rate", "\(grainRate)",
            "--halation", Self.number(effectiveHalation),
            "--glare", Self.number(glare),
            "--diffusion", Self.number(diffusion),
            "--spread-model", spreadModel.rawValue,
            "--frame-width-mm", Self.number(frameGauge.widthMM)
        ]
    }

    var bakeCacheKey: String {
        [
            filmID, paperID, scan.rawValue, input.rawValue,
            ocioEnabled ? "ocio" : "builtin", ocioInput,
            Self.number(exposure), Self.number(contrast),
            couplers ? "couplers" : "nocouplers",
            gamut ? "oklch" : "gamutoff",
            trc.rawValue
        ].joined(separator: "|").fnv1aHex
    }

    func matrix(for asset: MediaAsset) -> String {
        let tag = asset.colorSpace
        if tag.hasPrefix("bt2020") { return "bt2020" }
        switch tag {
        case "bt709", "smpte170m", "bt470bg", "smpte240m":
            return tag
        default:
            return input.defaultMatrix
        }
    }

    func bakeArguments(filmPath: URL, paperPath: URL?, filterPath: URL?, ocioConfig: URL?,
                       output: URL) -> [String] {
        var args = ["lut", "bake", "--out", output.path]
        args += filmArguments(filmPath: filmPath, paperPath: paperPath, filterPath: filterPath)
        args += inputArguments(ocioConfig: ocioConfig)
        args += lookArguments
        args += bakeHdrArguments
        args += ["--size", "33"]
        if threads > 0 { args += ["--threads", "\(threads)"] }
        return args
    }

    func videoArguments(asset: MediaAsset, filmPath: URL, paperPath: URL?, filterPath: URL?,
                        ocioConfig: URL?, lutPath: URL?, output: URL) -> [String] {
        var args = ["video", "--in", asset.path, "--out", output.path]
        if engine == .lut, let lutPath {
            args += ["--lut", lutPath.path]
        } else {
            args += ["--engine", engine.rawValue]
            args += filmArguments(filmPath: filmPath, paperPath: paperPath, filterPath: filterPath)
            args += inputArguments(ocioConfig: ocioConfig)
            args += lookArguments
        }
        args += ["--trc", trc.rawValue]
        args += ["--codec", codec.rawValue]
        if codec != .prores {
            args += ["--crf", "\(crf)", "--preset", preset]
        }
        args += textureArguments
        if textureEngine != .auto {
            args += ["--texture-engine", textureEngine.rawValue]
        }
        args += ["--in-matrix", matrix(for: asset)]
        args += ["--progress", "json"]
        if start > 0 { args += ["--start", Self.number(start)] }
        if let frames, frames > 0 { args += ["--frames", "\(frames)"] }
        if segment > 0 { args += ["--segment", Self.number(segment)] }
        if resume { args += ["--resume"] }
        if jobs > 0 { args += ["--jobs", "\(jobs)"] }
        if threads > 0 { args += ["--threads", "\(threads)"] }
        return args
    }

    func stillArguments(asset: MediaAsset, filmPath: URL, paperPath: URL?, filterPath: URL?,
                        ocioConfig: URL?, lutPath: URL?, inputImage: URL,
                        output: URL) -> [String] {
        var args: [String]
        if engine == .lut, let lutPath {
            args = ["lut", "apply", "--lut", lutPath.path]
        } else {
            args = ["lut", "apply", "--engine", engine.rawValue]
            args += filmArguments(filmPath: filmPath, paperPath: paperPath, filterPath: filterPath)
            args += inputArguments(ocioConfig: ocioConfig)
            args += lookArguments
            args += bakeHdrArguments
        }
        args += ["--in", inputImage.path, "--out", output.path, "--bits", "8"]
        args += ["--interp", interp.rawValue]
        args += textureArguments
        if textureEngine != .auto {
            args += ["--texture-engine", textureEngine.rawValue]
        }
        if strength < 0.999 {
            args += ["--strength", Self.number(strength)]
        }
        if threads > 0 { args += ["--threads", "\(threads)"] }
        return args
    }
}
