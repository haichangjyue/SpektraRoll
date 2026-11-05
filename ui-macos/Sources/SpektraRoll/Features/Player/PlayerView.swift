// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
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

    // D074 #2: zoom is relative to "fit"; 100% means one source pixel per
    // display point. pan is in points.
    @State private var zoom: CGFloat = 1
    @State private var pan = CGSize.zero
    @State private var fitScale: CGFloat = 1
    @State private var fittedSize = CGSize(width: 1, height: 1)
    @State private var availableSize = CGSize(width: 1, height: 1)

    var body: some View {
        ZStack {
            Theme.background

            if let asset = app.selectedAsset {
                if asset.isPlayable {
                    canvas(for: asset)
                    overlay(for: asset)
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
            loadCurrentAsset()
        }
        .onChange(of: app.selectedAssetID) { _, _ in
            resetZoom()
            loadCurrentAsset()
        }
        .onChange(of: player.isPlaying) { _, playing in
            if playing {
                app.preparePlaybackProxy()
                syncPlayerSource()
                return
            }
            guard !player.isSwitchingSource else { return }
            if app.showFilmPreview {
                app.refreshFilmPreview(at: player.currentTime)
            } else {
                app.refreshSourcePreview(at: player.currentTime)
            }
        }
        .onChange(of: app.showFilmPreview) { _, _ in
            guard player.isPlaying else { return }
            app.preparePlaybackProxy()
            syncPlayerSource()
        }
        .onChange(of: app.displayProxyURL) { _, _ in
            guard player.isPlaying else { return }
            syncPlayerSource()
        }
        .onChange(of: app.filmProxyURL) { _, _ in
            guard player.isPlaying else { return }
            syncPlayerSource()
        }
        .onChange(of: app.settings) { old, new in
            if app.showFilmPreview {
                if player.isPlaying {
                    app.scheduleFilmProxyRefresh()
                } else {
                    app.refreshFilmPreview(at: player.currentTime)
                }
            } else if old.input != new.input
                        || old.ocioEnabled != new.ocioEnabled
                        || old.ocioInput != new.ocioInput {
                app.refreshSourcePreview(at: player.currentTime)
                if player.isPlaying, let asset = app.selectedAsset {
                    app.prepareDisplayProxy(for: asset)
                }
            }
        }
    }

    private func loadCurrentAsset() {
        guard let asset = app.selectedAsset, asset.isPlayable else { return }
        player.load(url: asset.url)
        app.prepareDisplayProxy(for: asset)
        app.refreshSourcePreview(at: 0)
        if app.showFilmPreview {
            app.refreshFilmPreview(at: 0)
        }
    }

    /// D074 #3: pick the source clip, the display proxy or the film proxy for
    /// the current playback mode, preserving the playhead.
    private func syncPlayerSource() {
        guard let asset = app.selectedAsset, asset.isPlayable else { return }
        if app.showFilmPreview, let film = app.filmProxyURL {
            player.switchSource(to: film, source: .film, autoPlay: true)
        } else if let display = app.displayProxyURL {
            player.switchSource(to: display, source: .display, autoPlay: true)
        } else {
            player.switchSource(to: asset.url, source: .original, autoPlay: true)
        }
    }

    // MARK: - Video canvas

    private func canvas(for asset: MediaAsset) -> some View {
        GeometryReader { geo in
            let inset: CGFloat = 24
            let available = CGSize(
                width: max(1, geo.size.width - inset * 2),
                height: max(1, geo.size.height - inset * 2)
            )
            let native = CGSize(
                width: CGFloat(max(asset.width, 1)),
                height: CGFloat(max(asset.height, 1))
            )
            let fitted = aspectFitSize(native: native, in: available)

            ZStack {
                ZStack {
                    if !player.isPlaying, let image = activeStill {
                        Image(nsImage: image)
                            .resizable()
                            .interpolation(.high)
                            .aspectRatio(contentMode: .fit)
                            .frame(width: fitted.width, height: fitted.height)
                    } else {
                        PlayerSurface(player: player.player)
                            .frame(width: fitted.width, height: fitted.height)
                    }
                }
                .scaleEffect(zoom)
                .offset(pan)
                .frame(width: geo.size.width, height: geo.size.height)
                .clipped()

                if let status = previewStatus {
                    VStack(spacing: 8) {
                        ProgressView()
                            .controlSize(.small)
                        Text(status)
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
            .frame(width: geo.size.width, height: geo.size.height)
            .contentShape(Rectangle())
            .overlay(
                InteractionLayer(
                    onScroll: { delta in
                        adjustZoom(by: CGFloat(exp(Double(delta) * 0.02)))
                    },
                    onMagnify: { magnification in
                        adjustZoom(by: max(0.2, 1 + magnification))
                    },
                    onPan: { delta in
                        pan.width += delta.width
                        pan.height += delta.height
                        clampPan()
                    },
                    onDoubleClick: {
                        toggleZoom()
                    }
                )
            )
            .onAppear {
                updateViewport(native: native, available: available, fitted: fitted)
            }
            .onChange(of: geo.size) { _, newSize in
                let newAvailable = CGSize(
                    width: max(1, newSize.width - inset * 2),
                    height: max(1, newSize.height - inset * 2)
                )
                let newFitted = aspectFitSize(native: native, in: newAvailable)
                updateViewport(native: native, available: newAvailable, fitted: newFitted)
            }
        }
    }

    private var activeStill: NSImage? {
        app.showFilmPreview ? app.filmPreview : app.sourcePreview
    }

    private var previewStatus: String? {
        if app.isPreviewing { return "渲染胶片预览…" }
        if app.isSourcePreviewing { return "解码显示画面…" }
        return nil
    }

    // MARK: - Zoom / pan

    private func aspectFitSize(native: CGSize, in available: CGSize) -> CGSize {
        guard native.width > 0, native.height > 0 else { return available }
        let scale = min(available.width / native.width, available.height / native.height)
        return CGSize(width: native.width * scale, height: native.height * scale)
    }

    private func updateViewport(native: CGSize, available: CGSize, fitted: CGSize) {
        availableSize = available
        fittedSize = fitted
        fitScale = fitted.width / max(native.width, 1)
        clampPan()
    }

    private func adjustZoom(by factor: CGFloat) {
        let next = min(20, max(0.1, zoom * factor))
        guard abs(next - zoom) > 0.0001 else { return }
        zoom = next
        clampPan()
    }

    private func clampPan() {
        let maxX = max(0, (fittedSize.width * zoom - availableSize.width) / 2)
        let maxY = max(0, (fittedSize.height * zoom - availableSize.height) / 2)
        pan.width = min(maxX, max(-maxX, pan.width))
        pan.height = min(maxY, max(-maxY, pan.height))
    }

    private func setFit() {
        zoom = 1
        pan = .zero
    }

    private func setOneToOne() {
        guard fitScale > 0 else { return }
        zoom = 1 / fitScale
        pan = .zero
        clampPan()
    }

    private func toggleZoom() {
        if abs(zoom - 1) < 0.01 {
            setOneToOne()
        } else {
            setFit()
        }
    }

    private func resetZoom() {
        zoom = 1
        pan = .zero
    }

    private var zoomLabel: String {
        let actual = zoom * fitScale
        if abs(zoom - 1) < 0.01 { return "适合" }
        if abs(actual - 1) < 0.02 { return "100%" }
        return "\(Int((actual * 100).rounded()))%"
    }

    // MARK: - Overlay

    private func overlay(for asset: MediaAsset) -> some View {
        VStack(spacing: 0) {
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

            if app.isGeneratingProxy {
                HStack(spacing: 8) {
                    ProgressView()
                        .controlSize(.small)
                    Text("\(app.proxyStatus) \(Int((app.proxyProgress * 100).rounded()))%")
                        .font(Theme.micro)
                        .foregroundStyle(Theme.textMuted)
                        .monospacedDigit()
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(Capsule().fill(Theme.backgroundElevated.opacity(0.94)))
                .overlay(Capsule().stroke(Theme.border, lineWidth: 1))
                .frame(maxWidth: .infinity, alignment: .trailing)
                .padding(.horizontal, 26)
                .padding(.bottom, 6)
            }

            if !player.isPlaying, let stats = app.frameStats {
                HistogramView(stats: stats)
                    .padding(.horizontal, 26)
                    .padding(.bottom, 10)
            }

            playerControls
                .padding(.horizontal, 26)
                .padding(.bottom, 18)
        }
    }

    private var playerControls: some View {
        HStack(spacing: 10) {
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

            Button("适合") {
                setFit()
            }
            .buttonStyle(CompactPillButtonStyle())

            Button("100%") {
                setOneToOne()
            }
            .buttonStyle(CompactPillButtonStyle())

            Text(zoomLabel)
                .font(Theme.micro)
                .monospacedDigit()
                .foregroundStyle(Theme.textDim)
                .frame(width: 42, alignment: .trailing)

            Spacer()

            Button {
                app.inspectorTab = .output
            } label: {
                HStack(spacing: 5) {
                    Image(systemName: "square.and.arrow.up")
                        .font(.system(size: 10, weight: .semibold))
                    Text("导出")
                        .font(Theme.font(11, weight: .semibold))
                }
            }
            .buttonStyle(PillButtonStyle(filled: false))

            Button {
                app.setFilmPreview(!app.showFilmPreview, at: player.currentTime,
                                   renderStill: !player.isPlaying)
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

// MARK: - Mouse / trackpad input

/// D074 #2: SwiftUI has no scroll-wheel modifier, so a transparent AppKit view
/// forwards wheel, pinch, drag and double-click to the player.
private struct InteractionLayer: NSViewRepresentable {
    var onScroll: (CGFloat) -> Void
    var onMagnify: (CGFloat) -> Void
    var onPan: (CGSize) -> Void
    var onDoubleClick: () -> Void

    func makeNSView(context: Context) -> InteractionView {
        let view = InteractionView()
        view.onScroll = onScroll
        view.onMagnify = onMagnify
        view.onPan = onPan
        view.onDoubleClick = onDoubleClick
        return view
    }

    func updateNSView(_ nsView: InteractionView, context: Context) {
        nsView.onScroll = onScroll
        nsView.onMagnify = onMagnify
        nsView.onPan = onPan
        nsView.onDoubleClick = onDoubleClick
    }
}

private final class InteractionView: NSView {
    var onScroll: ((CGFloat) -> Void)?
    var onMagnify: ((CGFloat) -> Void)?
    var onPan: ((CGSize) -> Void)?
    var onDoubleClick: (() -> Void)?

    private var lastPoint: NSPoint?

    override var acceptsFirstResponder: Bool { true }

    override func scrollWheel(with event: NSEvent) {
        onScroll?(event.scrollingDeltaY)
    }

    override func magnify(with event: NSEvent) {
        onMagnify?(event.magnification)
    }

    override func mouseDown(with event: NSEvent) {
        if event.clickCount == 2 {
            onDoubleClick?()
            return
        }
        lastPoint = convert(event.locationInWindow, from: nil)
    }

    override func mouseDragged(with event: NSEvent) {
        guard let last = lastPoint else { return }
        let point = convert(event.locationInWindow, from: nil)
        // AppKit y grows up, SwiftUI offset y grows down.
        onPan?(CGSize(width: point.x - last.x, height: last.y - point.y))
        lastPoint = point
    }

    override func mouseUp(with event: NSEvent) {
        lastPoint = nil
    }
}
