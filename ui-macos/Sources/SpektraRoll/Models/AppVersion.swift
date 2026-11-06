// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

/// D083: the release channel lives in `CFBundleShortVersionString`
/// (`0.0.1-alpha.1`). INFO only shows it in the "关于" panel; the engine
/// section no longer repeats it.
enum AppVersion {
    static let marketing: String = {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0.0.1-alpha.1"
    }()

    static let buildNumber: String = {
        Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1"
    }()

    static var display: String { marketing }
    static var fullDisplay: String { "\(marketing) (\(buildNumber))" }

    static let author = "haichangjyue"
    static let basedOn = "spektrafilm"

    static let sourceURL = URL(string: "https://github.com/haichangjyue/SpektraRoll")!
    static let issuesURL = URL(string: "https://github.com/haichangjyue/SpektraRoll/issues")!
    static let upstreamURL = URL(string: "https://github.com/andreavolpato/spektrafilm")!
}
