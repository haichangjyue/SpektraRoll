// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Optional RED R3D reader (D068). The RED SDK is never bundled with the
// project or an official binary: a user who has accepted RED's EULA points the
// build at their own SDK with -DSR_WITH_REDSDK=ON. Without that option this
// header is backed by red_stub.cpp, so every command still links and can give a
// clear "this build has no RED support" message.
#pragma once

#include <cctype>
#include <string>

namespace sr {

// True for a path whose extension is .r3d (case-insensitive). This is only a
// filename check; it does not open the file and works in every build.
inline bool redIsRedFile(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= path.size()) return false;
    std::string suffix = path.substr(dot + 1);
    for (char& c : suffix) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return suffix == "r3d";
}

struct RedRuntime {
    bool built = false;      // compiled with SR_WITH_REDSDK
    bool available = false;  // SDK initialized successfully
    std::string sdkVersion;
    std::string libraryPath;  // folder passed to InitializeSdk
    std::string message;      // human-readable state / failure
};

struct RedClip {
    int width = 0;
    int height = 0;
    long long frames = 0;
    double fps = 0.0;
    int audioChannels = 0;
    int minimumColorVersion = 0;
    std::string timecode;
};

// Initializes the SDK once per process (lazily) and reports the result. Safe to
// call from any command; the stub returns built=false.
RedRuntime redRuntime();

// Reads clip metadata through the SDK if this build has it. Frame decoding is
// intentionally not part of the D068 skeleton; `video` still explains that it
// needs the REDCINE-X / ProRes 4444 path for now.
bool redProbeClip(const std::string& path, RedClip& out, std::string& error);

}  // namespace sr
