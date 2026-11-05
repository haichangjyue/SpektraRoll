// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

enum EngineError: LocalizedError {
    case cliNotFound
    case toolNotFound(String)
    case commandFailed(String)
    case decoding(String)

    var errorDescription: String? {
        switch self {
        case .cliNotFound:
            return "找不到 spektra-roll-cli。请用 scripts/build-macos-ui.sh 构建，或设置 SPEKTRA_CLI。"
        case .toolNotFound(let name):
            return "找不到 \(name)。请把它放进 app 的 Helpers 目录，或安装到 PATH。"
        case .commandFailed(let message):
            return message.isEmpty ? "引擎命令失败。" : message
        case .decoding(let message):
            return "无法解析引擎输出：\(message)"
        }
    }
}

struct ProcessResult {
    let status: Int32
    let stdout: String
    let stderr: String
}

private final class DataBox: @unchecked Sendable {
    private let lock = NSLock()
    private var data = Data()

    func append(_ chunk: Data) {
        lock.lock()
        data.append(chunk)
        lock.unlock()
    }

    func set(_ value: Data) {
        lock.lock()
        data = value
        lock.unlock()
    }

    var value: Data {
        lock.lock()
        defer { lock.unlock() }
        return data
    }
}

final class EngineClient: @unchecked Sendable {
    let cliURL: URL?
    let ffmpegURL: URL?
    let ffprobeURL: URL?
    let dataRoot: URL?
    let profilesRoot: URL?
    let ocioRoot: URL?
    let ocioConfigURL: URL?
    let filtersURL: URL?

    private let processLock = NSLock()
    private var currentProcess: Process?

    init() {
        let fileManager = FileManager.default
        let cwd = URL(fileURLWithPath: fileManager.currentDirectoryPath)
        let bundleURL = Bundle.main.bundleURL
        let resources = Bundle.main.resourceURL

        var cliCandidates: [URL] = []
        if let env = ProcessInfo.processInfo.environment["SPEKTRA_CLI"], !env.isEmpty {
            cliCandidates.append(URL(fileURLWithPath: env))
        }
        cliCandidates.append(bundleURL.appendingPathComponent("Contents/Helpers/spektra-roll-cli"))
        if let resources {
            cliCandidates.append(resources.appendingPathComponent("spektra-roll-cli"))
        }
        cliCandidates.append(cwd.appendingPathComponent("build-red/spektra-roll-cli"))
        cliCandidates.append(cwd.appendingPathComponent("build/spektra-roll-cli"))
        cliCandidates.append(cwd.deletingLastPathComponent().appendingPathComponent("build-red/spektra-roll-cli"))
        cliCandidates.append(cwd.deletingLastPathComponent().appendingPathComponent("build/spektra-roll-cli"))
        cliCandidates.append(URL(fileURLWithPath: "/opt/homebrew/bin/spektra-roll-cli"))
        cliCandidates.append(URL(fileURLWithPath: "/usr/local/bin/spektra-roll-cli"))
        self.cliURL = Self.firstExecutable(cliCandidates, pathName: "spektra-roll-cli")

        var ffmpegCandidates: [URL] = []
        if let env = ProcessInfo.processInfo.environment["SPEKTRA_FFMPEG"], !env.isEmpty {
            ffmpegCandidates.append(URL(fileURLWithPath: env))
        }
        ffmpegCandidates.append(bundleURL.appendingPathComponent("Contents/Helpers/ffmpeg"))
        ffmpegCandidates.append(URL(fileURLWithPath: "/opt/homebrew/bin/ffmpeg"))
        ffmpegCandidates.append(URL(fileURLWithPath: "/usr/local/bin/ffmpeg"))
        let ffmpeg = Self.firstExecutable(ffmpegCandidates, pathName: "ffmpeg")
        self.ffmpegURL = ffmpeg
        if let ffmpeg {
            let sibling = ffmpeg.deletingLastPathComponent().appendingPathComponent("ffprobe")
            self.ffprobeURL = FileManager.default.isExecutableFile(atPath: sibling.path) ? sibling : nil
        } else {
            self.ffprobeURL = Self.firstExecutable([], pathName: "ffprobe")
        }

        var rootCandidates: [URL] = []
        if let resources {
            rootCandidates.append(resources.appendingPathComponent("data"))
        }
        rootCandidates.append(cwd.appendingPathComponent("data"))
        rootCandidates.append(cwd.deletingLastPathComponent().appendingPathComponent("data"))
        let data = rootCandidates.first { candidate in
            var isDirectory: ObjCBool = false
            return fileManager.fileExists(atPath: candidate.appendingPathComponent("profiles").path,
                                          isDirectory: &isDirectory) && isDirectory.boolValue
        }
        self.dataRoot = data
        self.profilesRoot = data?.appendingPathComponent("profiles")
        self.ocioRoot = data?.appendingPathComponent("ocio")

        if let ocioRoot {
            let preferred = ocioRoot.appendingPathComponent("studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio")
            if fileManager.fileExists(atPath: preferred.path) {
                self.ocioConfigURL = preferred
            } else {
                let entries = (try? fileManager.contentsOfDirectory(at: ocioRoot,
                                                                     includingPropertiesForKeys: nil)) ?? []
                self.ocioConfigURL = entries.first { $0.pathExtension.lowercased() == "ocio" }
            }
        } else {
            self.ocioConfigURL = nil
        }

        if let profilesRoot {
            let filters = profilesRoot.appendingPathComponent("neutral_print_filters.json")
            self.filtersURL = fileManager.fileExists(atPath: filters.path) ? filters : nil
        } else {
            self.filtersURL = nil
        }
    }

