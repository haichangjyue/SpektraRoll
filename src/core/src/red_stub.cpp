// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Fallback for builds without the optional RED R3D SDK (D068). It keeps the
// interface linkable and makes every entry point explain how to get a real
// build instead of failing with a missing symbol.
#include "sr/red.hpp"

namespace sr {

RedRuntime redRuntime() {
    RedRuntime info;
    info.message =
        "this build was compiled without the RED R3D SDK; see docs/red-sdk.md "
        "(bring your own SDK, local build only)";
    return info;
}

bool redProbeClip(const std::string& path, RedClip& out, std::string& error) {
    (void)path;
    out = RedClip{};
    error = redRuntime().message;
    return false;
}

}  // namespace sr
