// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

/// D082: a frozen render-queue entry. The settings are a value snapshot, so
/// editing the inspector after enqueueing never mutates a pending job.
struct RenderJob: Identifiable, Codable, Equatable {
    enum Status: String, Codable {
        case waiting
        case running
        case done
        case failed
        case canceled

        var title: String {
            switch self {
            case .waiting: return "等待中"
            case .running: return "渲染中"
            case .done: return "完成"
            case .failed: return "失败"
            case .canceled: return "已取消"
            }
        }
    }

    let id: UUID
    var assetID: MediaAsset.ID
    var assetPath: String
    var assetName: String
    var settings: LookSettings
    var outputPath: String
    var status: Status = .waiting
    var progress: Double = 0
    var message: String = ""

    init(asset: MediaAsset, settings: LookSettings, output: URL) {
        id = UUID()
        assetID = asset.id
        assetPath = asset.path
        assetName = asset.displayName
        self.settings = settings
        outputPath = output.path
    }

    var outputURL: URL { URL(fileURLWithPath: outputPath) }
}
