// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct RootView: View {
    @EnvironmentObject private var app: AppState
    @AppStorage("sr.hasSeenAlphaNotice") private var hasSeenAlphaNotice = false
    @State private var showAlphaNotice = false

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
        .sheet(isPresented: $showAlphaNotice) {
            AlphaNoticeSheet {
                hasSeenAlphaNotice = true
                showAlphaNotice = false
            }
        }
        .onAppear {
            if !hasSeenAlphaNotice {
                showAlphaNotice = true
            }
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
