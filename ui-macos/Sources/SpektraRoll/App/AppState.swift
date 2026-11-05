// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import Combine
import Foundation
import UniformTypeIdentifiers

enum ImportMode: String, CaseIterable, Identifiable {
    case transform
    case ocio

    var id: String { rawValue }
    var title: String {
        switch self {
        case .transform: return "画面类型"
        case .ocio: return "色彩管理（OCIO）"
        }
    }
}

struct ImportDraft: Identifiable {
    let id = UUID()
    let urls: [URL]
}

@MainActor
final class AppState: ObservableObject {
    let engine = EngineClient()

    @Published var capabilities: EngineCapabilities?
    @Published var catalog: StockCatalog?
    @Published var ocioInfo: OcioInfo?

    @Published var assets: [MediaAsset] = []
    @Published var selectedAssetID: MediaAsset.ID?
    @Published var settings = LookSettings()
    @Published var inspectorTab: InspectorTab = .info
    @Published var importDraft: ImportDraft?

    @Published var statusMessage = "正在连接引擎…"
    @Published var errorMessage: String?

    @Published var isRendering = false
    @Published var renderProgress: Double = 0
    @Published var renderStatus = ""
    @Published var renderLog: [String] = []

    @Published var showFilmPreview = false
    @Published var filmPreview: NSImage?
    @Published var isPreviewing = false

    // D074 #4: the unfiltered view is the source decoded to display sRGB, not
    // the log-coded AVPlayer frame.
    @Published var sourcePreview: NSImage?
    @Published var isSourcePreviewing = false

    // D074 #1: histogram and exposure references for the frame on screen.
    @Published var frameStats: FrameStats?

    // D074 #3: low-resolution playback proxies. The display proxy is the
    // source decoded to sRGB; the film proxy carries the current look.
    @Published var displayProxyURL: URL?
    @Published var filmProxyURL: URL?
    @Published var isGeneratingProxy = false
    @Published var isGeneratingFilmProxy = false
    @Published var proxyProgress: Double = 0
    @Published var proxyStatus = ""
    @Published var isAutoGrading = false

    // D077: proxies live in a user-selectable folder with a soft size cap.
    @Published var proxyCacheLimitGB: Double = 5.0
    @Published var proxyCacheDirectory: URL
    @Published var proxyCacheBytes: Int64 = 0

    @Published var outputDirectory: URL
    @Published var outputName = "grade"

    // D082: neutral filter pack and render queue.
    @Published var neutralFilters: FilterSolve?
    @Published var renderJobs: [RenderJob] = []
    @Published var isQueueRunning = false

    private let proxyEngine = EngineClient()
    private var proxyTask: Task<Void, Never>?
    private var proxyTaskKey: String?
    private var pendingProxy: ProxyRequest?
    private var proxyDebounceTask: Task<Void, Never>?
    private var proxyGeneration = 0
    private var queueTask: Task<Void, Never>?
    private var activeJobID: UUID?

    private var didBootstrap = false
    private var previewTask: Task<Void, Never>?
    private var previewDebounceTask: Task<Void, Never>?
    private var previewDirty = false
    private var pendingPreviewTime: Double = 0
    private var previewGeneration = 0

    private var sourcePreviewTask: Task<Void, Never>?
    private var sourceDirty = false
    private var pendingSourceTime: Double = 0
    private var sourceGeneration = 0

