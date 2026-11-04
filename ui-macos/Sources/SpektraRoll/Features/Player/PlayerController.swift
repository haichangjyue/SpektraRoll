// SPDX-License-Identifier: GPL-3.0-or-later
import AVFoundation
import Combine
import Foundation

@MainActor
final class PlayerController: ObservableObject {
    let player = AVPlayer()

    @Published private(set) var isPlaying = false
    @Published private(set) var currentTime: Double = 0
    @Published private(set) var duration: Double = 0

    private var timeObserver: Any?
    private var statusObservation: NSKeyValueObservation?
    private var endObserver: NSObjectProtocol?

    init() {
        timeObserver = player.addPeriodicTimeObserver(
            forInterval: CMTime(seconds: 0.1, preferredTimescale: 600),
            queue: .main
        ) { [weak self] time in
            guard let self else { return }
            self.currentTime = time.seconds.isFinite ? max(0, time.seconds) : 0
            if let item = self.player.currentItem {
                let length = item.duration.seconds
                if length.isFinite, length > 0 {
                    self.duration = length
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
        let item = AVPlayerItem(url: url)
        player.replaceCurrentItem(with: item)
        player.pause()
        currentTime = 0
        duration = 0
        isPlaying = false
    }

    func toggle() {
        if isPlaying {
            player.pause()
        } else {
            if duration > 0, currentTime >= duration - 0.05 {
                seek(to: 0)
            }
            player.play()
        }
    }

    func seek(to seconds: Double) {
        let clamped = max(0, min(seconds, max(duration, 0)))
        player.seek(to: CMTime(seconds: clamped, preferredTimescale: 600))
        currentTime = clamped
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
