// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import SwiftUI

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
            let allSpaces = ProcessInfo.processInfo.environment["SPEKTRA_ALL_SPACES"] == "1"
            for window in NSApp.windows {
                if allSpaces {
                    window.collectionBehavior.insert(.canJoinAllSpaces)
                }
                if window.isMiniaturized {
                    window.deminiaturize(nil)
                }
                window.makeKeyAndOrderFront(nil)
            }
            NSApp.activate(ignoringOtherApps: true)
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
