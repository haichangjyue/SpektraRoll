// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

enum InputTransform: String, CaseIterable, Codable, Identifiable {
    case srgb
    case appleLog = "apple-log"
    case sLog3 = "s-log3"
    case sLog3SGamut3 = "s-log3-sgamut3"
    case cLog3 = "c-log3"
    case vLog = "v-log"
    case logC3 = "logc3"
    case logC4 = "logc4"
    case bmdFilm = "bmd-film"
    case davinci

    var id: String { rawValue }

    var title: String {
        switch self {
        case .srgb: return "sRGB"
        case .appleLog: return "Apple Log"
        case .sLog3: return "Sony S-Log3"
        case .sLog3SGamut3: return "Sony S-Log3（S-Gamut3）"
        case .cLog3: return "Canon C-Log3"
        case .vLog: return "Panasonic V-Log"
        case .logC3: return "ARRI LogC3"
        case .logC4: return "ARRI LogC4"
        case .bmdFilm: return "Blackmagic Film Gen 5"
        case .davinci: return "DaVinci Intermediate"
        }
    }

    var detail: String {
        switch self {
        case .srgb: return "Rec.709"
        case .appleLog: return "BT.2020"
        case .sLog3: return "S-Gamut3.Cine"
        case .sLog3SGamut3: return "S-Gamut3"
        case .cLog3: return "Cinema Gamut"
        case .vLog: return "V-Gamut"
        case .logC3: return "Wide Gamut 3"
        case .logC4: return "Wide Gamut 4"
        case .bmdFilm: return "Blackmagic Wide Gamut"
        case .davinci: return "DaVinci Wide Gamut"
        }
    }

    /// Untagged Sony XAVC still needs an explicit container matrix. These are
    /// the defaults the UI uses when the clip has no color_space tag.
    var defaultMatrix: String {
        self == .appleLog ? "bt2020" : "bt709"
    }
}

enum RenderEngine: String, CaseIterable, Codable, Identifiable {
    case lut
    case spectral
    case spectralGpu = "spectral-gpu"

    var id: String { rawValue }

    var title: String {
        switch self {
        case .lut: return "LUT（快）"
        case .spectral: return "光谱 CPU（物理）"
        case .spectralGpu: return "光谱 GPU（物理，快）"
        }
    }
}

enum ScanMode: String, CaseIterable, Codable, Identifiable {
    case negative
    case print
    case slide

    var id: String { rawValue }

    var title: String {
        switch self {
        case .negative: return "扫描负片"
        case .print: return "扫描印片"
        case .slide: return "反转片直扫"
        }
    }
}

enum TransferFunction: String, CaseIterable, Codable, Identifiable {
    case srgb
    case pq
    case hlg

    var id: String { rawValue }

    var title: String {
        switch self {
        case .srgb: return "SDR"
        case .pq: return "HDR PQ"
        case .hlg: return "HDR HLG"
        }
    }
}

enum VideoCodec: String, CaseIterable, Codable, Identifiable {
    case h264
    case hevc
    case prores

    var id: String { rawValue }

    var title: String {
        switch self {
        case .h264: return "H.264"
        case .hevc: return "HEVC Main10"
        case .prores: return "ProRes 422 HQ"
        }
    }
}

enum TextureEngine: String, CaseIterable, Codable, Identifiable {
    case auto
    case cpu
    case gpu

    var id: String { rawValue }
    var title: String { rawValue.uppercased() }
}

enum SpreadModel: String, CaseIterable, Codable, Identifiable {
    case physical
    case box

    var id: String { rawValue }
    var title: String { self == .physical ? "物理（微米）" : "盒状（旧观感）" }
}

enum LutInterpolation: String, CaseIterable, Codable, Identifiable {
    case tetra
    case tri

    var id: String { rawValue }
    var title: String { self == .tetra ? "四面体" : "三线性" }
}

enum FrameGauge: String, CaseIterable, Codable, Identifiable {
    case super8
    case mm16
    case super16
    case mm35
    case super35
    case mm65
    case imax

    var id: String { rawValue }

    var title: String {
        switch self {
        case .super8: return "Super 8"
        case .mm16: return "16mm"
        case .super16: return "Super 16"
        case .mm35: return "35mm"
        case .super35: return "Super 35"
        case .mm65: return "65mm"
        case .imax: return "IMAX"
        }
    }

    /// Camera-negative long side, matching SpektraLab's `FilmFrame` data
    /// (Super 8 5.79, 16mm 10.26, Super 16 12.52, Super 35 24.89, 65mm 52.63).
    /// 35mm keeps the 36 mm full-frame/135 width and IMAX the 70 mm width.
    var widthMM: Double {
        switch self {
        case .super8: return 5.79
        case .mm16: return 10.26
        case .super16: return 12.52
        case .mm35: return 36.0
        case .super35: return 24.89
        case .mm65: return 52.63
        case .imax: return 70.0
        }
    }

    var isCine: Bool {
        switch self {
        case .super8, .mm16, .super16, .mm35, .super35, .mm65, .imax:
            return true
        }
    }

    var widthText: String {
        String(format: "%.2f mm", widthMM)
    }

    var menuTitle: String {
        isCine ? "\(title)  ·  CINE" : title
    }
}

struct LookSettings: Codable, Equatable {
    var engine: RenderEngine = .lut
    var input: InputTransform = .sLog3
    var ocioEnabled: Bool = false
    var ocioInput: String = "S-Log3 S-Gamut3.Cine"
    var filmID: String = "kodak_portra_400"
    var paperID: String = "kodak_portra_endura"
    var scan: ScanMode = .negative
    var frameGauge: FrameGauge = .mm35
    var exposure: Double = 0.7
    var contrast: Double = 1.15
    var couplers: Bool = false
    var gamut: Bool = true
    var grain: Double = 0.4
    var grainRate: Int = 3
    var halation: Double = 0.45
    var antiHalation: Bool = false
    var glare: Double = 0.2
    var diffusion: Double = 0.1
    var spreadModel: SpreadModel = .physical
    var textureEngine: TextureEngine = .auto
    var trc: TransferFunction = .srgb
    var codec: VideoCodec = .h264
    var crf: Int = 16
    var preset: String = "medium"
    var interp: LutInterpolation = .tetra
    var strength: Double = 1.0
    var threads: Int = 0
    var start: Double = 0
    var frames: Int? = nil
    var segment: Double = 0
    var jobs: Int = 0
    var resume: Bool = false

    /// v1: the engine has no separate anti-halation parameter. Turning the
    /// layer on scales halation down; this is a UI-level approximation until a
    /// real parameter exists (documented in docs/ui-design-macos.md).
    var effectiveHalation: Double {
        antiHalation ? halation * 0.6 : halation
    }

    static func number(_ value: Double) -> String {
        String(format: "%.4f", value)
    }
}
