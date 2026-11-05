// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

/// D078: the INFO panel shows the app build separately from the CLI engine.
/// Every internal (unreleased) build carries its test tag; bump `testTag` with
/// each round. When the product is released, set `isReleased = true` and the
/// tag disappears.
enum AppVersion {
    static let marketing: String = {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0.1.0"
    }()

    static let testTag = "D079"
    static let isReleased = false

    static var display: String {
        isReleased ? marketing : "内部测试（\(testTag)） · \(marketing)"
    }
}
