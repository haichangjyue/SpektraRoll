// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

@main
@MainActor
struct SpektraRollApp: App {
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
