// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Optional RED R3D reader (D068/D069). The RED SDK is never bundled with the
// project or an official binary. A local `spektra-red` plugin (built with MSVC
// against the user's own SDK) is loaded at runtime; the core executable only
// speaks this C++ interface and never includes RED headers.
#pragma once

#include <cctype>
#include <memory>
#include <string>

#include "sr/image.hpp"

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
    bool built = false;      // the optional plugin was found and loaded
    bool available = false;  // the plugin initialized the RED SDK
    std::string sdkVersion;
    std::string libraryPath;  // plugin path, for diagnostics
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

enum class RedFormat { Log3G10, Linear, Srgb };
enum class RedScale { Full, Half, Quarter };

// Holds one open R3D clip and reuses the plugin's aligned decode buffer.
class RedReader {
public:
    RedReader();
    ~RedReader();
    RedReader(RedReader&&) noexcept;
    RedReader& operator=(RedReader&&) noexcept;
    RedReader(const RedReader&) = delete;
    RedReader& operator=(const RedReader&) = delete;

    bool open(const std::string& path, RedClip& clip, std::string& error);
    void close();
    bool isOpen() const;

    // Decodes one frame into `out` as 0..1 samples. Log3G10 is the LUT-domain
    // format; Linear/Srgb are Rec.709 for direct viewing.
    bool decode(long long frame, RedFormat format, RedScale scale, Image& out,
                std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Returns the plugin/SDK state. Safe to call from any command.
RedRuntime redRuntime();

// Convenience wrapper for `red-info --in`: opens the clip, fills metadata,
// closes it.
bool redProbeClip(const std::string& path, RedClip& out, std::string& error);

}  // namespace sr
