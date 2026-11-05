// swift-tools-version: 5.10
// SPDX-License-Identifier: GPL-3.0-or-later
import PackageDescription

let package = Package(
    name: "SpektraRoll",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "SpektraRoll", targets: ["SpektraRoll"])
    ],
    targets: [
        .executableTarget(
            name: "SpektraRoll",
            path: "Sources/SpektraRoll",
            linkerSettings: [
                .linkedFramework("Metal")
            ]
        )
    ]
)
