// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

struct EngineCapabilities: Codable {
    struct Build: Codable {
        let ocio: Bool
        let redLoader: Bool
        let d3d11: Bool
        let metal: Bool
        let exr: Bool
        let exrZip: Bool
    }

    struct Ocio: Codable {
        let available: Bool
        let version: String
    }

    struct Red: Codable {
        let built: Bool
        let available: Bool
        let sdkVersion: String
        let message: String
    }

    struct Gpu: Codable {
        let backend: String
        let spectralAvailable: Bool
        let textureAvailable: Bool
        let error: String
    }

    struct Preview: Codable {
        let protocolVersion: Int
        let frameTransport: String
        let commands: [String]

        enum CodingKeys: String, CodingKey {
            case protocolVersion = "protocol"
            case frameTransport
            case commands
        }
    }

    let version: String
    let platform: String
    let arch: String
    let build: Build
    let ocio: Ocio
    let red: Red
    let gpu: Gpu
    let engines: [String]
    let textureEngines: [String]
    let inputs: [String]
    let scans: [String]
    let codecs: [String]
    let progress: [String]
    let preview: Preview
    let commands: [String]
    let dataRoots: [String]
}

struct StockCatalog: Codable {
    struct Counts: Codable {
        let films: Int
        let papers: Int
    }

    struct Stock: Codable, Identifiable, Hashable {
        let id: String
        let name: String
        let path: String
        let type: String
        let support: String
        let stage: String
        let referenceIlluminant: String
        let viewingIlluminant: String
        let targetPrint: String
        let use: String
        let antihalation: String
        let positive: Bool
        let printFilm: Bool
    }

    let version: String
    let root: String
    let counts: Counts
    let films: [Stock]
    let papers: [Stock]
}

struct OcioInfo: Codable {
    struct Display: Codable {
        let name: String
        let views: [String]
    }

    let version: String
    let config: String
    let configFile: String
    let linearSpace: String
    let colorSpaces: [String]
    let displays: [Display]
}

struct RenderEvent: Decodable {
    let event: String
    let frame: Int?
    let total: Int?
    let percent: Double?
    let width: Int?
    let height: Int?
    let fps: Double?
    let totalFrames: Int?
    let segmented: Bool?
    let engine: String?
    let codec: String?
    let output: String?
    let frames: Int?
    let parts: Int?
    let elapsedMs: Double?
}

struct ProbeData {
    var width: Int = 0
    var height: Int = 0
    var fps: Double = 0
    var duration: Double = 0
    var frames: Int = 0
    var codec: String = ""
    var colorSpace: String = ""
    var colorTransfer: String = ""
    var colorPrimaries: String = ""
    var timecode: String = ""
}
