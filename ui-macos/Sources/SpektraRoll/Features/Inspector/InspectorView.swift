// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

enum InspectorTab: String, CaseIterable, Identifiable {
    case info
    case film
    case print
    case advanced
    case output

    var id: String { rawValue }
    var title: String { rawValue.uppercased() }
}

struct InspectorView: View {
    @EnvironmentObject private var app: AppState

    var body: some View {
        VStack(spacing: 0) {
            tabBar
            Hairline()
            ScrollView {
                Group {
                    switch app.inspectorTab {
                    case .info: InfoTab()
                    case .film: FilmTab()
                    case .print: PrintTab()
                    case .advanced: AdvancedTab()
                    case .output: OutputTab()
                    }
                }
                .padding(16)
            }
            .scrollIndicators(.hidden)
        }
        .background(Theme.backgroundElevated)
    }

    private var tabBar: some View {
        HStack(spacing: 0) {
            ForEach(InspectorTab.allCases) { tab in
                Button {
                    app.inspectorTab = tab
                } label: {
                    VStack(spacing: 7) {
                        Text(tab.title)
                            .font(Theme.font(10, weight: .semibold))
                            .foregroundStyle(app.inspectorTab == tab ? Theme.text : Theme.textDim)
                        Rectangle()
                            .fill(app.inspectorTab == tab ? Theme.primary : Color.clear)
                            .frame(height: 2)
                    }
                    .frame(maxWidth: .infinity)
                    .frame(height: 40)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .background(Theme.surface)
    }
}
