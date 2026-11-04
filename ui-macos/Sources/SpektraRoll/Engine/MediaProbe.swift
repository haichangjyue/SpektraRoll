// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import AVFoundation
import Foundation

enum MediaProbe {
    static func probe(url: URL, engine: EngineClient) async -> MediaAsset? {
        var asset = MediaAsset(path: url.path, name: url.lastPathComponent)
        asset.isR3D = url.pathExtension.lowercased() == "r3d"

        if !asset.isR3D {
            let avAsset = AVURLAsset(url: url)
            if let duration = try? await avAsset.load(.duration), duration.seconds.isFinite {
                asset.duration = duration.seconds
            }
            if let tracks = try? await avAsset.loadTracks(withMediaType: .video),
               let track = tracks.first {
                if let size = try? await track.load(.naturalSize),
                   let transform = try? await track.load(.preferredTransform) {
                    let oriented = size.applying(transform)
                    asset.width = Int(abs(oriented.width).rounded())
                    asset.height = Int(abs(oriented.height).rounded())
                }
                if let rate = try? await track.load(.nominalFrameRate) {
                    asset.fps = Double(rate)
                }
            }
        }

        let probe = await engine.probe(url: url)
        if probe.width > 0 { asset.width = probe.width }
        if probe.height > 0 { asset.height = probe.height }
        if probe.fps > 0 { asset.fps = probe.fps }
        if probe.duration > 0 { asset.duration = probe.duration }
        if probe.frames > 0 { asset.frames = probe.frames }
        asset.codec = probe.codec
        asset.colorSpace = probe.colorSpace
        asset.colorTransfer = probe.colorTransfer
        asset.colorPrimaries = probe.colorPrimaries
        asset.timecode = probe.timecode

        if asset.frames == 0, asset.duration > 0, asset.fps > 0 {
            asset.frames = Int((asset.duration * asset.fps).rounded())
        }
        if asset.duration == 0, asset.frames > 0, asset.fps > 0 {
            asset.duration = Double(asset.frames) / asset.fps
        }
        return asset
    }

    static func makeThumbnail(url: URL, cacheDirectory: URL) async -> String? {
        guard url.pathExtension.lowercased() != "r3d" else { return nil }
        return await Task.detached(priority: .utility) { () -> String? in
            let avAsset = AVURLAsset(url: url)
            let generator = AVAssetImageGenerator(asset: avAsset)
            generator.appliesPreferredTrackTransform = true
            generator.maximumSize = CGSize(width: 640, height: 360)
            generator.requestedTimeToleranceBefore = .zero
            generator.requestedTimeToleranceAfter = .zero
            var actualTime = CMTime.zero
            guard let image = try? generator.copyCGImage(at: .zero, actualTime: &actualTime) else {
                return nil
            }
            let bitmap = NSBitmapImageRep(cgImage: image)
            guard let data = bitmap.representation(using: .png, properties: [:]) else {
                return nil
            }
            let directory = cacheDirectory.appendingPathComponent("thumbnails", isDirectory: true)
            do {
                try FileManager.default.createDirectory(at: directory,
                                                        withIntermediateDirectories: true)
                let output = directory.appendingPathComponent("\(UUID().uuidString).png")
                try data.write(to: output)
                return output.path
            } catch {
                return nil
            }
        }.value
    }
}
