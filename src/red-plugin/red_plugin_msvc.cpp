/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 SpektraRoll contributors
 *
 * Optional RED R3D plugin (D069), built locally with MSVC against a user's
 * RED R3D SDK. This is the only file that includes R3DSDK.h. It exposes a C ABI
 * so the GPL MinGW CLI can load it at runtime without linking RED code.
 *
 * The plugin returns 16-bit interleaved RGB. The default format is Log3G10 in
 * REDWideGamutRGB, which is the domain of a `lut bake --ocio-input
 * "Log3G10 REDWideGamutRGB"` table. It can also return linear or sRGB Rec.709
 * for direct viewing.
 */
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "R3DSDK.h"
#include "red_plugin_api.h"

namespace {

void copyError(char* error, int errorSize, const std::string& text) {
    if (error == nullptr || errorSize <= 0) return;
    const size_t count = text.size() < static_cast<size_t>(errorSize - 1)
                             ? text.size()
                             : static_cast<size_t>(errorSize - 1);
    std::memcpy(error, text.data(), count);
    error[count] = '\0';
}

std::string moduleDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&sr_red_info), &module)) {
        return {};
    }
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    std::wstring wide(path, length);
    const size_t slash = wide.find_last_of(L"\\/");
    if (slash != std::wstring::npos) wide.resize(slash);

    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string utf8(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], bytes, nullptr, nullptr);
    utf8.resize(static_cast<size_t>(bytes - 1));
    return utf8;
}

std::string sdkRuntimePath() {
    if (const char* env = std::getenv("SPEKTRA_RED_SDK")) {
        if (*env != '\0') return env;
    }
    // build-red copies the RED redistributable DLLs next to the plugin.
    return moduleDirectory();
}

std::string loadStatusName(R3DSDK::LoadStatus status) {
    switch (status) {
        case R3DSDK::LSClipLoaded: return "clip loaded";
        case R3DSDK::LSPathNotFound: return "path not found";
        case R3DSDK::LSFailedToOpenFile: return "could not open file";
        case R3DSDK::LSNotAnR3DFile: return "not an R3D file";
        case R3DSDK::LSClipIsEmpty: return "clip has no video frames";
        case R3DSDK::LSOutOfMemory: return "out of memory";
        case R3DSDK::LSUnknownError: return "unknown error";
        case R3DSDK::LSNoClipOpen: return "no clip open";
        case R3DSDK::LSNotInitialized: return "SDK not initialized";
        default: break;
    }
    return "status " + std::to_string(static_cast<int>(status));
}

std::string decodeStatusName(R3DSDK::DecodeStatus status) {
    switch (status) {
        case R3DSDK::DSDecodeOK: return "ok";
        case R3DSDK::DSOutputBufferInvalid: return "output buffer invalid";
        case R3DSDK::DSRequestOutOfRange: return "frame out of range";
        case R3DSDK::DSInvalidParameter: return "invalid parameter";
        case R3DSDK::DSIsDroppedFrame: return "dropped frame";
        case R3DSDK::DSDecodeFailed: return "decode failed";
        case R3DSDK::DSOutOfMemory: return "out of memory";
        case R3DSDK::DSUnknownError: return "unknown error";
        case R3DSDK::DSNoClipOpen: return "no clip open";
        case R3DSDK::DSCannotReadFromFile: return "cannot read file";
        case R3DSDK::DSInvalidPixelType: return "invalid pixel type";
        case R3DSDK::DSNotAnHDRxClip: return "not an HDRx clip";
        case R3DSDK::DSCancelled: return "cancelled";
        case R3DSDK::DSUnsupportedClipFormat: return "unsupported clip format";
        case R3DSDK::DSParameterUnsupported: return "parameter unsupported";
        case R3DSDK::DSDecoderNotOpened: return "decoder not opened";
        default: break;
    }
    return "status " + std::to_string(static_cast<int>(status));
}

struct Reader {
    R3DSDK::Clip* clip = nullptr;
    void* buffer = nullptr;
    size_t bufferBytes = 0;
    int width = 0;
    int height = 0;
};

struct Runtime {
    bool initialized = false;
    bool available = false;
    std::string version;
    std::string error;
};

Runtime gRuntime;

std::once_flag gOnce;

void initialize() {
    const std::string path = sdkRuntimePath();
    if (path.empty()) {
        gRuntime.error = "could not find the RED SDK DLL folder; set SPEKTRA_RED_SDK";
        return;
    }
    const R3DSDK::InitializeStatus status =
        R3DSDK::InitializeSdk(path.c_str(), OPTION_RED_NONE);
    gRuntime.initialized = true;
    if (status != R3DSDK::ISInitializeOK) {
        gRuntime.error = "RED SDK initialization failed at " + path + " (status " +
                         std::to_string(static_cast<int>(status)) + ")";
        return;
    }
    gRuntime.available = true;
    if (const char* version = R3DSDK::GetSdkVersion()) gRuntime.version = version;
}

}  // namespace