    // MARK: - Commands

    func capabilities() async throws -> EngineCapabilities {
        let result = try await run(["capabilities", "--json"])
        guard result.status == 0 else { throw EngineError.commandFailed(result.stderr) }
        guard let data = result.stdout.data(using: .utf8) else {
            throw EngineError.decoding("empty capabilities output")
        }
        do {
            return try JSONDecoder().decode(EngineCapabilities.self, from: data)
        } catch {
            throw EngineError.decoding("capabilities: \(error.localizedDescription)")
        }
    }

    func catalog() async throws -> StockCatalog {
        guard let profilesRoot else { throw EngineError.commandFailed("找不到 data/profiles") }
        let result = try await run(["catalog", "--json", "--dir", profilesRoot.path])
        guard result.status == 0 else { throw EngineError.commandFailed(result.stderr) }
        guard let data = result.stdout.data(using: .utf8) else {
            throw EngineError.decoding("empty catalog output")
        }
        do {
            return try JSONDecoder().decode(StockCatalog.self, from: data)
        } catch {
            throw EngineError.decoding("catalog: \(error.localizedDescription)")
        }
    }

    func ocioInfo(config: URL) async throws -> OcioInfo {
        let result = try await run(["ocio-info", "--json", "--ocio-config", config.path])
        guard result.status == 0 else { throw EngineError.commandFailed(result.stderr) }
        guard let data = result.stdout.data(using: .utf8) else {
            throw EngineError.decoding("empty ocio-info output")
        }
        do {
            return try JSONDecoder().decode(OcioInfo.self, from: data)
        } catch {
            throw EngineError.decoding("ocio-info: \(error.localizedDescription)")
        }
    }

    func probe(url: URL) async -> ProbeData {
        guard let ffprobeURL else { return ProbeData() }
        let args = [
            "-v", "error", "-select_streams", "v:0",
            "-show_streams", "-show_format", "-of", "json", url.path
        ]
        guard let result = try? await run(args, tool: ffprobeURL), result.status == 0 else {
            return ProbeData()
        }
        guard let object = try? JSONSerialization.jsonObject(with: Data(result.stdout.utf8)),
              let raw = object as? [String: Any] else {
            return ProbeData()
        }
        return Self.parseProbe(raw)
    }

    func extractFrame(url: URL, at seconds: Double, output: URL) async throws {
        guard let ffmpegURL else { throw EngineError.toolNotFound("ffmpeg") }
        try FileManager.default.createDirectory(at: output.deletingLastPathComponent(),
                                                withIntermediateDirectories: true)
        let args = [
            "-v", "error", "-y",
            "-ss", String(format: "%.6f", max(0, seconds)),
            "-i", url.path,
            "-frames:v", "1", "-pix_fmt", "rgb24",
            output.path
        ]
        let result = try await run(args, tool: ffmpegURL)
        guard result.status == 0 else {
            throw EngineError.commandFailed(result.stderr)
        }
    }

