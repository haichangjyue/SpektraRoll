// SPDX-License-Identifier: GPL-3.0-or-later
import AVFoundation
import Combine
import Dispatch
import Foundation

enum PlaybackSource {
    case original
    case display
    case film
}

@MainActor
final class PlayerController: ObservableObject {
    let player = AVPlayer()

    @Published private(set) var isPlaying = false
    @Published private(set) var currentTime: Double = 0
    @Published private(set) var duration: Double = 0
    @Published private(set) var source: PlaybackSource = .original
    /// True while `replaceCurrentItem` is settling; the pause event it emits
    /// must not trigger a still-preview refresh.
    @Published private(set) var isSwitchingSource = false
    /// D077 #2: while a film proxy is being generated the player stays paused;
    /// the user's play request is remembered and resumed when the proxy is
    /// ready (pressing space again cancels the pending resume).
    @Published private(set) var isPlaybackBlocked = false
    private var resumeAfterBlock = false

    private(set) var currentURL: URL?
    private var timeObserver: Any?
    private var statusObservation: NSKeyValueObservation?
    private var durationObservation: NSKeyValueObservation?
    private var endObserver: NSObjectProtocol?

    init() {
        timeObserver = player.addPeriodicTimeObserver(
            forInterval: CMTime(seconds: 0.1, preferredTimescale: 600),
            queue: .main
        ) { [weak self] time in
            Task { @MainActor in
                guard let self else { return }
                self.currentTime = time.seconds.isFinite ? max(0, time.seconds) : 0
                if let item = self.player.currentItem {
                    let length = item.duration.seconds
                    if length.isFinite, length > 0 {
                        self.duration = length
                    }
                }
            }
        }

        statusObservation = player.observe(\.timeControlStatus, options: [.new]) { [weak self] player, _ in
            Task { @MainActor in
                self?.isPlaying = player.timeControlStatus == .playing
            }
        }

        endObserver = NotificationCenter.default.addObserver(
            forName: .AVPlayerItemDidPlayToEndTime,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            Task { @MainActor in
                self?.isPlaying = false
                self?.currentTime = self?.duration ?? 0
            }
        }
    }

    func load(url: URL) {
        currentURL = url
        source = .original
        isPlaybackBlocked = false
        resumeAfterBlock = false
        debugLog("load \(url.lastPathComponent)")
        install(url: url, seek: 0, autoPlay: false, resetTime: true)
    }

    /// D074 #3: switch between the source clip and the low-resolution display
    /// or film proxy without losing the playhead. The item replacement is
    /// asynchronous, so `isSwitchingSource` suppresses the resulting pause
    /// event while the seek/play settles.
    func switchSource(to url: URL, source: PlaybackSource, autoPlay: Bool) {
        if currentURL?.standardizedFileURL == url.standardizedFileURL {
            self.source = source
            if autoPlay, !isPlaying {
                player.play()
            }
            return
        }
        let seconds = player.currentTime().seconds
        let target = seconds.isFinite ? max(0, seconds) : 0
        let shouldPlay = autoPlay || isPlaying
        isSwitchingSource = true
        self.source = source
        let position = String(format: "%.2f", target)
        debugLog("switch \(sourceName(source)) \(url.lastPathComponent) at \(position)s play=\(shouldPlay)")
        install(url: url, seek: target, autoPlay: shouldPlay, resetTime: false)
    }

    private func sourceName(_ source: PlaybackSource) -> String {
        switch source {
        case .original: return "original"
        case .display: return "display"
        case .film: return "film"
        }
    }

    private func debugLog(_ message: String) {
        guard ProcessInfo.processInfo.environment["SPEKTRA_PLAYER_DEBUG"] == "1" else { return }
        let url = URL(fileURLWithPath: "/tmp/spektra-player.log")
        let line = message + "\n"
        if let handle = try? FileHandle(forWritingTo: url) {
            handle.seekToEndOfFile()
            handle.write(line.data(using: .utf8) ?? Data())
            handle.closeFile()
        } else {
            try? line.write(to: url, atomically: true, encoding: .utf8)
        }
    }

    private func install(url: URL, seek seconds: Double, autoPlay: Bool, resetTime: Bool) {
        durationObservation?.invalidate()
        player.pause()
        if resetTime {
            currentTime = 0
            duration = 0
            isPlaying = false
        }
        let item = AVPlayerItem(url: url)
        player.replaceCurrentItem(with: item)
        currentURL = url
        durationObservation = item.observe(\.duration, options: [.initial, .new]) { [weak self] item, _ in
            let length = item.duration.seconds
            guard length.isFinite, length > 0 else { return }
            Task { @MainActor in
                self?.duration = length
            }
        }
        let time = CMTime(seconds: max(0, seconds), preferredTimescale: 600)
        player.seek(to: time, toleranceBefore: .zero, toleranceAfter: .zero) { [weak self] _ in
            Task { @MainActor in
                guard let self else { return }
                if autoPlay {
                    self.player.play()
                }
                // The pause notification from replaceCurrentItem can arrive
                // slightly after the seek callback; keep the guard up briefly.
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.35) {
                    self.isSwitchingSource = false
                }
            }
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
            self?.isSwitchingSource = false
        }
    }

    func pause() {
        player.pause()
    }

    func toggle() {
        if isPlaybackBlocked {
            resumeAfterBlock.toggle()
            return
        }
        if isPlaying {
            player.pause()
        } else {
            if duration > 0, currentTime >= duration - 0.05 {
                seek(to: 0)
            }
            player.play()
        }
    }

    func blockPlaybackForFilmProxy(rememberPlayRequest: Bool) {
        if rememberPlayRequest || isPlaying {
            resumeAfterBlock = true
        }
        isPlaybackBlocked = true
        player.pause()
        debugLog("block film proxy playing=\(isPlaying) resume=\(resumeAfterBlock)")
    }

    func unblockAfterFilmProxy(resume: Bool) {
        isPlaybackBlocked = false
        let shouldPlay = resume && resumeAfterBlock
        resumeAfterBlock = false
        debugLog("unblock film proxy play=\(shouldPlay)")
        if shouldPlay {
            player.play()
        }
    }

    func seek(to seconds: Double) {
        let clamped = max(0, min(seconds, max(duration, 0)))
        player.seek(to: CMTime(seconds: clamped, preferredTimescale: 600))
        currentTime = clamped
    }

    /// D082: one-frame step for the timeline. The caller supplies the clip fps
    /// because the player itself has no frame-rate metadata.
    func stepFrame(_ delta: Int, fps: Double) {
        guard fps > 0 else { return }
        seek(to: currentTime + Double(delta) / fps)
    }

    func frameNumber(fps: Double) -> Int {
        guard fps > 0 else { return 0 }
        return max(0, Int((currentTime * fps).rounded()))
    }

    var timeText: String {
        Self.format(currentTime) + " / " + Self.format(duration)
    }

    static func format(_ seconds: Double) -> String {
        guard seconds.isFinite, seconds >= 0 else { return "00:00" }
        let total = Int(seconds.rounded())
        let hours = total / 3600
        let minutes = (total % 3600) / 60
        let secs = total % 60
        if hours > 0 {
            return String(format: "%d:%02d:%02d", hours, minutes, secs)
        }
        return String(format: "%02d:%02d", minutes, secs)
    }
}
