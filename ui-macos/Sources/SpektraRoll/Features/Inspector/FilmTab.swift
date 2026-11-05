// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct FilmTab: View {
    @EnvironmentObject private var app: AppState

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            SectionTitle("FILM", trailing: app.filmStock?.name)

            HStack(spacing: 7) {
                Text("画幅")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textMuted)
                if app.settings.frameGauge.isCine {
                    TagBadge(text: "CINE")
                }
                Text(app.settings.frameGauge.widthText)
                    .font(Theme.micro)
                    .foregroundStyle(Theme.textDim)
                    .monospacedDigit()
                Spacer()
                Picker("画幅", selection: $app.settings.frameGauge) {
                    ForEach(FrameGauge.allCases) { gauge in
                        Text(gauge.menuTitle).tag(gauge)
                    }
                }
                .labelsHidden()
                .pickerStyle(.menu)
                .frame(width: 160)
            }

            if let catalog = app.catalog {
                filmGroup("彩色负片", stocks: catalog.films.filter { !$0.positive && $0.use != "cine" })
                filmGroup("电影负片", stocks: catalog.films.filter { !$0.positive && $0.use == "cine" })
                filmGroup("反转片", stocks: catalog.films.filter { $0.positive })
            } else {
                ProgressView("加载片库…")
                    .font(Theme.caption)
                    .foregroundStyle(Theme.textMuted)
            }

            SectionTitle("质感")
            Panel {
                VStack(alignment: .leading, spacing: 13) {
                    SliderRow(label: "颗粒", value: $app.settings.grain,
                              range: 0...1, step: 0.01)
                    HStack {
                        Text("颗粒稳定度")
                            .font(Theme.caption)
                            .foregroundStyle(Theme.textMuted)
                        Spacer()
                        Stepper("", value: $app.settings.grainRate, in: 1...8)
                            .labelsHidden()
                            .controlSize(.small)
                        Text("\(app.settings.grainRate)")
                            .font(Theme.caption)
                            .monospacedDigit()
                            .foregroundStyle(Theme.text)
                            .frame(width: 18, alignment: .trailing)
                    }
                    SliderRow(label: "光晕", value: $app.settings.halation,
                              range: 0...1, step: 0.01)
                    ToggleRow(
                        label: "防光晕层",
                        isOn: $app.settings.antiHalation,
                        detail: "v1 开启后光晕按 0.6 倍计算；当前胶片标记：\(app.filmStock?.antihalation ?? "—")"
                    )
                    SliderRow(label: "耀光", value: $app.settings.glare,
                              range: 0...1, step: 0.01)
                    SliderRow(label: "扩散", value: $app.settings.diffusion,
                              range: 0...1, step: 0.01)
                }
            }
        }
    }

    private func filmGroup(_ title: String, stocks: [StockCatalog.Stock]) -> some View {
        VStack(alignment: .leading, spacing: 7) {
            Text(title)
                .font(Theme.caption)
                .foregroundStyle(Theme.textMuted)
            ForEach(stocks) { stock in
                filmRow(stock)
            }
        }
    }

    private func filmRow(_ stock: StockCatalog.Stock) -> some View {
        let selected = app.settings.filmID == stock.id
        return Button {
            app.selectFilm(stock)
        } label: {
            HStack(spacing: 8) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(stock.name)
                        .font(Theme.caption)
                        .foregroundStyle(Theme.text)
                    Text("\(stock.referenceIlluminant) · \(stock.type)")
                        .font(Theme.micro)
                        .foregroundStyle(Theme.textDim)
                }
                Spacer(minLength: 4)
                TagBadge(text: badge(for: stock))
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

    private func badge(for stock: StockCatalog.Stock) -> String {
        if stock.positive { return "SLIDE" }
        return stock.use == "cine" ? "CINE" : "STILL"
    }
}