    /// D074 #4: decode the source to display sRGB without running the film
    /// chain. `convert` is the same path as the CLI's "hold to compare" and
    /// supports both the built-in camera transfers and OCIO inputs.
    func convertToDisplay(settings: LookSettings, ocioConfig: URL?,
                          input: URL, output: URL) async throws -> ProcessResult {
        var args = ["convert", "--in", input.path, "--out", output.path, "--bits", "8"]
        if settings.ocioEnabled, let ocioConfig, !settings.ocioInput.isEmpty {
            args += ["--ocio-config", ocioConfig.path, "--ocio-input", settings.ocioInput]
        } else {
            args += ["--from", settings.input.rawValue]
        }
        let result = try await run(args)
        guard result.status == 0 else {
            throw EngineError.commandFailed(result.stderr.isEmpty ? result.stdout : result.stderr)
        }
        return result
    }

    func lutURL(for settings: LookSettings) -> URL {
        cacheDirectory
            .appendingPathComponent("luts", isDirectory: true)
            .appendingPathComponent("\(settings.bakeCacheKey).cube")
    }

    @discardableResult
    func ensureLUT(settings: LookSettings, filmPath: URL, paperPath: URL?, ocioConfig: URL?) async throws -> URL {
        let output = lutURL(for: settings)
        if FileManager.default.fileExists(atPath: output.path) {
            return output
        }
        try FileManager.default.createDirectory(at: output.deletingLastPathComponent(),
                                                withIntermediateDirectories: true)
        let args = settings.bakeArguments(filmPath: filmPath, paperPath: paperPath,
                                          filterPath: filtersURL, ocioConfig: ocioConfig,
                                          output: output)
        let result = try await run(args)
        guard result.status == 0 else {
            throw EngineError.commandFailed(result.stderr.isEmpty ? result.stdout : result.stderr)
        }
        guard FileManager.default.fileExists(atPath: output.path) else {
            throw EngineError.commandFailed("LUT 烘焙没有生成文件。")
        }
        return output
    }

    func render(settings: LookSettings, asset: MediaAsset, filmPath: URL, paperPath: URL?,
                ocioConfig: URL?, lutPath: URL?, output: URL,
                onLine: @escaping (String) -> Void) async throws -> ProcessResult {
        let args = settings.videoArguments(asset: asset, filmPath: filmPath, paperPath: paperPath,
                                           filterPath: filtersURL, ocioConfig: ocioConfig,
                                           lutPath: lutPath, ffmpeg: ffmpegURL,
                                           output: output)
        return try await stream(args, onLine: onLine)
    }

    /// D074 #3: low-resolution playback proxy (display or film). The UI holds
    /// a second EngineClient for this so cancelling a stale proxy never kills
    /// a final render.
    func renderProxy(settings: LookSettings, asset: MediaAsset, filmPath: URL?,
                     paperPath: URL?, ocioConfig: URL?, lutPath: URL?,
                     displayOnly: Bool, maxSide: Int, output: URL,
                     onLine: @escaping (String) -> Void) async throws -> ProcessResult {
        let args: [String]
        if displayOnly {
            args = settings.displayProxyArguments(asset: asset, ocioConfig: ocioConfig,
                                                  ffmpeg: ffmpegURL, output: output,
                                                  maxSide: maxSide)
        } else {
            guard let filmPath else {
                throw EngineError.commandFailed("胶片参数还没准备好。")
            }
            args = settings.videoArguments(asset: asset, filmPath: filmPath,
                                           paperPath: paperPath, filterPath: filtersURL,
                                           ocioConfig: ocioConfig, lutPath: lutPath,
                                           ffmpeg: ffmpegURL, output: output,
                                           maxSide: maxSide)
        }
        return try await stream(args, onLine: onLine)
    }