    init() {
        let caches = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask).first
        proxyCacheDirectory = (caches ?? URL(fileURLWithPath: NSTemporaryDirectory()))
            .appendingPathComponent("SpektraRoll", isDirectory: true)
            .appendingPathComponent("preview-cache", isDirectory: true)
        let movies = FileManager.default.urls(for: .moviesDirectory, in: .userDomainMask).first
        outputDirectory = (movies ?? URL(fileURLWithPath: NSTemporaryDirectory()))
            .appendingPathComponent("SpektraRoll", isDirectory: true)
        FontBook.registerBundledFonts()
        loadPersisted()
    }

    var selectedAsset: MediaAsset? {
        guard let selectedAssetID else { return nil }
        return assets.first { $0.id == selectedAssetID }
    }

    var filmStock: StockCatalog.Stock? {
        catalog?.films.first { $0.id == settings.filmID }
    }

    var paperStock: StockCatalog.Stock? {
        catalog?.papers.first { $0.id == settings.paperID }
    }

    var ocioAvailable: Bool {
        capabilities?.ocio.available ?? false
    }

    var ocioColorSpaces: [String] {
        ocioInfo?.colorSpaces ?? []
    }

    var catalogueReady: Bool {
        catalog != nil
    }

    private struct ProxyRequest {
        let key: String
        let output: URL
        let displayOnly: Bool
        let asset: MediaAsset
        let settings: LookSettings
        let filmURL: URL?
        let paperURL: URL?
        let ocioConfig: URL?
        let maxSide: Int
    }

    func bootstrap() async {
        guard !didBootstrap else { return }
        didBootstrap = true
        do {
            let caps = try await engine.capabilities()
            capabilities = caps
            statusMessage = "引擎 \(caps.version) · \(caps.platform) \(caps.arch)"
            let stocks = try await engine.catalog()
            catalog = stocks
            if caps.ocio.available, let config = engine.ocioConfigURL {
                ocioInfo = try? await engine.ocioInfo(config: config)
            } else {
                settings.ocioEnabled = false
            }
            normalizeSelection()
            refreshNeutralFilters()
            if ProcessInfo.processInfo.environment["SPEKTRA_OCIO"] == "1",
               ocioInfo != nil {
                settings.ocioEnabled = true
                if let space = ocioColorSpaces.first(where: {
                    $0.contains("S-Log3") && $0.contains("S-Gamut3.Cine")
                }) {
                    settings.ocioInput = space
                }
            }
            if let raw = ProcessInfo.processInfo.environment["SPEKTRA_PROXY_LIMIT_GB"],
               let value = Double(raw) {
                proxyCacheLimitGB = min(1000, max(0, value))
            }
            cleanupProxyCache()
            if let tab = ProcessInfo.processInfo.environment["SPEKTRA_TAB"],
               let selected = InspectorTab(rawValue: tab.lowercased()) {
                inspectorTab = selected
            }
            if let path = ProcessInfo.processInfo.environment["SPEKTRA_AUTOLOAD"], !path.isEmpty {
                await debugAutoImport(URL(fileURLWithPath: path))
            }
        } catch {
            statusMessage = "引擎未连接"
            errorMessage = error.localizedDescription
        }
    }

    // MARK: - Library

    func presentImportPanel() {
        let panel = NSOpenPanel()
        panel.title = "导入素材或项目目录"
        panel.prompt = "导入"
        panel.allowsMultipleSelection = true
        panel.canChooseFiles = true
        panel.canChooseDirectories = true
        panel.resolvesAliases = true
        var types: [UTType] = [.movie, .mpeg4Movie, .quickTimeMovie, .folder]
        if let r3d = UTType(filenameExtension: "r3d") { types.append(r3d) }
        if let mxf = UTType(filenameExtension: "mxf") { types.append(mxf) }
        panel.allowedContentTypes = types
        panel.begin { [weak self] response in
            guard response == .OK else { return }
            Task { @MainActor in
                self?.handleImportSelection(panel.urls)
            }
        }
    }

    func handleImportSelection(_ urls: [URL]) {
        let fileManager = FileManager.default
        var found: [URL] = []
        for url in urls {
            var isDirectory: ObjCBool = false
            if fileManager.fileExists(atPath: url.path, isDirectory: &isDirectory),
               isDirectory.boolValue {
                if let enumerator = fileManager.enumerator(
                    at: url,
                    includingPropertiesForKeys: [.isRegularFileKey],
                    options: [.skipsHiddenFiles]
                ) {
                    for case let file as URL in enumerator where Self.isVideoFile(file) {
                        found.append(file)
                    }
                }
            } else if Self.isVideoFile(url) {
                found.append(url)
            }
        }
        let unique = Array(Set(found)).sorted { $0.path.localizedStandardCompare($1.path) == .orderedAscending }
        guard !unique.isEmpty else {
            statusMessage = "没有找到支持的视频文件"
            return
        }
        importDraft = ImportDraft(urls: unique)
    }

    func confirmImport(_ draft: ImportDraft, mode: ImportMode,
                       input: InputTransform, ocioInput: String) {
        importDraft = nil
        statusMessage = "正在读取素材…"
        let useOcio = mode == .ocio && ocioAvailable && !ocioInput.isEmpty
        Task {
            var imported: [MediaAsset] = []
            for url in draft.urls {
                if url.pathExtension.lowercased() == "r3d" {
                    statusMessage = "R3D 仅 Windows；macOS v1 暂不支持。"
                    continue
                }
                guard var asset = await MediaProbe.probe(url: url, engine: engine) else {
                    continue
                }
                asset.input = input
                asset.ocioEnabled = useOcio
                asset.ocioInput = useOcio ? ocioInput : ""
                if let thumb = await MediaProbe.makeThumbnail(
                    url: url,
                    cacheDirectory: engine.cacheDirectory
                ) {
                    asset.thumbnailPath = thumb
                }
                imported.append(asset)
            }
            assets.removeAll { existing in imported.contains { $0.path == existing.path } }
            assets.append(contentsOf: imported)
            if let first = imported.first {
                selectedAssetID = first.id
                settings.input = first.input
                settings.ocioEnabled = first.ocioEnabled
                if !first.ocioInput.isEmpty {
                    settings.ocioInput = first.ocioInput
                }
            }
            statusMessage = imported.isEmpty ? "没有导入素材" : "已导入 \(imported.count) 个素材"
            save()
        }
    }

    func selectAsset(_ asset: MediaAsset) {
        selectedAssetID = asset.id
        settings.input = asset.input
        settings.ocioEnabled = asset.ocioEnabled && ocioAvailable
        if !asset.ocioInput.isEmpty {
            settings.ocioInput = asset.ocioInput
        }
        if selectedAssetID != asset.id {
            clearPreviews()
        }
        statusMessage = asset.isPlayable ? asset.displayName : "R3D 仅 Windows"
        save()
    }

    func removeAsset(_ asset: MediaAsset) {
        assets.removeAll { $0.id == asset.id }
        if selectedAssetID == asset.id {
            selectedAssetID = assets.first?.id
        }
        save()
    }

    // MARK: - Materials

    func selectFilm(_ stock: StockCatalog.Stock) {
        settings.filmID = stock.id
        if stock.positive {
            settings.scan = .slide
            settings.contrast = 1.0
        } else if let paper = catalog?.papers.first(where: { $0.id == stock.targetPrint }) {
            settings.paperID = paper.id
            settings.scan = paper.printFilm ? .print : .negative
            settings.contrast = settings.scan == .negative ? 1.15 : 1.0
        } else {
            if settings.scan == .slide {
                settings.scan = .negative
            }
            settings.contrast = settings.scan == .negative ? 1.15 : 1.0
        }
        settings.gamut = settings.scan == .negative
        refreshNeutralFilters()
        save()
    }

    func selectPaper(_ stock: StockCatalog.Stock) {
        settings.paperID = stock.id
        if settings.scan != .slide {
            settings.scan = .print
            settings.contrast = 1.0
            settings.gamut = false
        }
        refreshNeutralFilters()
        save()
    }

    func setScan(_ mode: ScanMode) {
        guard let film = filmStock else { return }
        if film.positive && mode != .slide { return }
        if !film.positive && mode == .slide { return }
        settings.scan = mode
        settings.contrast = mode == .negative ? 1.15 : 1.0
        settings.gamut = mode == .negative
        refreshNeutralFilters()
        save()
    }

    /// D082: ask the CLI for the database neutral C/M/Y values so the PRINT tab
    /// can show them next to the trim offsets.
    func refreshNeutralFilters() {
        guard settings.scan != .slide,
              let film = filmStock,
              let paper = paperStock else {
            neutralFilters = nil
            return
        }
        let filmURL = URL(fileURLWithPath: film.path)
        let paperURL = URL(fileURLWithPath: paper.path)
        Task { [weak self] in
            guard let self else { return }
            do {
                let solve = try await self.engine.solveFilterPack(filmPath: filmURL,
                                                                  paperPath: paperURL)
                self.neutralFilters = solve
            } catch {
                self.neutralFilters = nil
            }
        }
    }

    func resetFilterOffsets() {
        settings.filterCOffset = 0
        settings.filterMOffset = 0
        settings.filterYOffset = 0
        save()
    }

    // MARK: - Render

    func chooseOutputDirectory() {
        let panel = NSOpenPanel()
        panel.title = "选择输出目录"
        panel.prompt = "选择"
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.directoryURL = outputDirectory
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            Task { @MainActor in
                self?.outputDirectory = url
                self?.save()
            }
        }
    }

    func render() {
        guard !isQueueRunning else {
            errorMessage = "渲染队列正在运行；先停止队列或把这条加入队列。"
            return
        }
        guard let asset = selectedAsset else {
            errorMessage = "先在左侧选择一个素材。"
            return
        }
        guard let catalog,
              let film = catalog.films.first(where: { $0.id == settings.filmID }) else {
            errorMessage = "片库还没加载完成。"
            return
        }
        cancelProxyGeneration()
        let paper = catalog.papers.first { $0.id == settings.paperID }
        let output = makeOutputURL(for: asset)
        let snapshot = settings

        isRendering = true
        renderProgress = 0
        renderLog = []
        renderStatus = "准备渲染…"

        Task {
            do {
                try FileManager.default.createDirectory(
                    at: output.deletingLastPathComponent(),
                    withIntermediateDirectories: true
                )
                let result = try await renderOutput(
                    settings: snapshot,
                    asset: asset,
                    filmURL: URL(fileURLWithPath: film.path),
                    paperURL: paper.map { URL(fileURLWithPath: $0.path) },
                    output: output
                ) { phase in
                    self.renderStatus = phase
                } onLine: { line in
                    self.consume(line)
                }
                isRendering = false
                if result.status == 0 {
                    renderProgress = 1
                    renderStatus = "完成：\(output.lastPathComponent)"
                    statusMessage = "渲染完成"
                } else {
                    renderStatus = "失败"
                    errorMessage = result.stderr.isEmpty ? "渲染失败" : result.stderr
                }
            } catch {
                isRendering = false
                renderStatus = "失败"
                errorMessage = error.localizedDescription
            }
        }
    }

    func cancelRender() {
        if isQueueRunning {
            stopRenderQueue()
            return
        }
        engine.cancel()
        isRendering = false
        renderStatus = "已取消"
    }

    /// Shared by the immediate render and the queue. The settings snapshot
    /// decides the materials, so a queued job is immune to later edits.
    private func renderOutput(settings: LookSettings, asset: MediaAsset, filmURL: URL,
                              paperURL: URL?, output: URL,
                              onPhase: @escaping (String) -> Void,
                              onLine: @escaping (String) -> Void) async throws -> ProcessResult {
        var lut: URL?
        if settings.engine == .lut {
            onPhase("烘焙 LUT…")
            lut = try await engine.ensureLUT(
                settings: settings,
                filmPath: filmURL,
                paperPath: paperURL,
                ocioConfig: engine.ocioConfigURL
            )
        }
        onPhase("渲染中…")
        return try await engine.render(
            settings: settings,
            asset: asset,
            filmPath: filmURL,
            paperPath: paperURL,
            ocioConfig: engine.ocioConfigURL,
            lutPath: lut,
            output: output,
            onLine: onLine
        )
    }

    // MARK: - Render queue (D082)

    func enqueueRender() {
        guard let asset = selectedAsset else {
            errorMessage = "先在左侧选择一个素材。"
            return
        }
        guard catalogueReady else {
            errorMessage = "片库还没加载完成。"
            return
        }
        let reserved = Set(renderJobs.map(\.outputPath))
        let output = makeOutputURL(for: asset, reserved: reserved)
        renderJobs.append(RenderJob(asset: asset, settings: settings, output: output))
        save()
        statusMessage = "已加入队列：\(asset.displayName)"
    }

    func removeRenderJob(_ id: UUID) {
        guard id != activeJobID else { return }
        renderJobs.removeAll { $0.id == id }
        save()
    }

    func clearFinishedRenderJobs() {
        renderJobs.removeAll { $0.status == .done || $0.status == .failed || $0.status == .canceled }
        save()
    }

    func startRenderQueue() {
        guard !isQueueRunning else { return }
        guard !isRendering else {
            errorMessage = "正在渲染；先等当前任务完成。"
            return
        }
        guard renderJobs.contains(where: { $0.status == .waiting }) else {
            statusMessage = "队列里没有等待中的任务"
            return
        }
        isQueueRunning = true
        queueTask = Task { [weak self] in
            await self?.runRenderQueue()
        }
    }

    func stopRenderQueue() {
        queueTask?.cancel()
        queueTask = nil
        engine.cancel()
        if let id = activeJobID, let index = renderJobs.firstIndex(where: { $0.id == id }) {
            renderJobs[index].status = .canceled
            renderJobs[index].message = "已取消"
        }
        activeJobID = nil
        isQueueRunning = false
        isRendering = false
        renderStatus = "队列已停止"
        save()
    }

    private func runRenderQueue() async {
        while !Task.isCancelled {
            guard let index = renderJobs.firstIndex(where: { $0.status == .waiting }) else {
                break
            }
            let job = renderJobs[index]
            guard let asset = assets.first(where: { $0.id == job.assetID })
                    ?? assets.first(where: { $0.path == job.assetPath }) else {
                renderJobs[index].status = .failed
                renderJobs[index].message = "素材已从片库移除"
                save()
                continue
            }
            activeJobID = job.id
            renderJobs[index].status = .running
            renderJobs[index].progress = 0
            renderJobs[index].message = "准备…"
            isRendering = true
            renderProgress = 0
            renderLog = []
            renderStatus = "队列：\(job.assetName)"
            save()

            do {
                guard let film = filmPath(for: job.settings) else {
                    throw EngineError.commandFailed("片库里找不到这条任务的胶片。")
                }
                try FileManager.default.createDirectory(
                    at: job.outputURL.deletingLastPathComponent(),
                    withIntermediateDirectories: true
                )
                let result = try await renderOutput(
                    settings: job.settings,
                    asset: asset,
                    filmURL: URL(fileURLWithPath: film),
                    paperURL: paperPath(for: job.settings).map { URL(fileURLWithPath: $0) },
                    output: job.outputURL
                ) { phase in
                    self.updateJob(job.id) { current in
                        current.message = phase
                    }
                    self.renderStatus = "队列：\(phase)"
                } onLine: { line in
                    self.consumeQueueLine(line, jobID: job.id)
                }
                if result.status == 0 {
                    self.updateJob(job.id) { current in
                        current.status = .done
                        current.progress = 1
                        current.message = "完成"
                    }
                    self.statusMessage = "队列完成：\(job.assetName)"
                } else {
                    self.updateJob(job.id) { current in
                        current.status = .failed
                        current.message = result.stderr.isEmpty ? "渲染失败" : result.stderr
                    }
                }
            } catch {
                if Task.isCancelled {
                    self.updateJob(job.id) { current in
                        current.status = .canceled
                        current.message = "已取消"
                    }
                } else {
                    self.updateJob(job.id) { current in
                        current.status = .failed
                        current.message = error.localizedDescription
                    }
                }
            }
            activeJobID = nil
            isRendering = false
            save()
        }
        isQueueRunning = false
        queueTask = nil
        if !renderJobs.contains(where: { $0.status == .waiting || $0.status == .running }) {
            renderStatus = "队列完成"
        }
        if ProcessInfo.processInfo.environment["SPEKTRA_QUEUE_SELFTEST"] == "1" {
            let lines = renderJobs.map { job in
                let exists = FileManager.default.fileExists(atPath: job.outputPath)
                return "\(job.assetName)\t\(job.status.rawValue)\t\(job.message)\t\(job.outputPath)\texists=\(exists)"
            }
            try? lines.joined(separator: "\n")
                .write(toFile: "/tmp/spektra-queue.log", atomically: true, encoding: .utf8)
        }
    }

    private func updateJob(_ id: UUID, _ body: (inout RenderJob) -> Void) {
        guard let index = renderJobs.firstIndex(where: { $0.id == id }) else { return }
        body(&renderJobs[index])
    }

    private func consumeQueueLine(_ line: String, jobID: UUID) {
        let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        renderLog.append(trimmed)
        if renderLog.count > 240 {
            renderLog.removeFirst(renderLog.count - 240)
        }
        guard trimmed.hasPrefix("{"),
              let data = trimmed.data(using: .utf8),
              let event = try? JSONDecoder().decode(RenderEvent.self, from: data) else {
            return
        }
        switch event.event {
        case "start":
            updateJob(jobID) { job in
                job.status = .running
                job.message = "渲染中…"
            }
        case "progress":
            let percent = event.percent ?? 0
            let frame = event.frame ?? 0
            let total = event.total ?? 0
            updateJob(jobID) { job in
                job.progress = max(0, min(1, percent / 100.0))
                job.message = total > 0 ? "\(frame) / \(total) 帧" : "\(frame) 帧"
            }
            renderProgress = max(0, min(1, percent / 100.0))
            renderStatus = total > 0 ? "队列：\(frame) / \(total) 帧" : "队列：\(frame) 帧"
        case "done":
            updateJob(jobID) { job in
                job.progress = 1
            }
        default:
            break
        }
    }

    private func filmPath(for settings: LookSettings) -> String? {
        catalog?.films.first { $0.id == settings.filmID }?.path
    }

    private func paperPath(for settings: LookSettings) -> String? {
        guard settings.scan != .slide else { return nil }
        return catalog?.papers.first { $0.id == settings.paperID }?.path
    }

    // MARK: - Film / display preview

    func setFilmPreview(_ enabled: Bool, at seconds: Double, renderStill: Bool = true) {
        showFilmPreview = enabled
        if enabled {
            filmPreview = nil
            if renderStill {
                previewFrame(at: seconds, immediate: true)
            }
        } else {
            filmPreview = nil
            frameStats = nil
            cancelFilmProxyGeneration()
            if renderStill {
                refreshSourcePreview(at: seconds)
            }
        }
    }

    func refreshFilmPreview(at seconds: Double) {
        guard showFilmPreview else { return }
        previewFrame(at: seconds)
    }

    /// D074 #5: coalesce slider changes so dragging a parameter does not spawn
    /// one CLI process per pixel. The last value always wins.
    func previewFrame(at seconds: Double, immediate: Bool = false) {
        guard showFilmPreview else { return }
        pendingPreviewTime = max(0, seconds)
        previewDebounceTask?.cancel()
        if immediate {
            startFilmPreview()
            return
        }
        previewDebounceTask = Task { [weak self] in
            do {
                try await Task.sleep(nanoseconds: 180_000_000)
            } catch {
                return
            }
            guard !Task.isCancelled else { return }
            self?.startFilmPreview()
        }
    }

    private func startFilmPreview() {
        guard showFilmPreview else { return }
        guard let asset = selectedAsset, asset.isPlayable else { return }
        guard let catalog,
              let film = catalog.films.first(where: { $0.id == settings.filmID }) else {
            return
        }
        if isPreviewing {
            previewDirty = true
            return
        }
        let paper = catalog.papers.first { $0.id == settings.paperID }
        let filmURL = URL(fileURLWithPath: film.path)
        let paperURL = paper.map { URL(fileURLWithPath: $0.path) }
        let frameURL = engine.cacheDirectory.appendingPathComponent("preview/frame.png")
        let gradedURL = engine.cacheDirectory.appendingPathComponent("preview/graded.png")
        let seconds = pendingPreviewTime

        previewGeneration += 1
        let generation = previewGeneration
        isPreviewing = true
        previewDirty = false
        previewTask = Task { [weak self] in
            guard let self else { return }
            do {
                try await self.engine.extractFrame(url: asset.url, at: seconds, output: frameURL)
                var lut: URL?
                if self.settings.engine == .lut {
                    lut = try await self.engine.ensureLUT(
                        settings: self.settings,
                        filmPath: filmURL,
                        paperPath: paperURL,
                        ocioConfig: self.engine.ocioConfigURL
                    )
                }
                _ = try await self.engine.renderStill(
                    settings: self.settings,
                    asset: asset,
                    filmPath: filmURL,
                    paperPath: paperURL,
                    ocioConfig: self.engine.ocioConfigURL,
                    lutPath: lut,
                    inputImage: frameURL,
                    output: gradedURL
                )
                if !Task.isCancelled,
                   self.previewGeneration == generation,
                   self.showFilmPreview,
                   !self.previewDirty,
                   let image = NSImage(contentsOf: gradedURL) {
                    self.filmPreview = image
                    self.frameStats = FrameStats.compute(from: image)
                }
            } catch {
                if !Task.isCancelled {
                    self.statusMessage = "胶片预览失败：\(error.localizedDescription)"
                }
            }
            guard self.previewGeneration == generation else { return }
            self.isPreviewing = false
            if self.previewDirty {
                self.previewDirty = false
                self.startFilmPreview()
            }
        }
    }

    /// D074 #4: decode the current source frame to display sRGB. This is what
    /// the "original" side of the film-effect toggle shows when paused.
    func refreshSourcePreview(at seconds: Double) {
        guard !showFilmPreview else { return }
        guard let asset = selectedAsset, asset.isPlayable else { return }
        pendingSourceTime = max(0, seconds)
        if isSourcePreviewing {
            sourceDirty = true
            return
        }
        let seconds = pendingSourceTime
        let frameURL = engine.cacheDirectory.appendingPathComponent("preview/source-frame.png")
        let displayURL = engine.cacheDirectory.appendingPathComponent("preview/display.png")

        sourceGeneration += 1
        let generation = sourceGeneration
        isSourcePreviewing = true
        sourceDirty = false
        sourcePreviewTask = Task { [weak self] in
            guard let self else { return }
            do {
                try await self.engine.extractFrame(url: asset.url, at: seconds, output: frameURL)
                _ = try await self.engine.convertToDisplay(
                    settings: self.settings,
                    ocioConfig: self.engine.ocioConfigURL,
                    input: frameURL,
                    output: displayURL
                )
                if !Task.isCancelled,
                   self.sourceGeneration == generation,
                   !self.showFilmPreview,
                   !self.sourceDirty,
                   let image = NSImage(contentsOf: displayURL) {
                    self.sourcePreview = image
                    self.frameStats = FrameStats.compute(from: image)
                }
            } catch {
                if !Task.isCancelled {
                    self.statusMessage = "显示解码失败：\(error.localizedDescription)"
                }
            }
            guard self.sourceGeneration == generation else { return }
            self.isSourcePreviewing = false
            if self.sourceDirty {
                self.sourceDirty = false
                self.refreshSourcePreview(at: self.pendingSourceTime)
            }
        }
    }

    private func clearPreviews() {
        previewGeneration += 1
        sourceGeneration += 1
        previewDebounceTask?.cancel()
        previewTask?.cancel()
        sourcePreviewTask?.cancel()
        previewDirty = false
        sourceDirty = false
        isPreviewing = false
        isSourcePreviewing = false
        filmPreview = nil
        sourcePreview = nil
        frameStats = nil
        clearProxyState()
    }

    // MARK: - 配光 (D079)

    /// Meter the current source frame and set the scene exposure so its median
    /// lands on 18% linear grey, then print (refresh) the film effect. The
    /// neutral enlarger pack is already chosen from the film/paper database.
    func autoGrade(at seconds: Double) {
        guard !isAutoGrading else { return }
        guard let asset = selectedAsset, asset.isPlayable else {
            statusMessage = "先选择一个素材。"
            return
        }
        guard catalogueReady else {
            statusMessage = "片库还没加载完成。"
            return
        }
        isAutoGrading = true
        statusMessage = "配光中…"
        let frameURL = engine.cacheDirectory.appendingPathComponent("preview/meter-frame.png")
        let displayURL = engine.cacheDirectory.appendingPathComponent("preview/meter-display.png")
        Task {
            defer { isAutoGrading = false }
            do {
                try await engine.extractFrame(url: asset.url, at: seconds, output: frameURL)
                _ = try await engine.convertToDisplay(
                    settings: settings,
                    ocioConfig: engine.ocioConfigURL,
                    input: frameURL,
                    output: displayURL
                )
                guard let image = NSImage(contentsOf: displayURL),
                      let stats = FrameStats.compute(from: image) else {
                    throw EngineError.commandFailed("无法读取测光帧。")
                }
                let ev = FrameMeter.exposureEV(from: stats)
                settings.exposure = ev
                save()
                showFilmPreview = true
                filmPreview = nil
                frameStats = nil
                previewFrame(at: seconds, immediate: true)
                statusMessage = String(
                    format: "配光：%+.2f EV（中性滤色片自动）",
                    ev
                )
            } catch {
                statusMessage = "配光失败：\(error.localizedDescription)"
            }
        }
    }

    // MARK: - Playback proxies (D074 #3)

    /// D082: playback proxies always use the LUT engine, so the expensive
    /// spectral chain is not re-run for a low-resolution preview. Long clips
    /// also step down in resolution to keep the first-play wait bounded.
    private func displayProxyMaxSide(for asset: MediaAsset) -> Int {
        if asset.duration > 180 { return 540 }
        if asset.duration > 60 { return 720 }
        return 960
    }

    private func filmProxyMaxSide(for asset: MediaAsset) -> Int {
        if asset.duration > 60 { return 360 }
        if asset.duration > 20 { return 540 }
        return 720
    }

    var proxyCacheUsageText: String {
        let formatter = ByteCountFormatter()
        formatter.countStyle = .binary
        let used = formatter.string(fromByteCount: proxyCacheBytes)
        guard proxyCacheLimitGB > 0 else { return "\(used)（不限制）" }
        let limit = formatter.string(fromByteCount: Int64(proxyCacheLimitGB * 1_073_741_824))
        return "\(used) / \(limit)"
    }

    private func proxyCacheURL(key: String) -> URL {
        proxyCacheDirectory.appendingPathComponent("\(key).mov")
    }

    private func fileStamp(_ url: URL) -> String {
        let attributes = try? FileManager.default.attributesOfItem(atPath: url.path)
        let size = (attributes?[.size] as? NSNumber)?.int64Value ?? 0
        let modified = (attributes?[.modificationDate] as? Date)?.timeIntervalSince1970 ?? 0
        return "\(size)-\(Int(modified))"
    }

    private func displayProxyKey(_ asset: MediaAsset) -> String {
        [
            "display",
            asset.path,
            fileStamp(asset.url),
            settings.input.rawValue,
            settings.ocioEnabled ? "ocio" : "builtin",
            settings.ocioInput,
            "\(displayProxyMaxSide(for: asset))"
        ].joined(separator: "|").fnv1aHex
    }

    private func filmProxyKey(_ asset: MediaAsset) -> String {
        [
            "film",
            asset.path,
            fileStamp(asset.url),
            settings.proxyLookKey,
            "\(filmProxyMaxSide(for: asset))"
        ].joined(separator: "|").fnv1aHex
    }

    private func proxySettings(from source: LookSettings) -> LookSettings {
        var proxy = source
        // D082: a preview does not need the spectral chain. Baking the current
        // look to a LUT once and rendering the proxy through the LUT engine is
        // several times faster at the same visual result for 360–720 px.
        proxy.engine = .lut
        // Ask for the GPU texture chain explicitly: with the LUT engine "auto"
        // would keep grain/halation on the CPU and dominate the proxy cost.
        proxy.textureEngine = .gpu
        proxy.codec = .h264
        proxy.trc = .srgb
        proxy.crf = 20
        proxy.preset = "veryfast"
        proxy.start = 0
        proxy.rangeEnabled = false
        proxy.frames = nil
        proxy.segment = 0
        proxy.jobs = 0
        proxy.resume = false
        return proxy
    }

    /// Called when the player starts or the film-effect toggle changes while
    /// playing: make sure the matching proxy is cached or being generated.
    func preparePlaybackProxy() {
        guard let asset = selectedAsset, asset.isPlayable else { return }
        if showFilmPreview {
            prepareFilmProxy(for: asset)
        } else {
            prepareDisplayProxy(for: asset)
        }
    }

    func prepareDisplayProxy(for asset: MediaAsset) {
        guard asset.isPlayable else { return }
        let key = displayProxyKey(asset)
        let output = proxyCacheURL(key: key)
        if let cached = cachedProxy(at: output) {
            displayProxyURL = cached
            return
        }
        if displayProxyURL != output {
            displayProxyURL = nil
        }
        requestProxy(ProxyRequest(
            key: key,
            output: output,
            displayOnly: true,
            asset: asset,
            settings: proxySettings(from: settings),
            filmURL: nil,
            paperURL: nil,
            ocioConfig: engine.ocioConfigURL,
            maxSide: displayProxyMaxSide(for: asset)
        ))
    }

    private func prepareFilmProxy(for asset: MediaAsset) {
        guard let catalog,
              let film = catalog.films.first(where: { $0.id == settings.filmID }) else {
            return
        }
        let paper = catalog.papers.first { $0.id == settings.paperID }
        let key = filmProxyKey(asset)
        let output = proxyCacheURL(key: key)
        if let cached = cachedProxy(at: output) {
            filmProxyURL = cached
            return
        }
        if filmProxyURL != output {
            filmProxyURL = nil
        }
        requestProxy(ProxyRequest(
            key: key,
            output: output,
            displayOnly: false,
            asset: asset,
            settings: proxySettings(from: settings),
            filmURL: URL(fileURLWithPath: film.path),
            paperURL: paper.map { URL(fileURLWithPath: $0.path) },
            ocioConfig: engine.ocioConfigURL,
            maxSide: filmProxyMaxSide(for: asset)
        ))
    }

    /// D074 #5/#3: a slider drag while playing should not regenerate for every
    /// tick. Keep showing the old proxy until the settled look is ready.
    func scheduleFilmProxyRefresh() {
        guard showFilmPreview, let asset = selectedAsset, asset.isPlayable else { return }
        proxyDebounceTask?.cancel()
        proxyDebounceTask = Task { [weak self] in
            do {
                try await Task.sleep(nanoseconds: 500_000_000)
            } catch {
                return
            }
            guard !Task.isCancelled else { return }
            self?.prepareFilmProxy(for: asset)
        }
    }

    private func cachedProxy(at url: URL) -> URL? {
        guard FileManager.default.fileExists(atPath: url.path) else { return nil }
        let attributes = try? FileManager.default.attributesOfItem(atPath: url.path)
        let size = (attributes?[.size] as? NSNumber)?.int64Value ?? 0
        return size > 4096 ? url : nil
    }

    private func requestProxy(_ request: ProxyRequest) {
        if proxyTaskKey == request.key {
            return
        }
        if proxyTask != nil {
            pendingProxy = request
            proxyEngine.cancel()
            proxyTask?.cancel()
            return
        }
        startProxy(request)
    }

    private func startProxy(_ request: ProxyRequest) {
        proxyTaskKey = request.key
        proxyGeneration += 1
        let generation = proxyGeneration
        let temp = request.output.deletingPathExtension().appendingPathExtension("part.mov")
        isGeneratingProxy = true
        isGeneratingFilmProxy = !request.displayOnly
        proxyProgress = 0
        proxyStatus = request.displayOnly
            ? "生成原片代理…"
            : "生成胶片代理（LUT \(request.maxSide)px）…"
        try? FileManager.default.createDirectory(at: proxyCacheDirectory,
                                                 withIntermediateDirectories: true)
        proxyTask = Task { [weak self] in
            guard let self else { return }
            defer { self.finishProxyTask(generation: generation) }
            do {
                var lut: URL?
                if !request.displayOnly, request.settings.engine == .lut,
                   let filmURL = request.filmURL {
                    lut = try await self.proxyEngine.ensureLUT(
                        settings: request.settings,
                        filmPath: filmURL,
                        paperPath: request.paperURL,
                        ocioConfig: request.ocioConfig
                    )
                }
                guard !Task.isCancelled else { return }
                try? FileManager.default.removeItem(at: temp)
                let result = try await self.proxyEngine.renderProxy(
                    settings: request.settings,
                    asset: request.asset,
                    filmPath: request.filmURL,
                    paperPath: request.paperURL,
                    ocioConfig: request.ocioConfig,
                    lutPath: lut,
                    displayOnly: request.displayOnly,
                    maxSide: request.maxSide,
                    output: temp
                ) { line in
                    Task { @MainActor in
                        self.consumeProxyLine(line, generation: generation)
                    }
                }
                guard !Task.isCancelled, self.proxyGeneration == generation else { return }
                guard result.status == 0 else {
                    try? FileManager.default.removeItem(at: temp)
                    self.proxyStatus = "实时预览生成失败"
                    if !result.stderr.isEmpty {
                        self.statusMessage = "实时预览生成失败：\(result.stderr)"
                    }
                    return
                }
                try? FileManager.default.removeItem(at: request.output)
                try FileManager.default.moveItem(at: temp, to: request.output)
                if request.displayOnly {
                    self.displayProxyURL = request.output
                } else {
                    self.filmProxyURL = request.output
                }
                self.proxyProgress = 1
                self.proxyStatus = "实时预览就绪"
                self.cleanupProxyCache(keeping: [request.output])
            } catch {
                if !Task.isCancelled {
                    self.proxyStatus = "实时预览生成失败"
                    self.statusMessage = "实时预览生成失败：\(error.localizedDescription)"
                }
            }
        }
    }

    private func finishProxyTask(generation: Int) {
        proxyTask = nil
        proxyTaskKey = nil
        if proxyGeneration == generation {
            isGeneratingProxy = false
        }
        if let pending = pendingProxy {
            pendingProxy = nil
            startProxy(pending)
        } else {
            isGeneratingFilmProxy = false
        }
    }

    private func consumeProxyLine(_ line: String, generation: Int) {
        guard generation == proxyGeneration else { return }
        let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.hasPrefix("{"),
              let data = trimmed.data(using: .utf8),
              let event = try? JSONDecoder().decode(RenderEvent.self, from: data) else {
            return
        }
        switch event.event {
        case "progress":
            if let percent = event.percent, percent >= 0 {
                proxyProgress = percent / 100.0
            }
        case "done":
            proxyProgress = 1
        default:
            break
        }
    }

    private func clearProxyState() {
        proxyDebounceTask?.cancel()
        proxyEngine.cancel()
        proxyTask?.cancel()
        proxyTask = nil
        proxyTaskKey = nil
        pendingProxy = nil
        proxyGeneration += 1
        isGeneratingProxy = false
        isGeneratingFilmProxy = false
        proxyProgress = 0
        proxyStatus = ""
        displayProxyURL = nil
        filmProxyURL = nil
    }

    private func cancelProxyGeneration() {
        proxyDebounceTask?.cancel()
        proxyEngine.cancel()
        proxyTask?.cancel()
        proxyTask = nil
        proxyTaskKey = nil
        pendingProxy = nil
        proxyGeneration += 1
        isGeneratingProxy = false
        isGeneratingFilmProxy = false
    }

    private func cancelFilmProxyGeneration() {
        guard isGeneratingFilmProxy else { return }
        proxyEngine.cancel()
        proxyTask?.cancel()
        if let pending = pendingProxy, !pending.displayOnly {
            pendingProxy = nil
        }
        if proxyTask == nil {
            isGeneratingFilmProxy = false
        }
    }

    private func cleanupProxyCache(keeping: Set<URL> = []) {
        let fileManager = FileManager.default
        let directory = proxyCacheDirectory
        guard let files = try? fileManager.contentsOfDirectory(
            at: directory,
            includingPropertiesForKeys: [.contentModificationDateKey, .fileSizeKey],
            options: [.skipsHiddenFiles]
        ) else {
            proxyCacheBytes = 0
            return
        }
        let now = Date()
        let cutoff = now.addingTimeInterval(-7 * 24 * 60 * 60)
        let partCutoff = now.addingTimeInterval(-24 * 60 * 60)
        var entries: [(url: URL, size: Int64, date: Date, isPart: Bool)] = []

        for file in files {
            let isPart = file.lastPathComponent.hasSuffix(".part.mov")
            guard isPart || file.pathExtension.lowercased() == "mov" else { continue }
            let values = try? file.resourceValues(forKeys: [.contentModificationDateKey, .fileSizeKey])
            let modified = values?.contentModificationDate ?? .distantPast
            let size = Int64(values?.fileSize ?? 0)
            if isPart, modified < partCutoff {
                try? fileManager.removeItem(at: file)
                continue
            }
            if !isPart, modified < cutoff, !keeping.contains(file) {
                try? fileManager.removeItem(at: file)
                continue
            }
            entries.append((file, size, modified, isPart))
        }

        var total = entries.reduce(Int64(0)) { $0 + $1.size }
        let limit = Int64(max(0, proxyCacheLimitGB) * 1_073_741_824)
        if limit > 0, total > limit {
            var protected = keeping
            if let displayProxyURL { protected.insert(displayProxyURL) }
            if let filmProxyURL { protected.insert(filmProxyURL) }
            // Never delete an in-progress part; only finished proxies are
            // candidates for the size cap.
            for entry in entries.filter({ !$0.isPart }).sorted(by: { $0.date < $1.date }) {
                if total <= limit { break }
                guard !protected.contains(entry.url) else { continue }
                try? fileManager.removeItem(at: entry.url)
                if !fileManager.fileExists(atPath: entry.url.path) {
                    total -= entry.size
                }
            }
        }
        proxyCacheBytes = total
    }

    /// D077: called from ADVANCED when the user edits the size cap.
    func proxyCacheLimitChanged() {
        proxyCacheLimitGB = min(1000, max(0, proxyCacheLimitGB))
        cleanupProxyCache()
        save()
    }

    func chooseProxyCacheDirectory() {
        let panel = NSOpenPanel()
        panel.title = "选择代理缓存目录"
        panel.prompt = "选择"
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.directoryURL = proxyCacheDirectory
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            Task { @MainActor in
                guard let self else { return }
                self.cancelProxyGeneration()
                let old = self.proxyCacheDirectory
                let moved = self.migrateProxyCache(from: old, to: url)
                self.proxyCacheDirectory = url
                self.clearProxyState()
                self.refreshProxyCacheUsage()
                self.statusMessage = moved > 0 ? "已迁移 \(moved) 个代理" : "代理缓存目录已切换"
                self.save()
            }
        }
    }

    /// D082: moving the cache folder now carries the existing proxies with it.
    /// Only `.mov` / `.part.mov` files are touched; a cross-volume move falls
    /// back to copy + delete.
    private func migrateProxyCache(from old: URL, to new: URL) -> Int {
        guard old.standardizedFileURL != new.standardizedFileURL else { return 0 }
        let fileManager = FileManager.default
        try? fileManager.createDirectory(at: new, withIntermediateDirectories: true)
        guard let files = try? fileManager.contentsOfDirectory(
            at: old,
            includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles]
        ) else {
            return 0
        }
        var moved = 0
        for file in files where file.pathExtension.lowercased() == "mov" {
            let destination = new.appendingPathComponent(file.lastPathComponent)
            if fileManager.fileExists(atPath: destination.path) {
                try? fileManager.removeItem(at: file)
                continue
            }
            do {
                try fileManager.moveItem(at: file, to: destination)
                moved += 1
            } catch {
                if (try? fileManager.copyItem(at: file, to: destination)) != nil {
                    try? fileManager.removeItem(at: file)
                    moved += 1
                }
            }
        }
        return moved
    }

    func clearProxyCache() {
        cancelProxyGeneration()
        let fileManager = FileManager.default
        if let files = try? fileManager.contentsOfDirectory(
            at: proxyCacheDirectory,
            includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles]
        ) {
            for file in files {
                try? fileManager.removeItem(at: file)
            }
        }
        displayProxyURL = nil
        filmProxyURL = nil
        proxyCacheBytes = 0
        statusMessage = "代理缓存已清理"
    }

    func refreshProxyCacheUsage() {
        cleanupProxyCache()
    }

    // MARK: - Persistence

    private struct PersistedState: Codable {
        var assets: [MediaAsset]
        var settings: LookSettings
        var outputDirectory: String
        var outputName: String
        var proxyCacheLimitGB: Double?
        var proxyCacheDirectory: String?
        var renderJobs: [RenderJob]?
    }

    private var stateURL: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory,
                                            in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSTemporaryDirectory())
        return base.appendingPathComponent("SpektraRoll", isDirectory: true)
            .appendingPathComponent("state.json")
    }

    func save() {
        let state = PersistedState(
            assets: assets,
            settings: settings,
            outputDirectory: outputDirectory.path,
            outputName: outputName,
            proxyCacheLimitGB: proxyCacheLimitGB,
            proxyCacheDirectory: proxyCacheDirectory.path,
            renderJobs: renderJobs
        )
        do {
            let directory = stateURL.deletingLastPathComponent()
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            let data = try encoder.encode(state)
            try data.write(to: stateURL, options: .atomic)
        } catch {
            // Persistence must never block editing.
        }
    }

    private func loadPersisted() {
        guard let data = try? Data(contentsOf: stateURL) else { return }
        guard let state = try? JSONDecoder().decode(PersistedState.self, from: data) else {
            return
        }
        assets = state.assets
        // Older debug runs could import the same path more than once; keep the
        // first entry so the library does not grow duplicates.
        var seenPaths = Set<String>()
        assets = assets.filter { seenPaths.insert($0.path).inserted }
        settings = state.settings
        outputName = state.outputName
        if !state.outputDirectory.isEmpty {
            outputDirectory = URL(fileURLWithPath: state.outputDirectory)
        }
        if let limit = state.proxyCacheLimitGB {
            proxyCacheLimitGB = min(1000, max(0, limit))
        }
        if let directory = state.proxyCacheDirectory, !directory.isEmpty {
            proxyCacheDirectory = URL(fileURLWithPath: directory, isDirectory: true)
        }
        renderJobs = (state.renderJobs ?? []).map { job in
            var restored = job
            if restored.status == .running {
                restored.status = .waiting
                restored.progress = 0
                restored.message = "上次退出时未完成"
            }
            return restored
        }
        selectedAssetID = assets.first?.id
        refreshNeutralFilters()
    }

    private func debugAutoImport(_ url: URL) async {
        guard var asset = await MediaProbe.probe(url: url, engine: engine) else { return }
        asset.input = .sLog3
        asset.ocioEnabled = false
        if let thumb = await MediaProbe.makeThumbnail(url: url,
                                                      cacheDirectory: engine.cacheDirectory) {
            asset.thumbnailPath = thumb
        }
        assets.removeAll { $0.path == asset.path }
        assets.append(asset)
        selectedAssetID = asset.id
        settings.input = .sLog3
        statusMessage = "已导入 \(asset.displayName)"
        save()

        if ProcessInfo.processInfo.environment["SPEKTRA_AUTOPREVIEW"] == "1" {
            try? await Task.sleep(nanoseconds: 1_500_000_000)
            setFilmPreview(true, at: 0)
        }
        if ProcessInfo.processInfo.environment["SPEKTRA_AUTOGRADE"] == "1" {
            try? await Task.sleep(nanoseconds: 2_000_000_000)
            autoGrade(at: 0)
        }
        if ProcessInfo.processInfo.environment["SPEKTRA_AUTORENDER"] == "1" {
            outputName = "spektra-debug-render"
            try? await Task.sleep(nanoseconds: 1_000_000_000)
            render()
        }
        if ProcessInfo.processInfo.environment["SPEKTRA_QUEUE_SELFTEST"] == "1" {
            settings.engine = .lut
            settings.codec = .h264
            settings.trc = .srgb
            settings.start = 0
            settings.rangeEnabled = true
            settings.frames = 2
            outputName = "spektra-queue-selftest-1"
            enqueueRender()
            settings.exposure += 0.3
            settings.frames = 3
            outputName = "spektra-queue-selftest-2"
            enqueueRender()
            startRenderQueue()
        }
    }

    private func normalizeSelection() {
        if selectedAssetID == nil {
            selectedAssetID = assets.first?.id
        }
        if let catalog, !catalog.films.contains(where: { $0.id == settings.filmID }) {
            if let first = catalog.films.first {
                settings.filmID = first.id
            }
        }
        if let catalog, !catalog.papers.contains(where: { $0.id == settings.paperID }) {
            if let paper = catalog.papers.first {
                settings.paperID = paper.id
            }
        }
        if let film = filmStock {
            if film.positive, settings.scan != .slide {
                settings.scan = .slide
                settings.contrast = 1.0
            } else if !film.positive, settings.scan == .slide {
                settings.scan = .negative
                settings.contrast = 1.15
            }
        }
        settings.gamut = settings.scan == .negative
        if settings.trc != .srgb, settings.codec == .h264 {
            settings.codec = .hevc
        }
        if settings.ocioInput.isEmpty {
            settings.ocioInput = ocioColorSpaces.first(where: { $0.contains("S-Log3") })
                ?? ocioColorSpaces.first
                ?? "S-Log3 S-Gamut3.Cine"
        }
    }

    private func consume(_ line: String) {
        let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        renderLog.append(trimmed)
        if renderLog.count > 240 {
            renderLog.removeFirst(renderLog.count - 240)
        }
        guard trimmed.hasPrefix("{"),
              let data = trimmed.data(using: .utf8),
              let event = try? JSONDecoder().decode(RenderEvent.self, from: data) else {
            return
        }
        switch event.event {
        case "start":
            renderProgress = 0
            renderStatus = "渲染中…"
        case "progress":
            if let percent = event.percent, percent >= 0 {
                renderProgress = percent / 100.0
            }
            let frame = event.frame ?? 0
            let total = event.total ?? 0
            renderStatus = total > 0 ? "\(frame) / \(total) 帧" : "\(frame) 帧"
        case "done":
            renderProgress = 1
            renderStatus = "完成"
        default:
            break
        }
    }

    private func makeOutputURL(for asset: MediaAsset, reserved: Set<String> = []) -> URL {
        let typed = outputName.trimmingCharacters(in: .whitespacesAndNewlines)
        let fallback = asset.url.deletingPathExtension().lastPathComponent + "-spektra"
        let raw = typed.isEmpty ? fallback : typed
        let stem = URL(fileURLWithPath: raw).deletingPathExtension().lastPathComponent
        var candidate = outputDirectory.appendingPathComponent(stem).appendingPathExtension("mov")
        var index = 2
        while FileManager.default.fileExists(atPath: candidate.path)
                || reserved.contains(candidate.path) {
            candidate = outputDirectory
                .appendingPathComponent("\(stem)-\(index)")
                .appendingPathExtension("mov")
            index += 1
        }
        return candidate
    }

    private static func isVideoFile(_ url: URL) -> Bool {
        let ext = url.pathExtension.lowercased()
        return ["mov", "mp4", "m4v", "mxf", "r3d"].contains(ext)
    }
}
