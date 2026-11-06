// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

/// D078: the INFO panel shows the app build separately from the CLI engine.
/// D083: the first public preview is labelled "预览版 1"; drop `previewLabel`
/// (or set it to nil) for a final release.
enum AppVersion {
    static let marketing: String = {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0.1.0"
    }()

    static let buildNumber: String = {
        Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1"
    }()

    static let previewLabel: String? = "1"

    static var display: String {
        if let previewLabel {
            return "预览版 \(previewLabel) · \(marketing)"
        }
        return marketing
    }

    static var fullDisplay: String {
        "\(display) (\(buildNumber))"
    }

    static let sourceURL = URL(string: "https://github.com/haichangjyue/SpektraRoll")!
    static let issuesURL = URL(string: "https://github.com/haichangjyue/SpektraRoll/issues")!
}