    func renderStill(settings: LookSettings, asset: MediaAsset, filmPath: URL, paperPath: URL?,
                     ocioConfig: URL?, lutPath: URL?, inputImage: URL,
                     output: URL) async throws -> ProcessResult {
        let args = settings.stillArguments(asset: asset, filmPath: filmPath, paperPath: paperPath,
                                           filterPath: filtersURL, ocioConfig: ocioConfig,
                                           lutPath: lutPath, inputImage: inputImage, output: output)
        let result = try await run(args)
        guard result.status == 0 else {
            throw EngineError.commandFailed(result.stderr.isEmpty ? result.stdout : result.stderr)
        }
        return result
    }

    func cancel() {
        processLock.lock()
        let process = currentProcess
        currentProcess = nil
        processLock.unlock()
        process?.terminate()
    }

    var workingDirectory: URL? {
        dataRoot?.deletingLastPathComponent()
    }

    var cacheDirectory: URL {
        let base = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSTemporaryDirectory())
        return base.appendingPathComponent("SpektraRoll", isDirectory: true)
    }

    // MARK: - Process plumbing

    func run(_ arguments: [String], tool: URL? = nil) async throws -> ProcessResult {
        guard let executable = tool ?? cliURL else { throw EngineError.cliNotFound }
        let workingDirectory = self.workingDirectory
        return try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<ProcessResult, Error>) in
            DispatchQueue.global(qos: .userInitiated).async {
                do {
                    let result = try Self.runSync(executable: executable,
                                                  arguments: arguments,
                                                  workingDirectory: workingDirectory)
                    continuation.resume(returning: result)
                } catch {
                    continuation.resume(throwing: error)
                }
            }
        }
    }

    func stream(_ arguments: [String], onLine: @escaping (String) -> Void) async throws -> ProcessResult {
        guard let executable = cliURL else { throw EngineError.cliNotFound }
        let workingDirectory = self.workingDirectory
        return try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<ProcessResult, Error>) in
            DispatchQueue.global(qos: .userInitiated).async {
                do {
                    let process = Process()
                    process.executableURL = executable
                    process.arguments = arguments
                    process.currentDirectoryURL = workingDirectory
                    let outPipe = Pipe()
                    let errPipe = Pipe()
                    process.standardOutput = outPipe
                    process.standardError = errPipe
                    try process.run()
                    self.setCurrent(process)

                    let stdoutBox = DataBox()
                    let stderrBox = DataBox()
                    let group = DispatchGroup()

                    group.enter()
                    DispatchQueue.global(qos: .userInitiated).async {
                        var buffer = Data()
                        let handle = outPipe.fileHandleForReading
                        while true {
                            let chunk = handle.availableData
                            if chunk.isEmpty { break }
                            stdoutBox.append(chunk)
                            buffer.append(chunk)
                            while let newline = buffer.firstIndex(of: 0x0A) {
                                let lineData = buffer[buffer.startIndex..<newline]
                                buffer.removeSubrange(buffer.startIndex...newline)
                                if let line = String(data: lineData, encoding: .utf8) {
                                    onLine(line)
                                }
                            }
                        }
                        if !buffer.isEmpty, let line = String(data: buffer, encoding: .utf8), !line.isEmpty {
                            onLine(line)
                        }
                        group.leave()
                    }

                    group.enter()
                    DispatchQueue.global(qos: .userInitiated).async {
                        stderrBox.set(errPipe.fileHandleForReading.readDataToEndOfFile())
                        group.leave()
                    }

                    process.waitUntilExit()
                    self.setCurrent(nil)
                    group.wait()
                    continuation.resume(returning: ProcessResult(
                        status: process.terminationStatus,
                        stdout: String(data: stdoutBox.value, encoding: .utf8) ?? "",
                        stderr: String(data: stderrBox.value, encoding: .utf8) ?? ""
                    ))
                } catch {
                    continuation.resume(throwing: error)
                }
            }
        }
    }

    private func setCurrent(_ process: Process?) {
        processLock.lock()
        currentProcess = process
        processLock.unlock()
    }

    private static func runSync(executable: URL, arguments: [String],
                                workingDirectory: URL?) throws -> ProcessResult {
        let process = Process()
        process.executableURL = executable
        process.arguments = arguments
        process.currentDirectoryURL = workingDirectory
        let outPipe = Pipe()
        let errPipe = Pipe()
        process.standardOutput = outPipe
        process.standardError = errPipe
        try process.run()

        let stdoutBox = DataBox()
        let stderrBox = DataBox()
        let group = DispatchGroup()
        group.enter()
        DispatchQueue.global(qos: .userInitiated).async {
            stdoutBox.set(outPipe.fileHandleForReading.readDataToEndOfFile())
            group.leave()
        }
        group.enter()
        DispatchQueue.global(qos: .userInitiated).async {
            stderrBox.set(errPipe.fileHandleForReading.readDataToEndOfFile())
            group.leave()
        }
        process.waitUntilExit()
        group.wait()

        return ProcessResult(status: process.terminationStatus,
                             stdout: String(data: stdoutBox.value, encoding: .utf8) ?? "",
                             stderr: String(data: stderrBox.value, encoding: .utf8) ?? "")
    }

    private static func firstExecutable(_ candidates: [URL], pathName: String) -> URL? {
        let fileManager = FileManager.default
        for candidate in candidates where fileManager.isExecutableFile(atPath: candidate.path) {
            return candidate
        }
        let path = ProcessInfo.processInfo.environment["PATH"] ?? ""
        for directory in path.split(separator: ":") {
            let candidate = URL(fileURLWithPath: String(directory)).appendingPathComponent(pathName)
            if fileManager.isExecutableFile(atPath: candidate.path) {
                return candidate
            }
        }
        return nil
    }

    private static func parseProbe(_ raw: [String: Any]) -> ProbeData {
        var data = ProbeData()
        let streams = raw["streams"] as? [[String: Any]] ?? []
        guard let stream = streams.first else { return data }
        let format = raw["format"] as? [String: Any] ?? [:]

        data.width = intValue(stream["width"]) ?? 0
        data.height = intValue(stream["height"]) ?? 0
        data.codec = stream["codec_name"] as? String ?? ""
        data.colorSpace = stream["color_space"] as? String ?? ""
        data.colorTransfer = stream["color_transfer"] as? String ?? ""
        data.colorPrimaries = stream["color_primaries"] as? String ?? ""

        let rate = stream["r_frame_rate"] as? String ?? stream["avg_frame_rate"] as? String ?? ""
        data.fps = parseRate(rate)
        data.duration = doubleValue(stream["duration"]) ?? doubleValue(format["duration"]) ?? 0
        data.frames = intValue(stream["nb_frames"]) ?? 0
        if data.frames == 0, data.fps > 0, data.duration > 0 {
            data.frames = Int((data.duration * data.fps).rounded())
        }

        let sideData = stream["side_data_list"] as? [[String: Any]] ?? []
        for item in sideData {
            if let rotation = intValue(item["rotation"]),
               rotation == 90 || rotation == 270 || rotation == -90 || rotation == -270 {
                swap(&data.width, &data.height)
                break
            }
        }

        let streamTags = stream["tags"] as? [String: Any] ?? [:]
        let formatTags = format["tags"] as? [String: Any] ?? [:]
        data.timecode = streamTags["timecode"] as? String
            ?? formatTags["timecode"] as? String
            ?? ""

        if data.width == 0 || data.height == 0 {
            let streamsWithSize = streams.first { intValue($0["width"]) != nil }
            if let streamWithSize = streamsWithSize {
                data.width = intValue(streamWithSize["width"]) ?? data.width
                data.height = intValue(streamWithSize["height"]) ?? data.height
            }
        }
        return data
    }

    private static func parseRate(_ value: String) -> Double {
        let parts = value.split(separator: "/")
        if parts.count == 2, let numerator = Double(String(parts[0])),
           let denominator = Double(String(parts[1])), denominator != 0 {
            return numerator / denominator
        }
        return Double(value) ?? 0
    }

    private static func intValue(_ value: Any?) -> Int? {
        if let number = value as? NSNumber { return number.intValue }
        if let string = value as? String { return Int(string) }
        return nil
    }

    private static func doubleValue(_ value: Any?) -> Double? {
        if let number = value as? NSNumber { return number.doubleValue }
        if let string = value as? String { return Double(string) }
        return nil
    }
}
