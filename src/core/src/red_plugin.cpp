// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Runtime loader for the optional spektra-red plugin (D069). The core binary
// never links the RED SDK or includes its headers: it looks for a plugin next
// to the executable (or at SPEKTRA_RED_PLUGIN) and talks to it through the C
// ABI in src/red-plugin/red_plugin_api.h.
#include "sr/red.hpp"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include "red_plugin_api.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <vector>
#endif

namespace sr {
namespace {

struct Plugin {
    bool loaded = false;
    std::string path;
    std::string message;

    decltype(&sr_red_abi) abi = nullptr;
    decltype(&sr_red_info) info = nullptr;
    decltype(&sr_red_open) open = nullptr;
    decltype(&sr_red_decode) decode = nullptr;
    decltype(&sr_red_has_audio) hasAudio = nullptr;
    decltype(&sr_red_write_audio) writeAudio = nullptr;
    decltype(&sr_red_close) close = nullptr;

#if defined(_WIN32)
    HMODULE module = nullptr;
#else
    void* module = nullptr;
#endif
};

std::string envValue(const char* name) {
    if (const char* value = std::getenv(name)) return value;
    return {};
}

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (count <= 0) return {};
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), count);
    wide.resize(static_cast<size_t>(count - 1));
    return wide;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string utf8(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &utf8[0], count, nullptr, nullptr);
    utf8.resize(static_cast<size_t>(count - 1));
    return utf8;
}

std::string executableDirectory() {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    std::wstring wide(path, length);
    const size_t slash = wide.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    wide.resize(slash);
    return wideToUtf8(wide);
}

std::wstring toNative(const std::string& path) { return utf8ToWide(path); }
#else
std::string executableDirectory() {
    uint32_t size = 4096;
    std::vector<char> buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.assign(size + 1, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
    }
    std::string path(buffer.data());
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return {};
    return path.substr(0, slash);
}
#endif

std::string pluginName() {
#if defined(_WIN32)
    return "spektra-red.dll";
#else
    return "libspektra-red.dylib";
#endif
}

Plugin& plugin() {
    static Plugin state;
    static std::once_flag once;
    std::call_once(once, [] {
        const std::string explicitPath = envValue("SPEKTRA_RED_PLUGIN");
        const std::string besideExe = executableDirectory();
        const std::string candidates[] = {
            explicitPath,
            besideExe.empty() ? std::string() : besideExe + "/" + pluginName(),
        };
        for (const std::string& candidate : candidates) {
            if (candidate.empty()) continue;
#if defined(_WIN32)
            const std::wstring native = toNative(candidate);
            state.module = LoadLibraryW(native.c_str());
            if (state.module == nullptr) continue;
            state.abi = reinterpret_cast<decltype(&sr_red_abi)>(
                GetProcAddress(state.module, "sr_red_abi"));
            state.info = reinterpret_cast<decltype(&sr_red_info)>(
                GetProcAddress(state.module, "sr_red_info"));
            state.open = reinterpret_cast<decltype(&sr_red_open)>(
                GetProcAddress(state.module, "sr_red_open"));
            state.decode = reinterpret_cast<decltype(&sr_red_decode)>(
                GetProcAddress(state.module, "sr_red_decode"));
            state.hasAudio = reinterpret_cast<decltype(&sr_red_has_audio)>(
                GetProcAddress(state.module, "sr_red_has_audio"));
            state.writeAudio = reinterpret_cast<decltype(&sr_red_write_audio)>(
                GetProcAddress(state.module, "sr_red_write_audio"));
            state.close = reinterpret_cast<decltype(&sr_red_close)>(
                GetProcAddress(state.module, "sr_red_close"));
#else
            state.module = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (state.module == nullptr) continue;
            state.abi = reinterpret_cast<decltype(&sr_red_abi)>(dlsym(state.module, "sr_red_abi"));
            state.info =
                reinterpret_cast<decltype(&sr_red_info)>(dlsym(state.module, "sr_red_info"));
            state.open =
                reinterpret_cast<decltype(&sr_red_open)>(dlsym(state.module, "sr_red_open"));
            state.decode =
                reinterpret_cast<decltype(&sr_red_decode)>(dlsym(state.module, "sr_red_decode"));
            state.hasAudio = reinterpret_cast<decltype(&sr_red_has_audio)>(
                dlsym(state.module, "sr_red_has_audio"));
            state.writeAudio = reinterpret_cast<decltype(&sr_red_write_audio)>(
                dlsym(state.module, "sr_red_write_audio"));
            state.close =
                reinterpret_cast<decltype(&sr_red_close)>(dlsym(state.module, "sr_red_close"));
#endif
            if (state.abi == nullptr || state.info == nullptr || state.open == nullptr ||
                state.decode == nullptr || state.hasAudio == nullptr ||
                state.writeAudio == nullptr || state.close == nullptr) {
                state.message = "RED plugin is missing an expected C entry point: " + candidate;
#if defined(_WIN32)
                FreeLibrary(state.module);
#else
                dlclose(state.module);
#endif
                state.module = nullptr;
                state.abi = nullptr;
                state.info = nullptr;
                state.open = nullptr;
                state.decode = nullptr;
                state.hasAudio = nullptr;
                state.writeAudio = nullptr;
                state.close = nullptr;
                continue;
            }
            if (state.abi() != SR_RED_PLUGIN_ABI) {
                state.message = "RED plugin ABI mismatch at " + candidate;
#if defined(_WIN32)
                FreeLibrary(state.module);
#else
                dlclose(state.module);
#endif
                state.module = nullptr;
                state.abi = nullptr;
                state.info = nullptr;
                state.open = nullptr;
                state.decode = nullptr;
                state.hasAudio = nullptr;
                state.writeAudio = nullptr;
                state.close = nullptr;
                continue;
            }
            state.loaded = true;
            state.path = candidate;
            state.message = "RED plugin loaded";
            return;
        }
        if (state.message.empty()) {
            state.message =
                "RED plugin not found; build it with scripts/build-red.cmd (BYO SDK) "
                "or set SPEKTRA_RED_PLUGIN";
        }
    });
    return state;
}

}  // namespace

