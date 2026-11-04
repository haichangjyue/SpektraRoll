/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 SpektraRoll contributors
 *
 * C ABI between the GPL SpektraRoll CLI and the optional, locally built
 * spektra-red plugin (D069). The plugin is the only piece that includes the
 * RED SDK headers; it is built with MSVC because the Windows SDK libraries are
 * MSVC-only. The core executable never links RED code or headers.
 */
#ifndef SPEKTRA_RED_PLUGIN_API_H
#define SPEKTRA_RED_PLUGIN_API_H

#include <stdint.h>

#define SR_RED_PLUGIN_ABI 1

#if defined(_WIN32)
#define SR_RED_EXPORT __declspec(dllexport)
#else
#define SR_RED_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Decode formats. LOG3G10 is the LUT-domain format used for film bakes. */
enum SrRedFormat {
    SR_RED_FORMAT_LOG3G10 = 0,
    SR_RED_FORMAT_LINEAR = 1,
    SR_RED_FORMAT_SRGB = 2
};

/* Decode scale. */
enum SrRedScale {
    SR_RED_SCALE_FULL = 0,
    SR_RED_SCALE_HALF = 1,
    SR_RED_SCALE_QUARTER = 2
};

typedef struct SrRedPluginInfo {
    int abi;
    int available;
    char version[256];
    char error[1024];
} SrRedPluginInfo;

typedef struct SrRedPluginClip {
    int width;
    int height;
    int64_t frames;
    double fps;
    int audio_channels;
    int min_color_version;
    char timecode[64];
} SrRedPluginClip;

/* Opaque clip reader. The decoded frame pointer is valid until the next call
 * to sr_red_decode or sr_red_close. */
typedef void* SrRedPluginHandle;

SR_RED_EXPORT int sr_red_abi(void);
SR_RED_EXPORT int sr_red_info(SrRedPluginInfo* out);

SR_RED_EXPORT int sr_red_open(const char* utf8_path, SrRedPluginClip* out,
                              SrRedPluginHandle* handle, char* error, int error_size);
SR_RED_EXPORT int sr_red_decode(SrRedPluginHandle handle, int64_t frame, int format, int scale,
                                const uint16_t** pixels, int* width, int* height, char* error,
                                int error_size);
SR_RED_EXPORT void sr_red_close(SrRedPluginHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* SPEKTRA_RED_PLUGIN_API_H */
