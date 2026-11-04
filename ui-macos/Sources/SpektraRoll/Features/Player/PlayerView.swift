// SPDX-License-Identifier: GPL-3.0-or-later
import AVFoundation
import AVKit
import SwiftUI

struct PlayerSurface: NSViewRepresentable {
    let player: AVPlayer

    func makeNSView(context: Context) -> AVPlayerView {
        let view = AVPlayerView()
        view.controlsStyle = .none
        view.videoGravity = .resizeAspect
        view.player = player
        return view
    }

    func updateNSView(_ nsView: AVPlayerView, context: Context) {
        nsView.player = player
    }
}

struct PlayerView: View {
    @EnvironmentObject private var app: AppState
    @EnvironmentObject private var player: PlayerController

    var body: some View {
        ZStack {
            Theme.background

            if let asset = app.selectedAsset {
                if asset.isPlayable {
                    ZStack {
                        if app.showFilmPreview, let image = app.filmPreview, !player.isPlaying {
                            Image(nsImage: image)
                                .resizable()
                                .interpolation(.high)
                                .aspectRatio(contentMode: .fit)
                                .padding(24)
                        } else {
                            PlayerSurface(player: player.player)
                                .padding(24)
                        }

                        if app.isPreviewing {
                            VStack(spacing: 8) {
                                ProgressView()
                                    .controlSize(.small)
                                Text("渲染胶片预览…")
                                    .font(Theme.micro)
                                    .foregroundStyle(Theme.textMuted)
                            }
                            .padding(14)
                            .background(
                                RoundedRectangle(cornerRadius: 8, style: .continuous)
                                    .fill(Theme.backgroundElevated.opacity(0.92))
                            )
                        }
                    }

                    VStack {
                        HStack(alignment: .top) {
                            VStack(alignment: .leading, spacing: 3) {
                                Text(asset.displayName)
                                    .font(Theme.font(13, weight: .semibold))
                                    .foregroundStyle(Theme.text)
                                Text("\(asset.resolutionText) · \(asset.fpsText) · \(asset.durationText)")
                                    .font(Theme.micro)
                                    .foregroundStyle(Theme.textMuted)
                            }
                            Spacer()
                            TagBadge(text: asset.inputDescription)
                        }
                        .padding(.horizontal, 26)
                        .padding(.top, 18)

                        Spacer()

                        playerControls
                            .padding(.horizontal, 26)
                            .padding(.bottom, 18)
                    }
                } else {
                    EmptyStateView(
                        title: "R3D 仅 Windows",
                        subtitle: "macOS v1 没有 RED 插件；请先用 Windows 版或 REDCINE-X 导出 ProRes。",
                        icon: "exclamationmark.triangle"
                    )
                    .padding(40)
                }
            } else {
                EmptyStateView(
                    title: "还没有素材",
                    subtitle: "双击左侧空白，导入视频后双击缩略图开始播放。",
                    icon: "play.rectangle"
                )
                .padding(40)
            }
        }
        .onAppear {
            if let asset = app.selectedAsset, asset.isPlayable {
                player.load(url: asset.url)
            }
        }
        .onChange(of: app.selectedAssetID) { _, _ in
            guard let asset = app.selectedAsset, asset.isPlayable else { return }
            player.load(url: asset.url)
        }
        .onChange(of: player.isPlaying) { _, playing in
            if !playing, app.showFilmPreview {
                app.refreshFilmPreview(at: player.currentTime)
            }
        }
    }

    private var playerControls: some View {
        HStack(spacing: 12) {
            Button {
                player.toggle()
            } label: {
                Image(systemName: player.isPlaying ? "pause.fill" : "play.fill")
                    .font(.system(size: 11, weight: .bold))
                    .foregroundStyle(Theme.primaryText)
                    .frame(width: 30, height: 30)
                    .background(Circle().fill(Theme.primary))
            }
            .buttonStyle(.plain)

            Text(player.timeText)
                .font(Theme.caption)
                .monospacedDigit()
                .foregroundStyle(Theme.textMuted)

            Spacer()

            Button {
                app.setFilmPreview(!app.showFilmPreview, at: player.currentTime)
            } label: {
                HStack(spacing: 5) {
                    Image(systemName: app.showFilmPreview ? "film.fill" : "film")
                        .font(.system(size: 10, weight: .semibold))
                    Text("胶片效果")
                        .font(Theme.font(11, weight: .semibold))
                }
            }
            .buttonStyle(PillButtonStyle(filled: app.showFilmPreview))

            Text(app.statusMessage)
                .font(Theme.micro)
                .foregroundStyle(Theme.textDim)
                .lineLimit(1)
        }
    }
}
