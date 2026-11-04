// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Optional RED R3D SDK adapter (D068). This file is compiled only when the
// build is configured with -DSR_WITH_REDSDK=ON and the user's SDK is found.
// The SDK and its headers are not part of this repository; see
// docs/red-sdk.md for the BYO-SDK build.
//
// D068 is a skeleton: it initializes the SDK and reports clip metadata. Frame
// decoding is deliberately left for the next delivery, so `video` can explain
// the REDCINE-X / ProRes 4444 route instead of half-working.
#include "sr/red.hpp"

#include <cstdlib>
#include <mutex>
#include <string>

#include <R3DSDK.h>

#ifndef SR_REDSDK_DEFAULT_LIB_DIR
#define SR_REDSDK_DEFAULT_LIB_DIR ""
#endif

namespace sr {
namespace {

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

std::string initializeStatusName(R3DSDK::InitializeStatus status) {
    switch (status) {
        case R3DSDK::ISInitializeOK: return "ok";
        case R3DSDK::ISLibraryNotLoaded: return "dynamic library not loaded";
        case R3DSDK::ISR3DSDKLibraryNotFound: return "R3DSDK library not found";
        case R3DSDK::ISRedCudaLibraryNotFound: return "CUDA library not found";
        case R3DSDK::ISRedOpenCLLibraryNotFound: return "OpenCL library not found";
        case R3DSDK::ISR3DDecoderLibraryNotFound: return "decoder library not found";
        case R3DSDK::ISLibraryVersionMismatch: return "library version mismatch";
        case R3DSDK::ISInvalidR3DSDKLibrary: return "invalid R3DSDK library";
        case R3DSDK::ISInvalidRedCudaLibrary: return "invalid CUDA library";
        case R3DSDK::ISInvalidRedOpenCLLibrary: return "invalid OpenCL library";
        case R3DSDK::ISInvalidR3DDecoderLibrary: return "invalid decoder library";
        case R3DSDK::ISRedCudaLibraryInitializeFailed: return "CUDA initialization failed";
        case R3DSDK::ISRedOpenCLLibraryInitializeFailed: return "OpenCL initialization failed";
        case R3DSDK::ISR3DDecoderLibraryInitializeFailed: return "decoder initialization failed";
        case R3DSDK::ISR3DSDKLibraryInitializeFailed: return "R3DSDK initialization failed";
        case R3DSDK::ISInvalidPath: return "invalid library path";
        case R3DSDK::ISInternalError: return "internal error";
        default: break;
    }
    return "status " + std::to_string(static_cast<int>(status));
}

std::string defaultLibraryPath() {
    if (const char* env = std::getenv("SPEKTRA_RED_SDK")) {
        if (*env != '\0') return env;
    }
    return SR_REDSDK_DEFAULT_LIB_DIR;
}

}  // namespace

RedRuntime redRuntime() {
    static std::once_flag once;
    static RedRuntime state;
    std::call_once(once, [] {
        state.built = true;
        state.libraryPath = defaultLibraryPath();
        if (state.libraryPath.empty()) {
            state.message =
                "RED SDK path is empty; set SPEKTRA_RED_SDK to the folder with "
                "the R3DSDK dynamic library";
            return;
        }
        const R3DSDK::InitializeStatus status =
            R3DSDK::InitializeSdk(state.libraryPath.c_str(), OPTION_RED_DECODER);
        if (status != R3DSDK::ISInitializeOK) {
            state.message = "RED SDK initialization failed (" +
                            initializeStatusName(status) + ") at " + state.libraryPath;
            return;
        }
        state.available = true;
        const char* version = R3DSDK::GetSdkVersion();
        state.sdkVersion = version != nullptr ? version : "";
        state.message = "RED SDK initialized";
    });
    return state;
}

bool redProbeClip(const std::string& path, RedClip& out, std::string& error) {
    out = RedClip{};
    const RedRuntime runtime = redRuntime();
    if (!runtime.available) {
        error = runtime.message;
        return false;
    }

    R3DSDK::Clip clip;
    const R3DSDK::LoadStatus status = clip.LoadFrom(path.c_str());
    if (status != R3DSDK::LSClipLoaded) {
        error = "R3D load failed: " + loadStatusName(status);
        return false;
    }

    out.width = static_cast<int>(clip.Width());
    out.height = static_cast<int>(clip.Height());
    out.frames = static_cast<long long>(clip.VideoFrameCount());
    out.fps = static_cast<double>(clip.VideoAudioFramerate());
    out.audioChannels = static_cast<int>(clip.AudioChannelCount());
    out.minimumColorVersion = static_cast<int>(clip.MinimumColorVersion());
    if (const char* timecode = clip.Timecode(0)) out.timecode = timecode;
    return true;
}

}  // namespace sr
