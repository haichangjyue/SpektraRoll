// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import Darwin
import Foundation
import Metal

/// D077 #4: the machine running the UI, shown at the bottom of INFO.
/// The macOS build says "Mac"; the `#else` branch says "PC" for a future
/// Windows shell.
struct SystemProfile {
    let platformName: String
    let osVersion: String
    let screen: String
    let cpu: String
    let gpu: String
    let ram: String
    let storage: String
    let hdr: String
    let colorSpace: String

    static func current() -> SystemProfile {
        let screen = NSScreen.main
        let scale = screen?.backingScaleFactor ?? 1
        let pixelWidth = Int((screen?.frame.width ?? 0) * scale)
        let pixelHeight = Int((screen?.frame.height ?? 0) * scale)
        let screenName = screen?.localizedName ?? "—"
        let screenCount = NSScreen.screens.count
        let screenText = screen == nil
            ? "—"
            : "\(screenName) · \(pixelWidth)×\(pixelHeight) · \(Int(scale))x"
                + (screenCount > 1 ? " · \(screenCount) 屏" : "")

        let hdrHeadroom = max(
            screen?.maximumPotentialExtendedDynamicRangeColorComponentValue ?? 1,
            screen?.maximumReferenceExtendedDynamicRangeColorComponentValue ?? 1
        )
        let hdrText = hdrHeadroom > 1.01
            ? String(format: "开启（EDR %.2fx）", hdrHeadroom)
            : "关闭（SDR）"

        let storageText = storageDescription()
        let cpuName = sysctlString("machdep.cpu.brand_string")
            ?? sysctlString("hw.model")
            ?? "—"
        let gpuName = MTLCreateSystemDefaultDevice()?.name ?? "—"

        return SystemProfile(
            platformName: platformName,
            osVersion: osVersion,
            screen: screenText,
            cpu: cpuName,
            gpu: gpuName,
            ram: ByteCountFormatter.string(
                fromByteCount: Int64(ProcessInfo.processInfo.physicalMemory),
                countStyle: .memory
            ),
            storage: storageText,
            hdr: hdrText,
            colorSpace: screen?.colorSpace?.localizedName ?? "—"
        )
    }

    private static var platformName: String {
        #if os(macOS)
        return "Mac"
        #else
        return "PC"
        #endif
    }

    private static var osVersion: String {
        #if os(macOS)
        let version = ProcessInfo.processInfo.operatingSystemVersion
        return "macOS \(version.majorVersion).\(version.minorVersion).\(version.patchVersion)"
        #else
        return ProcessInfo.processInfo.operatingSystemVersionString
        #endif
    }

    private static func storageDescription() -> String {
        let home = URL(fileURLWithPath: NSHomeDirectory())
        let values = try? home.resourceValues(
            forKeys: [.volumeTotalCapacityKey, .volumeAvailableCapacityForImportantUsageKey]
        )
        let total = Int64(values?.volumeTotalCapacity ?? 0)
        let free = values?.volumeAvailableCapacityForImportantUsage ?? 0
        guard total > 0 else { return "—" }
        let formatter = ByteCountFormatter()
        formatter.countStyle = .file
        return "\(formatter.string(fromByteCount: free)) 可用 / \(formatter.string(fromByteCount: total))"
    }

    private static func sysctlString(_ name: String) -> String? {
        var size = 0
        guard sysctlbyname(name, nil, &size, nil, 0) == 0, size > 0 else { return nil }
        var buffer = [CChar](repeating: 0, count: size)
        guard sysctlbyname(name, &buffer, &size, nil, 0) == 0 else { return nil }
        return String(cString: buffer).trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
