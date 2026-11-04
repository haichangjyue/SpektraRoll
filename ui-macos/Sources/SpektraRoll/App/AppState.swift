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

    @Published var outputDirectory: URL
    @Published var outputName = "grade"

    private var didBootstrap = false
    private var previewTask: Task<Void, Never>?

    init() {
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
        filmPreview = nil
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
        save()
    }

    func selectPaper(_ stock: StockCatalog.Stock) {
        settings.paperID = stock.id
        if settings.scan != .slide {
            settings.scan = .print
            settings.contrast = 1.0
            settings.gamut = false
        }
        save()
    }

    func setScan(_ mode: ScanMode) {
        guard let film = filmStock else { return }
        if film.positive && mode != .slide { return }
        if !film.positive && mode == .slide { return }
        settings.scan = mode
        settings.contrast = mode == .negative ? 1.15 : 1.0
        settings.gamut = mode == .negative
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
        guard let asset = selectedAsset else {
            errorMessage = "先在左侧选择一个素材。"
            return
        }
        guard let catalog,
              let film = catalog.films.first(where: { $0.id == settings.filmID }) else {
            errorMessage = "片库还没加载完成。"
            return
        }
        let paper = catalog.papers.first { $0.id == settings.paperID }
        let filmURL = URL(fileURLWithPath: film.path)
        let paperURL = paper.map { URL(fileURLWithPath: $0.path) }
        let output = makeOutputURL(for: asset)

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
                var lut: URL?
                if settings.engine == .lut {
                    renderStatus = "烘焙 LUT…"
                    lut = try await engine.ensureLUT(
                        settings: settings,
                        filmPath: filmURL,
                        paperPath: paperURL,
                        ocioConfig: engine.ocioConfigURL
                    )
                }
                renderStatus = "渲染中…"
                let result = try await engine.render(
                    settings: settings,
                    asset: asset,
                    filmPath: filmURL,
                    paperPath: paperURL,
                    ocioConfig: engine.ocioConfigURL,
                    lutPath: lut,
                    output: output
                ) { line in
                    Task { @MainActor in
                        self.consume(line)
                    }
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
        engine.cancel()
        isRendering = false
        renderStatus = "已取消"
    }

    // MARK: - Film preview

    func setFilmPreview(_ enabled: Bool, at seconds: Double) {
        showFilmPreview = enabled
        if enabled {
            previewFrame(at: seconds)
        } else {
            filmPreview = nil
        }
    }

    func refreshFilmPreview(at seconds: Double) {
        guard showFilmPreview else { return }
        previewFrame(at: seconds)
    }

    func previewFrame(at seconds: Double) {
        guard let asset = selectedAsset, asset.isPlayable else { return }
        guard !isPreviewing else { return }
        guard let catalog,
              let film = catalog.films.first(where: { $0.id == settings.filmID }) else {
            return
        }
        let paper = catalog.papers.first { $0.id == settings.paperID }
        let filmURL = URL(fileURLWithPath: film.path)
        let paperURL = paper.map { URL(fileURLWithPath: $0.path) }
        let frameURL = engine.cacheDirectory.appendingPathComponent("preview/frame.png")
        let gradedURL = engine.cacheDirectory.appendingPathComponent("preview/graded.png")

        isPreviewing = true
        filmPreview = nil
        previewTask = Task {
            do {
                try await engine.extractFrame(url: asset.url, at: seconds, output: frameURL)
                var lut: URL?
                if settings.engine == .lut {
                    lut = try await engine.ensureLUT(
                        settings: settings,
                        filmPath: filmURL,
                        paperPath: paperURL,
                        ocioConfig: engine.ocioConfigURL
                    )
                }
                _ = try await engine.renderStill(
                    settings: settings,
                    asset: asset,
                    filmPath: filmURL,
                    paperPath: paperURL,
                    ocioConfig: engine.ocioConfigURL,
                    lutPath: lut,
                    inputImage: frameURL,
                    output: gradedURL
                )
                if let image = NSImage(contentsOf: gradedURL) {
                    filmPreview = image
                }
            } catch {
                statusMessage = "胶片预览失败：\(error.localizedDescription)"
            }
            isPreviewing = false
        }
    }

    // MARK: - Persistence

    private struct PersistedState: Codable {
        var assets: [MediaAsset]
        var settings: LookSettings
        var outputDirectory: String
        var outputName: String
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
            outputName: outputName
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
        settings = state.settings
        outputName = state.outputName
        if !state.outputDirectory.isEmpty {
            outputDirectory = URL(fileURLWithPath: state.outputDirectory)
        }
        selectedAssetID = assets.first?.id
    }

    private func debugAutoImport(_ url: URL) async {
        guard var asset = await MediaProbe.probe(url: url, engine: engine) else { return }
        asset.input = .sLog3
        asset.ocioEnabled = false
        if let thumb = await MediaProbe.makeThumbnail(url: url,
                                                      cacheDirectory: engine.cacheDirectory) {
            asset.thumbnailPath = thumb
        }
        assets.append(asset)
        selectedAssetID = asset.id
        settings.input = .sLog3
        statusMessage = "已导入 \(asset.displayName)"
        save()

        if ProcessInfo.processInfo.environment["SPEKTRA_AUTOPREVIEW"] == "1" {
            try? await Task.sleep(nanoseconds: 1_500_000_000)
            setFilmPreview(true, at: 0)
        }
        if ProcessInfo.processInfo.environment["SPEKTRA_AUTORENDER"] == "1" {
            outputName = "spektra-debug-render"
            try? await Task.sleep(nanoseconds: 1_000_000_000)
            render()
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

    private func makeOutputURL(for asset: MediaAsset) -> URL {
        let typed = outputName.trimmingCharacters(in: .whitespacesAndNewlines)
        let fallback = asset.url.deletingPathExtension().lastPathComponent + "-spektra"
        let raw = typed.isEmpty ? fallback : typed
        let stem = URL(fileURLWithPath: raw).deletingPathExtension().lastPathComponent
        var candidate = outputDirectory.appendingPathComponent(stem).appendingPathExtension("mov")
        var index = 2
        while FileManager.default.fileExists(atPath: candidate.path) {
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