struct RedReader::Impl {
    SrRedPluginHandle handle = nullptr;
};

RedReader::RedReader() : impl_(std::make_unique<Impl>()) {}

RedReader::~RedReader() { close(); }

RedReader::RedReader(RedReader&&) noexcept = default;
RedReader& RedReader::operator=(RedReader&&) noexcept = default;

RedRuntime redRuntime() {
    Plugin& state = plugin();
    RedRuntime runtime;
    runtime.built = state.loaded;
    runtime.libraryPath = state.path;
    runtime.message = state.message;
    if (state.loaded) {
        SrRedPluginInfo info{};
        if (state.info(&info) && info.available) {
            runtime.available = true;
            runtime.sdkVersion = info.version;
            runtime.message = "RED SDK initialized";
        } else if (info.error[0] != '\0') {
            runtime.message = info.error;
        } else {
            runtime.message = "RED plugin loaded but the SDK did not initialize";
        }
    }
    return runtime;
}

bool RedReader::open(const std::string& path, RedClip& clip, std::string& error) {
    close();
    Plugin& state = plugin();
    if (!state.loaded) {
        error = state.message;
        return false;
    }
    SrRedPluginClip info{};
    char message[1024] = {};
    if (!state.open(path.c_str(), &info, &impl_->handle, message, sizeof(message))) {
        error = message[0] != '\0' ? message : "could not open the R3D clip";
        return false;
    }
    clip.width = info.width;
    clip.height = info.height;
    clip.frames = info.frames;
    clip.fps = info.fps;
    clip.audioChannels = info.audio_channels;
    clip.audioSampleRate = info.audio_sample_rate;
    clip.minimumColorVersion = info.min_color_version;
    clip.timecode = info.timecode;
    return true;
}

void RedReader::close() {
    if (impl_ && impl_->handle != nullptr) {
        plugin().close(impl_->handle);
        impl_->handle = nullptr;
    }
}

bool RedReader::isOpen() const { return impl_ && impl_->handle != nullptr; }

bool RedReader::hasAudio() const {
    Plugin& state = plugin();
    if (!state.loaded || !impl_ || impl_->handle == nullptr) return false;
    return state.hasAudio(impl_->handle) != 0;
}

bool RedReader::writeAudio(const std::string& path, int& sampleRate, int& channels,
                           std::string& error) {
    Plugin& state = plugin();
    if (!state.loaded || !impl_ || impl_->handle == nullptr) {
        error = state.loaded ? "no R3D clip is open" : state.message;
        return false;
    }
    char message[1024] = {};
    int rate = 0;
    int count = 0;
    if (!state.writeAudio(impl_->handle, path.c_str(), &rate, &count, message, sizeof(message))) {
        error = message[0] != '\0' ? message : "could not write R3D audio";
        return false;
    }
    sampleRate = rate;
    channels = count;
    return true;
}

bool RedReader::decode(long long frame, RedFormat format, RedScale scale, Image& out,
                       std::string& error) {
    Plugin& state = plugin();
    if (!state.loaded || !impl_ || impl_->handle == nullptr) {
        error = state.loaded ? "no R3D clip is open" : state.message;
        return false;
    }
    const uint16_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    char message[1024] = {};
    if (!state.decode(impl_->handle, frame, static_cast<int>(format), static_cast<int>(scale),
                      &pixels, &width, &height, message, sizeof(message))) {
        error = message[0] != '\0' ? message : "R3D decode failed";
        return false;
    }
    if (pixels == nullptr || width <= 0 || height <= 0) {
        error = "R3D decode returned an empty frame";
        return false;
    }
    out.width = width;
    out.height = height;
    out.pixels.resize(static_cast<size_t>(width) * height * 3);
    constexpr double kScale = 1.0 / 65535.0;
    for (size_t i = 0; i < out.pixels.size(); ++i) {
        out.pixels[i] = static_cast<float>(pixels[i] * kScale);
    }
    return true;
}

bool redProbeClip(const std::string& path, RedClip& out, std::string& error) {
    RedReader reader;
    RedClip clip;
    if (!reader.open(path, clip, error)) return false;
    out = clip;
    return true;
}

}  // namespace sr
