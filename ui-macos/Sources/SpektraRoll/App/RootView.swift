// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct RootView: View {
    @EnvironmentObject private var app: AppState

    var body: some View {
        HStack(spacing: 0) {
            LibraryView()
                .frame(width: 220)
            Rectangle()
                .fill(Theme.divider)
                .frame(width: 1)
            PlayerView()
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            Rectangle()
                .fill(Theme.divider)
                .frame(width: 1)
            InspectorView()
                .frame(width: 380)
        }
        .background(Theme.background)
        .frame(minWidth: 1280, minHeight: 800)
        .sheet(item: $app.importDraft) { draft in
            ImportSheet(draft: draft)
                .environmentObject(app)
        }
        .alert("提示", isPresented: Binding(
            get: { app.errorMessage != nil },
            set: { if !$0 { app.errorMessage = nil } }
        )) {
            Button("好", role: .cancel) {}
        } message: {
            Text(app.errorMessage ?? "")
        }
        .task {
            await app.bootstrap()
        }
        .onChange(of: app.settings) { _, _ in
            app.save()
        }
    }
}
