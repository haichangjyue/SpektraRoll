// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import SwiftUI

struct LibraryView: View {
    @EnvironmentObject private var app: AppState

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Text("底片")
                    .font(Theme.font(14, weight: .bold))
                    .foregroundStyle(Theme.text)
                Spacer()
                Text("\(app.assets.count)")
                    .font(Theme.micro)
                    .foregroundStyle(Theme.textDim)
                    .monospacedDigit()
                Button {
                    app.presentImportPanel()
                } label: {
                    Image(systemName: "plus")
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundStyle(Theme.text)
                        .frame(width: 24, height: 24)
                        .background(Circle().fill(Theme.surfaceRaised))
                }
                .buttonStyle(.plain)
            }
            .padding(.horizontal, 14)
            .frame(height: 48)

            Hairline()

            ScrollView {
                LazyVStack(spacing: 10) {
                    ForEach(app.assets) { asset in
                        LibraryCard(asset: asset, selected: asset.id == app.selectedAssetID)
                            .onTapGesture {
                                app.selectAsset(asset)
                            }
                            .highPriorityGesture(
                                TapGesture(count: 2).onEnded {
                                    app.selectAsset(asset)
                                }
                            )
                            .contextMenu {
                                Button("在播放器中打开") {
                                    app.selectAsset(asset)
                                }
                                Button("从底片移除", role: .destructive) {
                                    app.removeAsset(asset)
                                }
                            }
                    }

                    if app.assets.isEmpty {
                        EmptyStateView(
                            title: "导入素材",
                            subtitle: "双击这里，选择视频文件或项目目录。",
                            icon: "film.stack"
                        )
                        .onTapGesture(count: 2) {
                            app.presentImportPanel()
                        }
                    } else {
                        Button {
                            app.presentImportPanel()
                        } label: {
                            Label("导入素材", systemImage: "plus")
                                .font(Theme.caption)
                        }
                        .buttonStyle(PillButtonStyle(filled: false))
                        .padding(.top, 2)
                    }
                }
                .padding(12)
            }
            .scrollIndicators(.hidden)

            Spacer(minLength: 0)
            Text(app.statusMessage)
                .font(Theme.micro)
                .foregroundStyle(Theme.textDim)
                .lineLimit(2)
                .padding(.horizontal, 14)
                .padding(.bottom, 12)
        }
        .background(Theme.surface)
    }
}

private struct LibraryCard: View {
    let asset: MediaAsset
    let selected: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 7) {
            ZStack(alignment: .bottomTrailing) {
                thumbnail
                    .frame(maxWidth: .infinity)
                    .frame(height: 96)
                    .clipped()
                    .background(Theme.surfaceSunken)
                    .clipShape(RoundedRectangle(cornerRadius: 6, style: .continuous))
                Text(asset.durationText)
                    .font(Theme.font(9, weight: .semibold))
                    .foregroundStyle(Theme.text)
                    .padding(.horizontal, 5)
                    .padding(.vertical, 2)
                    .background(Capsule().fill(Theme.scrim.opacity(0.7)))
                    .padding(6)
            }
            Text(asset.displayName)
                .font(Theme.caption)
                .foregroundStyle(Theme.text)
                .lineLimit(1)
                .truncationMode(.middle)
            HStack(spacing: 6) {
                Text(asset.resolutionText)
                Text("·")
                Text(asset.fpsText)
            }
            .font(Theme.micro)
            .foregroundStyle(Theme.textDim)
            .lineLimit(1)
        }
        .padding(8)
        .background(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .fill(selected ? Theme.surfaceRaised : Theme.surfaceSunken)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(selected ? Theme.primary : Theme.border,
                        lineWidth: selected ? 2 : 1)
        )
        .contentShape(Rectangle())
    }

    @ViewBuilder
    private var thumbnail: some View {
        if let path = asset.thumbnailPath, let image = NSImage(contentsOfFile: path) {
            Image(nsImage: image)
                .resizable()
                .aspectRatio(contentMode: .fill)
        } else {
            ZStack {
                Theme.surfaceSunken
                Image(systemName: asset.isR3D ? "exclamationmark.triangle" : "film")
                    .font(.system(size: 20, weight: .light))
                    .foregroundStyle(Theme.textDim)
            }
        }
    }
}