extern "C" {

int sr_red_abi(void) { return SR_RED_PLUGIN_ABI; }

int sr_red_info(SrRedPluginInfo* out) {
    if (out == nullptr) return 0;
    std::call_once(gOnce, initialize);
    std::memset(out, 0, sizeof(*out));
    out->abi = SR_RED_PLUGIN_ABI;
    out->available = gRuntime.available ? 1 : 0;
    std::snprintf(out->version, sizeof(out->version), "%s", gRuntime.version.c_str());
    std::snprintf(out->error, sizeof(out->error), "%s", gRuntime.error.c_str());
    return 1;
}

int sr_red_open(const char* utf8_path, SrRedPluginClip* out, SrRedPluginHandle* handle, char* error,
                int errorSize) {
    if (handle == nullptr) {
        copyError(error, errorSize, "null handle");
        return 0;
    }
    *handle = nullptr;
    SrRedPluginInfo info{};
    sr_red_info(&info);
    if (!info.available) {
        copyError(error, errorSize, info.error[0] ? info.error : "RED SDK is not available");
        return 0;
    }
    if (utf8_path == nullptr || *utf8_path == '\0') {
        copyError(error, errorSize, "empty clip path");
        return 0;
    }

    auto reader = std::make_unique<Reader>();
    reader->clip = new R3DSDK::Clip(utf8_path);
    if (reader->clip->Status() != R3DSDK::LSClipLoaded) {
        copyError(error, errorSize,
                  "R3D load failed: " + loadStatusName(reader->clip->Status()));
        delete reader->clip;
        return 0;
    }
    if (out != nullptr) {
        std::memset(out, 0, sizeof(*out));
        out->width = static_cast<int>(reader->clip->Width());
        out->height = static_cast<int>(reader->clip->Height());
        out->frames = static_cast<int64_t>(reader->clip->VideoFrameCount());
        out->fps = static_cast<double>(reader->clip->VideoAudioFramerate());
        out->audio_channels = static_cast<int>(reader->clip->AudioChannelCount());
        out->min_color_version = static_cast<int>(reader->clip->MinimumColorVersion());
        if (const char* tc = reader->clip->Timecode(0)) {
            std::snprintf(out->timecode, sizeof(out->timecode), "%s", tc);
        }
    }
    *handle = reader.release();
    return 1;
}

int sr_red_decode(SrRedPluginHandle handle, int64_t frame, int format, int scale,
                  const uint16_t** pixels, int* width, int* height, char* error, int errorSize) {
    if (handle == nullptr) {
        copyError(error, errorSize, "null clip handle");
        return 0;
    }
    if (pixels == nullptr || width == nullptr || height == nullptr) {
        copyError(error, errorSize, "null output parameters");
        return 0;
    }
    auto* reader = static_cast<Reader*>(handle);
    if (frame < 0 || static_cast<size_t>(frame) >= reader->clip->VideoFrameCount()) {
        copyError(error, errorSize, "frame out of range");
        return 0;
    }

    size_t w = reader->clip->Width();
    size_t h = reader->clip->Height();
    R3DSDK::VideoDecodeMode mode = R3DSDK::DECODE_FULL_RES_PREMIUM;
    if (scale == SR_RED_SCALE_HALF) {
        w /= 2;
        h /= 2;
        mode = R3DSDK::DECODE_HALF_RES_GOOD;
    } else if (scale == SR_RED_SCALE_QUARTER) {
        w /= 4;
        h /= 4;
        mode = R3DSDK::DECODE_QUARTER_RES_GOOD;
    }
    if (w == 0 || h == 0) {
        copyError(error, errorSize, "invalid decode size");
        return 0;
    }

    const size_t bytes = w * h * 3U * 2U;
    if (reader->bufferBytes < bytes) {
        if (reader->buffer != nullptr) _aligned_free(reader->buffer);
        reader->buffer = _aligned_malloc(bytes, 16);
        reader->bufferBytes = reader->buffer != nullptr ? bytes : 0;
        if (reader->buffer == nullptr) {
            copyError(error, errorSize, "could not allocate the decode buffer");
            return 0;
        }
    }

    R3DSDK::ImageProcessingSettings settings;
    reader->clip->GetDefaultImageProcessingSettings(settings);
    settings.Version = R3DSDK::ColorVersion3;
    if (format == SR_RED_FORMAT_LOG3G10) {
        // Primary Development gives REDWideGamutRGB + Log3G10 exactly.
        settings.ImagePipelineMode = R3DSDK::Primary_Development_Only;
        settings.ColorSpace = R3DSDK::ImageColorREDWideGamutRGB;
        settings.GammaCurve = R3DSDK::ImageGammaLog3G10;
    } else {
        settings.ImagePipelineMode = R3DSDK::Full_Graded;
        settings.ColorSpace = R3DSDK::ImageColorRec709;
        settings.GammaCurve =
            format == SR_RED_FORMAT_SRGB ? R3DSDK::ImageGammaSRGB : R3DSDK::ImageGammaLinear;
        settings.OutputToneMap = R3DSDK::ToneMap_None;
        settings.HighlightRollOff = R3DSDK::RollOff_None;
    }

    R3DSDK::VideoDecodeJob job;
    job.OutputBufferSize = bytes;
    job.Mode = mode;
    job.PixelType = R3DSDK::PixelType_16Bit_RGB_Interleaved;
    job.OutputBuffer = reader->buffer;
    job.ImageProcessing = &settings;

    const R3DSDK::DecodeStatus status =
        reader->clip->DecodeVideoFrame(static_cast<size_t>(frame), job);
    if (status != R3DSDK::DSDecodeOK) {
        copyError(error, errorSize, "R3D decode failed: " + decodeStatusName(status));
        return 0;
    }

    reader->width = static_cast<int>(w);
    reader->height = static_cast<int>(h);
    *pixels = static_cast<const uint16_t*>(reader->buffer);
    *width = reader->width;
    *height = reader->height;
    return 1;
}

void sr_red_close(SrRedPluginHandle handle) {
    if (handle == nullptr) return;
    auto* reader = static_cast<Reader*>(handle);
    delete reader->clip;
    if (reader->buffer != nullptr) _aligned_free(reader->buffer);
    delete reader;
}

}  // extern "C"
