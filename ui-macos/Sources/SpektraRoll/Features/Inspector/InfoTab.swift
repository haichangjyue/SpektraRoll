// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

struct InfoRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            Text(label)
                .font(Theme.caption)
                .foregroundStyle(Theme.textDim)
                .frame(width: 74, alignment: .leading)
            Text(value.isEmpty ? "—" : value)
                .font(Theme.caption)
                .foregroundStyle(Theme.text)
                .textSelection(.enabled)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
    }
}

struct InfoLinkRow: View {
    let label: String
    let title: String
    let url: URL

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            Text(label)
                .font(Theme.caption)
                .foregroundStyle(Theme.textDim)
                .frame(width: 74, alignment: .leading)
            Link(title, destination: url)
                .font(Theme.caption)
                .foregroundStyle(Theme.text)
                .underline()
            Spacer(minLength: 0)
        }
    }
}

struct InfoTab: View {
    @EnvironmentObject private var app: AppState
    private let system = SystemProfile.current

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            SectionTitle("INFO")
            if let asset = app.selectedAsset {
                Panel {
                    VStack(alignment: .leading, spacing: 8) {
                        InfoRow(label: "文件", value: asset.displayName)
                        InfoRow(label: "路径", value: asset.path)
                        InfoRow(label: "分辨率", value: asset.resolutionText)
                        InfoRow(label: "帧率", value: asset.fpsText)
                        InfoRow(label: "时长", value: asset.durationText)
                        InfoRow(label: "总帧数", value: asset.framesText)
                        InfoRow(label: "编码", value: asset.codec)
                        InfoRow(label: "色彩空间", value: asset.colorSpace)
                        InfoRow(label: "传输", value: asset.colorTransfer)
                        InfoRow(label: "timecode", value: asset.timecode)
                        InfoRow(label: "输入", value: asset.inputDescription)
                    }
                }
            } else {
                EmptyStateView(title: "还没有素材",
                               subtitle: "双击左侧空白，导入视频或项目目录。",
                               icon: "film")
            }

            SectionTitle("引擎")
            Panel {
                VStack(alignment: .leading, spacing: 8) {
                    InfoRow(label: "APP", value: AppVersion.display)
                    InfoRow(label: "CLI", value: app.capabilities?.version ?? "未连接")
                    InfoRow(label: "OCIO", value: ocioText)
                    InfoRow(label: "GPU", value: gpuText)
                    InfoRow(label: "RED", value: redText)
                    InfoRow(label: "预览协议", value: previewText)
                }
            }

            SectionTitle(system.platformName)
            Panel {
                VStack(alignment: .leading, spacing: 8) {
                    InfoRow(label: "系统", value: system.osVersion)
                    InfoRow(label: "屏幕", value: system.screen)
                    InfoRow(label: "CPU", value: system.cpu)
                    InfoRow(label: "GPU", value: system.gpu)
                    InfoRow(label: "内存", value: system.ram)
                    InfoRow(label: "存储", value: system.storage)
                    InfoRow(label: "HDR", value: system.hdr)
                    InfoRow(label: "色彩空间", value: system.colorSpace)
                }
            }

            SectionTitle("关于")
            Panel {
                VStack(alignment: .leading, spacing: 8) {
                    InfoRow(label: "版本", value: AppVersion.fullDisplay)
                    InfoLinkRow(label: "源码", title: "GitHub · SpektraRoll", url: AppVersion.sourceURL)
                    InfoLinkRow(label: "反馈", title: "GitHub Issues", url: AppVersion.issuesURL)
                    InfoRow(label: "许可", value: "GPL-3.0-or-later")
                }
            }
        }
    }

    private var ocioText: String {
        guard let caps = app.capabilities else { return "—" }
        return caps.ocio.available ? "可用 \(caps.ocio.version)" : "不可用"
    }

    private var gpuText: String {
        guard let caps = app.capabilities else { return "—" }
        let backend = caps.gpu.backend
        let spectral = caps.gpu.spectralAvailable ? "光谱可用" : "光谱回退 CPU"
        let texture = caps.gpu.textureAvailable ? "质感可用" : "质感回退 CPU"
        return "\(backend) · \(spectral) · \(texture)"
    }

    private var redText: String {
        guard let caps = app.capabilities else { return "—" }
        if caps.red.available { return "可用 \(caps.red.sdkVersion)" }
        return caps.red.built ? "已构建但未初始化" : "未构建（仅 Windows 本地插件）"
    }

    private var previewText: String {
        guard let caps = app.capabilities else { return "—" }
        return "protocol \(caps.preview.protocolVersion) · \(caps.preview.frameTransport)"
    }
}
