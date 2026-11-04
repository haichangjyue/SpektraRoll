// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

struct MediaAsset: Identifiable, Codable, Hashable {
    var id: UUID = UUID()
    var path: String
    var name: String
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
    var input: InputTransform = .sLog3
    var ocioEnabled: Bool = false
    var ocioInput: String = ""
    var isR3D: Bool = false
    var thumbnailPath: String? = nil

    var url: URL { URL(fileURLWithPath: path) }

    var displayName: String {
        name.isEmpty ? url.deletingPathExtension().lastPathComponent : name
    }

    var resolutionText: String {
        guard width > 0, height > 0 else { return "—" }
        return "\(width)×\(height)"
    }

    var fpsText: String {
        guard fps > 0 else { return "—" }
        return String(format: "%.2f fps", fps)
    }

    var durationText: String {
        guard duration > 0 else { return "—" }
        let total = Int(duration.rounded())
        let hours = total / 3600
        let minutes = (total % 3600) / 60
        let seconds = total % 60
        if hours > 0 {
            return String(format: "%d:%02d:%02d", hours, minutes, seconds)
        }
        return String(format: "%02d:%02d", minutes, seconds)
    }

    var framesText: String {
        frames > 0 ? "\(frames) 帧" : "—"
    }

    var inputDescription: String {
        if ocioEnabled {
            return ocioInput.isEmpty ? "OCIO" : ocioInput
        }
        return input.title
    }

    var isPlayable: Bool {
        !isR3D
    }
}
