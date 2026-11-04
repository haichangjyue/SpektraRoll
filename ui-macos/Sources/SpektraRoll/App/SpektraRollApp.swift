// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import SwiftUI

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var timer: Timer?
    private var attempts = 0

    func applicationDidFinishLaunching(_ notification: Notification) {
        guard ProcessInfo.processInfo.environment["SPEKTRA_ALL_SPACES"] == "1" else { return }
        timer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] timer in
            guard let self else {
                timer.invalidate()
                return
            }
            self.attempts += 1
            let windows = NSApp.windows
            NSLog("SpektraRoll debug: attempt %d, windows %d", self.attempts, windows.count)
            for window in windows {
                window.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
                window.level = .floating
                window.setFrame(NSRect(x: 80, y: 80, width: 1200, height: 780), display: true)
                if window.isMiniaturized {
                    window.deminiaturize(nil)
                }
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
