// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import CoreText
import SwiftUI

extension Color {
    init(hex: String) {
        var value: UInt64 = 0
        let cleaned = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        Scanner(string: cleaned).scanHexInt64(&value)
        let red = Double((value >> 16) & 0xFF) / 255.0
        let green = Double((value >> 8) & 0xFF) / 255.0
        let blue = Double(value & 0xFF) / 255.0
        self.init(.sRGB, red: red, green: green, blue: blue, opacity: 1.0)
    }
}

enum FontBook {
    private static var didRegister = false

    /// Registers Source Han Sans / Noto Sans SC if the build script copied them
    /// into Contents/Resources/Fonts. Falls back to system fonts when absent.
    static func registerBundledFonts() {
        guard !didRegister else { return }
        didRegister = true
        for ext in ["otf", "ttf", "ttc"] {
            let urls = Bundle.main.urls(forResourcesWithExtension: ext, subdirectory: "Fonts") ?? []
            for url in urls {
                CTFontManagerRegisterFontsForURL(url as CFURL, .process, nil)
            }
        }
    }

    static let sourceHanSans = "Source Han Sans SC"
    static let notoSansSC = "Noto Sans SC"
    static let notoSansCJKSC = "Noto Sans CJK SC"
    static let pingFangSC = "PingFang SC"

    static func font(size: CGFloat, weight: Font.Weight = .regular) -> Font {
        if NSFont(name: sourceHanSans, size: size) != nil {
            return .custom(sourceHanSans, size: size).weight(weight)
        }
        if NSFont(name: notoSansSC, size: size) != nil {
            return .custom(notoSansSC, size: size).weight(weight)
        }
        if NSFont(name: notoSansCJKSC, size: size) != nil {
            return .custom(notoSansCJKSC, size: size).weight(weight)
        }
        if NSFont(name: "NotoSansCJKsc-Regular", size: size) != nil {
            return .custom("NotoSansCJKsc-Regular", size: size).weight(weight)
        }
        if NSFont(name: pingFangSC, size: size) != nil {
            return .custom(pingFangSC, size: size).weight(weight)
        }
        return .system(size: size, weight: weight)
    }
}

enum Theme {
    // Keep these values in sync with design/tokens.json (D073). The plan is to
    // generate this file from the tokens once the macOS design is frozen.
    static let background = Color(hex: "0A0A0B")
    static let backgroundElevated = Color(hex: "0F0F11")
    static let surface = Color(hex: "121214")
    static let surfaceRaised = Color(hex: "17171A")
    static let surfaceSunken = Color(hex: "0D0D0F")
    static let field = Color(hex: "1C1C1F")
    static let fieldHover = Color(hex: "242428")
    static let divider = Color(hex: "242428")
    static let border = Color(hex: "2E2E33")
    static let borderStrong = Color(hex: "3A3A40")
    static let text = Color(hex: "F4F4F5")
    static let textMuted = Color(hex: "9B9BA3")
    static let textDim = Color(hex: "66666E")
    static let primary = Color(hex: "FFFFFF")
    static let primaryHover = Color(hex: "E9E9E9")
    static let primaryText = Color(hex: "000000")
    static let success = Color(hex: "7BC08A")
    static let warning = Color(hex: "D9A441")
    static let error = Color(hex: "E06C60")
    static let info = Color(hex: "8BB4D4")
    static let scrim = Color(hex: "000000")

    static func font(_ size: CGFloat, weight: Font.Weight = .regular) -> Font {
        FontBook.font(size: size, weight: weight)
    }

    static let micro = font(10)
    static let caption = font(11)
    static let small = font(12)
    static let body = font(13)
    static let title = font(14, weight: .bold)
    static let heading = font(18, weight: .bold)
    static let display = font(24, weight: .bold)
}
