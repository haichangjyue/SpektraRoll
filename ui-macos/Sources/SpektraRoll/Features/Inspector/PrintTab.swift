// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct PrintTab: View {
    @EnvironmentObject private var app: AppState

    private var allowedScans: [ScanMode] {
        app.filmStock?.positive == true ? [.slide] : [.negative, .print]
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            SectionTitle("PRINT", trailing: app.paperStock?.name)

            if app.filmStock?.positive == true {
                Panel {
                    Text("反转片直接扫描，不使用相纸或印片。")
                        .font(Theme.caption)
                        .foregroundStyle(Theme.textMuted)
                        .fixedSize(horizontal: false, vertical: true)
                }
            } else if let catalog = app.catalog {
                paperGroup("相纸", stocks: catalog.papers.filter { !$0.printFilm })
                paperGroup("电影印片", stocks: catalog.papers.filter { $0.printFilm })
            } else {
                ProgressView("加载片库…")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textMuted)
            }

            VStack(alignment: .leading, spacing: 6) {
                Text("扫描方式")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textMuted)
                Picker("扫描方式", selection: Binding(
                    get: { app.settings.scan },
                    set: { app.setScan($0) }
                )) {
                    ForEach(allowedScans, id: \.self) { mode in
                        Text(mode.title).tag(mode)
                    }
                }
                .labelsHidden()
                .pickerStyle(.segmented)
            }

            if app.settings.scan != .slide {
                SectionTitle("放大机滤色片")
                Panel {
                    VStack(alignment: .leading, spacing: 12) {
                        HStack(spacing: 6) {
                            Text("中性值（数据库）")
                                .font(Theme.caption)
                                .foregroundStyle(Theme.textMuted)
                            Spacer(minLength: 4)
                            if let neutral = app.neutralFilters?.filters {
                                Text(String(format: "C %.1f  M %.1f  Y %.1f",
                                            neutral.c, neutral.m, neutral.y))
                                    .font(Theme.micro)
                                    .monospacedDigit()
                                    .foregroundStyle(Theme.text)
                            } else {
                                Text("—")
                                    .font(Theme.micro)
                                    .foregroundStyle(Theme.textDim)
                            }
                        }

                        SliderRow(label: "C 偏移", value: $app.settings.filterCOffset,
                                  range: -20...20, step: 0.1, format: "%+.1f")
                        SliderRow(label: "M 偏移", value: $app.settings.filterMOffset,
                                  range: -20...20, step: 0.1, format: "%+.1f")
                        SliderRow(label: "Y 偏移", value: $app.settings.filterYOffset,
                                  range: -20...20, step: 0.1, format: "%+.1f")

                        HStack(spacing: 8) {
                            Text("偏移叠加在数据库中性值上；0 = 中性。")
                                .font(Theme.micro)
                                .foregroundStyle(Theme.textDim)
                                .fixedSize(horizontal: false, vertical: true)
                            Spacer(minLength: 4)
                            Button("重置") {
                                app.resetFilterOffsets()
                            }
                            .buttonStyle(CompactPillButtonStyle())
                        }
                    }
                }
            }

            Panel {
                VStack(alignment: .leading, spacing: 13) {
                    SliderRow(label: "反差", value: $app.settings.contrast,
                              range: 0.5...2.0, step: 0.01)
                    ToggleRow(
                        label: "OkLCh 色域压缩",
                        isOn: $app.settings.gamut,
                        detail: "负片默认开；印片/扫描仪风格可以关掉。"
                    )
                }
            }
        }
    }

    private func paperGroup(_ title: String, stocks: [StockCatalog.Stock]) -> some View {
        VStack(alignment: .leading, spacing: 7) {
            Text(title)
                .font(Theme.caption)
                .foregroundStyle(Theme.textMuted)
            ForEach(stocks) { stock in
                paperRow(stock)
            }
        }
    }

    private func paperRow(_ stock: StockCatalog.Stock) -> some View {
        let selected = app.settings.paperID == stock.id
        return Button {
            app.selectPaper(stock)
        } label: {
            HStack(spacing: 8) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(stock.name)
                        .font(Theme.caption)
                        .foregroundStyle(Theme.text)
                    Text(stock.printFilm ? "电影印片" : "相纸")
                        .font(Theme.micro)
                        .foregroundStyle(Theme.textDim)
                }
                Spacer(minLength: 4)
                TagBadge(text: stock.printFilm ? "PRINT" : "PAPER")
                if selected {
                    Image(systemName: "checkmark")
                        .font(.system(size: 10, weight: .bold))
                        .foregroundStyle(Theme.primary)
                }
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .background(
                RoundedRectangle(cornerRadius: 6, style: .continuous)
                    .fill(selected ? Theme.surfaceRaised : Color.clear)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 6, style: .continuous)
                    .stroke(selected ? Theme.primary : Theme.border,
                            lineWidth: selected ? 1.5 : 1)
            )
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
