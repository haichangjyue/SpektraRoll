// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import SwiftUI

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var timer: Timer?
    private var captureTimer: Timer?
    private var attempts = 0

    private func debugLog(_ message: String) {
        let url = URL(fileURLWithPath: "/tmp/spektra-debug.log")
        let line = message + "\n"
        if let handle = try? FileHandle(forWritingTo: url) {
            handle.seekToEndOfFile()
            handle.write(line.data(using: .utf8) ?? Data())
            handle.closeFile()
        } else {
            try? line.write(to: url, atomically: true, encoding: .utf8)
        }
    }

    private func captureWindows() {
        for (index, window) in NSApp.windows.enumerated() {
            let windowID = CGWindowID(window.windowNumber)
            if let image = CGWindowListCreateImage(
                .null,
                .optionIncludingWindow,
                windowID,
                [.boundsIgnoreFraming, .bestResolution]
            ) {
                let rep = NSBitmapImageRep(cgImage: image)
                if let data = rep.representation(using: .png, properties: [:]) {
                    try? data.write(to: URL(fileURLWithPath: "/tmp/spektra-self-\(index).png"))
                }
            }
            if let view = window.contentView,
               let rep = view.bitmapImageRepForCachingDisplay(in: view.bounds) {
                view.cacheDisplay(in: view.bounds, to: rep)
                if let data = rep.representation(using: .png, properties: [:]) {
                    try? data.write(to: URL(fileURLWithPath: "/tmp/spektra-view-\(index).png"))
                }
            }
        }
    }

    private func startCaptureWatcher() {
        captureTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            let path = "/tmp/spektra-capture-now"
            guard FileManager.default.fileExists(atPath: path) else { return }
            try? FileManager.default.removeItem(atPath: path)
            self?.captureWindows()
        }
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.appearance = NSAppearance(named: .darkAqua)
        let enabled = ProcessInfo.processInfo.environment["SPEKTRA_ALL_SPACES"] == "1"
        let capture = ProcessInfo.processInfo.environment["SPEKTRA_CAPTURE"] == "1"
        if enabled || capture {
            debugLog("launch: allSpaces=\(enabled) windows=\(NSApp.windows.count)")
        }
        if capture {
            startCaptureWatcher()
            DispatchQueue.main.asyncAfter(deadline: .now() + 6) { [weak self] in
                self?.captureWindows()
            }
        }
        guard enabled else { return }
        timer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] timer in
            guard let self else {
                timer.invalidate()
                return
            }
            self.attempts += 1
            let windows = NSApp.windows
            self.debugLog("attempt \(self.attempts): windows=\(windows.count)")
            for window in windows {
                let content = window.contentView
                self.debugLog("  title=\(window.title) activeSpace=\(window.isOnActiveSpace) screen=\(window.screen?.localizedName ?? "nil") sharing=\(window.sharingType.rawValue) occlusion=\(window.occlusionState.rawValue) behavior=\(window.collectionBehavior.rawValue) frame=\(window.frame) visible=\(window.isVisible) alpha=\(window.alphaValue) opaque=\(window.isOpaque) content=\(String(describing: content)) subviews=\(content?.subviews.count ?? -1)")
                window.sharingType = .readOnly
                window.collectionBehavior = [.canJoinAllSpaces, .moveToActiveSpace, .fullScreenAuxiliary]
                window.level = .floating
                window.setFrame(NSRect(x: 80, y: 80, width: 1200, height: 780), display: true)
                if window.isMiniaturized {
                    window.deminiaturize(nil)
                }
                window.makeKeyAndOrderFront(nil)
                window.orderFrontRegardless()
            }
            NSApp.activate(ignoringOtherApps: true)
            if self.attempts >= 20 {
                timer.invalidate()
            }
        }
    }
}

@main
@MainActor
struct SpektraRollApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var app = AppState()
    @StateObject private var player = PlayerController()

    var body: some Scene {
        WindowGroup("SpektraRoll") {
            RootView()
                .environmentObject(app)
                .environmentObject(player)
                .preferredColorScheme(.dark)
        }
        .defaultSize(width: 1440, height: 900)
        .commands {
            CommandMenu("播放") {
                Button("播放 / 暂停") {
                    player.toggle()
                }
                .keyboardShortcut(.space, modifiers: [])
            }
            CommandMenu("素材") {
                Button("导入…") {
                    app.presentImportPanel()
                }
                .keyboardShortcut("i", modifiers: [.command])
            }
        }
    }
}
