// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Metal implementation of the per-pixel physical chain (D056). The kernel is a
// direct translation of FilmModel::transformFrom, the same one the D3D11
// backend runs (D046); the CPU spectral path stays the reference and the
// fallback. Apple-only: this file replaces gpu_film.cpp on macOS.
//
// The shader is compiled at runtime from source with newLibraryWithSource:, so
// there is no .metallib step in the build and no extra toolchain dependency.

#include "gpu_film.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace sr {
namespace {

// Kept in scalar-array form (float x[4], not float4) so the constant-buffer
// layout is byte-identical to the C++ Params struct below; float4 would force
// 16-byte alignment rules that are easy to get subtly wrong.
const char* kShaderSource = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct Params {
    float filmCurves[256][4];
    float filmCurvesCoupler[256][4];
    float paperCurves[256][4];
    float filmDensity[81][4];
    float paperDensity[81][4];
    float paperSensitivity[81][4];
    float cmfs[81][4];
    float printIlluminant[81][4];
    float viewingIlluminant[81][4];
    float filmBaseDensity[81][4];
    float paperBaseDensity[81][4];
    float filmMatrix[3][4];
    float couplersMatrix[3][4];
    float xyzD50ToSrgb[3][4];
    float filmMaxCmy[4];
    float negativeWhite[4];
    float negativeBlack[4];
    float negativeGamma[4];
    float filmAxisStart;
    float filmAxisStep;
    float paperAxisStart;
    float paperAxisStep;
    float filmMidgrayGreen;
    float printExposureFactor;
    float scanNormalization;
    float negativeGreyLuma;
    uint count;
    int couplersActive;
    int filmIsPositive;
    int scanNegative;
    int gamutCompress;
    int hdrMode;
    int appleLog;
    float exposureGain;
    float contrast;
    float peakNits;
    float strength;
    float pad1;
};

constant float kLog2Of10 = 3.321928094887362f;

float srgbDecode(float v) {
    float x = clamp(v, 0.0f, 1.0f);
    return x <= 0.04045f ? x / 12.92f : pow((x + 0.055f) / 1.055f, 2.4f);
}

float srgbEncode(float v) {
    return v <= 0.0031308f ? v * 12.92f : 1.055f * pow(v, 1.0f / 2.4f) - 0.055f;
}

float appleLogDecode(float encoded) {
    const float r0 = -0.05641088f;
    const float rt = 0.01f;
    const float sigma = 47.28711236f;
    const float beta = 0.00964052f;
    const float gamma = 0.08550479f;
    const float delta = 0.69336945f;
    const float pT = sigma * (rt - r0) * (rt - r0);
    if (encoded < 0.0f) return r0;
    if (encoded < pT) return sqrt(encoded / sigma) + r0;
    return exp2((encoded - delta) / gamma) - beta;
}

float3 bt2020ToSrgb(float3 v) {
    return float3(
        dot(float3(1.660303485421445f, -0.587570142531617f, -0.072890060215056f), v),
        dot(float3(-0.124375595308386f, 1.132834481431958f, -0.008359737187401f), v),
        dot(float3(-0.018112279959916f, -0.100583608507212f, 1.118770326157098f), v));
}

float pqEncode(float nits) {
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    float y = max(0.0f, nits) / 10000.0f;
    float p = pow(y, m1);
    return pow((c1 + c2 * p) / (1.0f + c3 * p), m2);
}

float hlgEncode(float linRgb) {
    const float a = 0.17883277f;
    const float b = 0.28466892f;
    const float c = 0.55991073f;
    float e = max(0.0f, linRgb);
    if (e <= 1.0f / 12.0f) return sqrt(3.0f * e);
    return a * log(12.0f * e - b) + c;
}

float sampleFilm(constant Params& p, float x, int c) {
    if (!isfinite(x)) return 0.0f;
    float end = p.filmAxisStart + 255.0f * p.filmAxisStep;
    if (x <= p.filmAxisStart) return p.filmCurves[0][c];
    if (x >= end) return p.filmCurves[255][c];
    float position = (x - p.filmAxisStart) / p.filmAxisStep;
    int low = min((int)position, 254);
    float t = position - float(low);
    return mix(p.filmCurves[low][c], p.filmCurves[low + 1][c], t);
}

float sampleFilmCoupler(constant Params& p, float x, int c) {
    if (!isfinite(x)) return 0.0f;
    float end = p.filmAxisStart + 255.0f * p.filmAxisStep;
    if (x <= p.filmAxisStart) return p.filmCurvesCoupler[0][c];
    if (x >= end) return p.filmCurvesCoupler[255][c];
    float position = (x - p.filmAxisStart) / p.filmAxisStep;
    int low = min((int)position, 254);
    float t = position - float(low);
    return mix(p.filmCurvesCoupler[low][c], p.filmCurvesCoupler[low + 1][c], t);
}

float samplePaper(constant Params& p, float x, int c) {
    if (!isfinite(x)) return 0.0f;
    float end = p.paperAxisStart + 255.0f * p.paperAxisStep;
    if (x <= p.paperAxisStart) return p.paperCurves[0][c];
    if (x >= end) return p.paperCurves[255][c];
    float position = (x - p.paperAxisStart) / p.paperAxisStep;
    int low = min((int)position, 254);
    float t = position - float(low);
    return mix(p.paperCurves[low][c], p.paperCurves[low + 1][c], t);
}

float3 filmCmy(constant Params& p, float3 linRgb) {
    float3 logRaw;
    logRaw.x = (p.filmMatrix[0][0] * linRgb.x + p.filmMatrix[1][0] * linRgb.y +
                p.filmMatrix[2][0] * linRgb.z) /
               p.filmMidgrayGreen;
    logRaw.y = (p.filmMatrix[0][1] * linRgb.x + p.filmMatrix[1][1] * linRgb.y +
                p.filmMatrix[2][1] * linRgb.z) /
               p.filmMidgrayGreen;
    logRaw.z = (p.filmMatrix[0][2] * linRgb.x + p.filmMatrix[1][2] * linRgb.y +
                p.filmMatrix[2][2] * linRgb.z) /
               p.filmMidgrayGreen;
    logRaw = float3(isnan(logRaw.x) ? 0.0f : logRaw.x, isnan(logRaw.y) ? 0.0f : logRaw.y,
                    isnan(logRaw.z) ? 0.0f : logRaw.z);
    logRaw = log10(max(logRaw, float3(0.0f)) + 1e-10f);

    float3 cmy = float3(sampleFilm(p, logRaw.x, 0), sampleFilm(p, logRaw.y, 1),
                        sampleFilm(p, logRaw.z, 2));
    if (p.couplersActive == 0) return cmy;

    float3 silver = p.filmIsPositive != 0
                        ? float3(p.filmMaxCmy[0], p.filmMaxCmy[1], p.filmMaxCmy[2]) - cmy
                        : cmy;
    float3 corrected;
    corrected.x = logRaw.x - dot(silver, float3(p.couplersMatrix[0][0], p.couplersMatrix[1][0],
                                                 p.couplersMatrix[2][0]));
    corrected.y = logRaw.y - dot(silver, float3(p.couplersMatrix[0][1], p.couplersMatrix[1][1],
                                                 p.couplersMatrix[2][1]));
    corrected.z = logRaw.z - dot(silver, float3(p.couplersMatrix[0][2], p.couplersMatrix[1][2],
                                                 p.couplersMatrix[2][2]));
    return float3(sampleFilmCoupler(p, corrected.x, 0), sampleFilmCoupler(p, corrected.y, 1),
                  sampleFilmCoupler(p, corrected.z, 2));
}

float3 scanPositive(constant Params& p, float3 positive) {
    float3 xyz = float3(0.0f);
    for (int l = 0; l < 81; ++l) {
        float density = p.filmDensity[l][0] * positive.x + p.filmDensity[l][1] * positive.y +
                        p.filmDensity[l][2] * positive.z;
        // Same guard as the CPU reference: undefined density contributes no light.
        float light = (density != density)
                          ? 0.0f
                          : exp2(-density * kLog2Of10) * p.viewingIlluminant[l][0];
        xyz += light * float3(p.cmfs[l][0], p.cmfs[l][1], p.cmfs[l][2]);
    }
    xyz /= p.scanNormalization;
    return float3(
        dot(float3(p.xyzD50ToSrgb[0][0], p.xyzD50ToSrgb[0][1], p.xyzD50ToSrgb[0][2]), xyz),
        dot(float3(p.xyzD50ToSrgb[1][0], p.xyzD50ToSrgb[1][1], p.xyzD50ToSrgb[1][2]), xyz),
        dot(float3(p.xyzD50ToSrgb[2][0], p.xyzD50ToSrgb[2][1], p.xyzD50ToSrgb[2][2]), xyz));
}

float3 printPath(constant Params& p, float3 cmyFilm) {
    float3 printRaw = float3(0.0f);
    for (int l = 0; l < 81; ++l) {
        float density = p.filmBaseDensity[l][0] +
                        p.filmDensity[l][0] * cmyFilm.x + p.filmDensity[l][1] * cmyFilm.y +
                        p.filmDensity[l][2] * cmyFilm.z;
        float light = (density != density)
                          ? 0.0f
                          : exp2(-density * kLog2Of10) * p.printIlluminant[l][0];
        printRaw += light * float3(p.paperSensitivity[l][0], p.paperSensitivity[l][1],
                                   p.paperSensitivity[l][2]);
    }

    float3 cmyPaper;
    float logX = log10(max(printRaw.x * p.printExposureFactor, 0.0f) + 1e-10f);
    float logY = log10(max(printRaw.y * p.printExposureFactor, 0.0f) + 1e-10f);
    float logZ = log10(max(printRaw.z * p.printExposureFactor, 0.0f) + 1e-10f);
    cmyPaper.x = samplePaper(p, logX, 0);
    cmyPaper.y = samplePaper(p, logY, 1);
    cmyPaper.z = samplePaper(p, logZ, 2);

    float3 xyz = float3(0.0f);
    for (int l = 0; l < 81; ++l) {
        float density = p.paperBaseDensity[l][0] +
                        p.paperDensity[l][0] * cmyPaper.x +
                        p.paperDensity[l][1] * cmyPaper.y +
                        p.paperDensity[l][2] * cmyPaper.z;
        float light = (density != density)
                          ? 0.0f
                          : exp2(-density * kLog2Of10) * p.viewingIlluminant[l][0];
        xyz += light * float3(p.cmfs[l][0], p.cmfs[l][1], p.cmfs[l][2]);
    }
    xyz /= p.scanNormalization;
    return float3(
        dot(float3(p.xyzD50ToSrgb[0][0], p.xyzD50ToSrgb[0][1], p.xyzD50ToSrgb[0][2]), xyz),
        dot(float3(p.xyzD50ToSrgb[1][0], p.xyzD50ToSrgb[1][1], p.xyzD50ToSrgb[1][2]), xyz),
        dot(float3(p.xyzD50ToSrgb[2][0], p.xyzD50ToSrgb[2][1], p.xyzD50ToSrgb[2][2]), xyz));
}

bool inGamut(float3 rgb) {
    const float e = 1e-7f;
    return !(rgb.x < -e || rgb.x > 1.0f + e || rgb.y < -e || rgb.y > 1.0f + e ||
             rgb.z < -e || rgb.z > 1.0f + e);
}

float3 linearToOkLab(float3 rgb) {
    float3 lms = float3(
        dot(float3(0.4122214708f, 0.5363325363f, 0.0514459929f), rgb),
        dot(float3(0.2119034982f, 0.6806995451f, 0.1073969566f), rgb),
        dot(float3(0.0883024619f, 0.2817188376f, 0.6299787005f), rgb));
    lms = float3(sign(lms.x) * pow(abs(lms.x), 1.0f / 3.0f),
                 sign(lms.y) * pow(abs(lms.y), 1.0f / 3.0f),
                 sign(lms.z) * pow(abs(lms.z), 1.0f / 3.0f));
    return float3(dot(float3(0.2104542553f, 0.7936177850f, -0.0040720468f), lms),
                  dot(float3(1.9779984951f, -2.4285922050f, 0.4505937099f), lms),
                  dot(float3(0.0259040371f, 0.7827717662f, -0.8086757660f), lms));
}

float3 okLabToLinear(float3 lab) {
    float3 lms = float3(
        dot(float3(1.0f, 0.3963377774f, 0.2158037573f), lab),
        dot(float3(1.0f, -0.1055613458f, -0.0638541728f), lab),
        dot(float3(1.0f, -0.0894841775f, -1.2914855480f), lab));
    lms = lms * lms * lms;
    return float3(dot(float3(4.0767416621f, -3.3077115913f, 0.2309699292f), lms),
                  dot(float3(-1.2684380046f, 2.6097574011f, -0.3413193965f), lms),
                  dot(float3(-0.0041960863f, -0.7034186147f, 1.7076147010f), lms));
}

float maxChroma(float lightness, float hue) {
    if (lightness <= 0.0f || lightness >= 1.0f) return 0.0f;
    float3 rgb = okLabToLinear(float3(lightness, 0.0f, 0.0f));
    if (!inGamut(rgb)) return 0.0f;
    float low = 0.0f;
    float high = 0.5f;
    while (high < 4.0f) {
        float3 probe = okLabToLinear(float3(lightness, high * cos(hue), high * sin(hue)));
        if (!inGamut(probe)) break;
        low = high;
        high *= 2.0f;
    }
    for (int i = 0; i < 40; ++i) {
        float mid = 0.5f * (low + high);
        float3 probe = okLabToLinear(float3(lightness, mid * cos(hue), mid * sin(hue)));
        if (inGamut(probe)) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return low;
}

float3 compressGamut(float3 rgb) {
    if (inGamut(rgb)) return rgb;
    float3 lab = linearToOkLab(rgb);
    float lightness = lab.x;
    float a = lab.y;
    float b = lab.z;
    float chroma = sqrt(a * a + b * b);
    if (!(chroma > 1e-9f) || !(lightness > 0.0f) || !(lightness < 1.0f)) return rgb;
    float hue = atan2(b, a);
    float cMax = maxChroma(lightness, hue);
    if (!(cMax > 1e-9f)) return rgb;
    float d = chroma / cMax;
    const float threshold = 0.8f;
    const float limit = 1.0f;
    float compressed = d;
    if (d > threshold) {
        float x = (d - threshold) / (limit - threshold);
        compressed = threshold + (limit - threshold) * (x / sqrt(1.0f + x * x));
    }
    float3 outColor = okLabToLinear(
        float3(lightness, compressed * cMax * cos(hue), compressed * cMax * sin(hue)));
    return clamp(outColor, float3(0.0f), float3(1.0f));
}

float3 encodeOutput(constant Params& p, float3 linRgb) {
    if (p.hdrMode == 0) {
        return float3(srgbEncode(linRgb.x), srgbEncode(linRgb.y), srgbEncode(linRgb.z));
    }
    float3 clamped = clamp(linRgb, float3(0.0f), float3(1.0f));
    if (p.hdrMode == 1) {
        return float3(pqEncode(clamped.x * p.peakNits), pqEncode(clamped.y * p.peakNits),
                      pqEncode(clamped.z * p.peakNits));
    }
    return float3(hlgEncode(clamped.x), hlgEncode(clamped.y), hlgEncode(clamped.z));
}

kernel void filmMain(uint gid [[thread_position_in_grid]],
                     constant Params& p [[buffer(0)]],
                     device const float4* inputPixels [[buffer(1)]],
                     device float4* outputPixels [[buffer(2)]]) {
    if (gid >= p.count) return;
    float3 encoded = inputPixels[gid].xyz;
    float3 linRgb;
    if (p.appleLog != 0) {
        float3 lin2020 = float3(appleLogDecode(encoded.x), appleLogDecode(encoded.y),
                                appleLogDecode(encoded.z));
        linRgb = max(bt2020ToSrgb(lin2020), float3(0.0f));
    } else {
        linRgb = float3(srgbDecode(encoded.x), srgbDecode(encoded.y), srgbDecode(encoded.z));
    }
    linRgb *= p.exposureGain;

    float3 cmyFilm = filmCmy(p, linRgb);
    float3 rgb;
    if (p.scanNegative != 0) {
        float3 positive = float3(p.filmMaxCmy[0], p.filmMaxCmy[1], p.filmMaxCmy[2]) - cmyFilm;
        rgb = scanPositive(p, positive);
        float3 span = float3(p.negativeWhite[0] - p.negativeBlack[0],
                             p.negativeWhite[1] - p.negativeBlack[1],
                             p.negativeWhite[2] - p.negativeBlack[2]);
        rgb = float3(span.x > 1e-9f ? (rgb.x - p.negativeBlack[0]) / span.x : 0.0f,
                     span.y > 1e-9f ? (rgb.y - p.negativeBlack[1]) / span.y : 0.0f,
                     span.z > 1e-9f ? (rgb.z - p.negativeBlack[2]) / span.z : 0.0f);
        if (p.gamutCompress != 0) rgb = compressGamut(rgb);
        float3 clamped = clamp(rgb, float3(0.0f), float3(1.0f));
        rgb = float3(pow(clamped.x, p.negativeGamma[0]), pow(clamped.y, p.negativeGamma[1]),
                     pow(clamped.z, p.negativeGamma[2]));
        if (p.contrast != 1.0f) {
            float luma = dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
            if (luma > 1e-9f) {
                float scale = pow(luma / p.negativeGreyLuma, p.contrast - 1.0f);
                rgb = min(rgb * scale, float3(1.0f));
            }
        }
    } else {
        rgb = printPath(p, cmyFilm);
        if (p.gamutCompress != 0) rgb = compressGamut(rgb);
    }

    float blend = clamp(p.strength, 0.0f, 1.0f);
    outputPixels[gid] = float4(mix(encoded, encodeOutput(p, rgb), blend), 1.0f);
}
)MSL";

}  // namespace

struct GpuFilm::Impl {
    // Same layout as the MSL Params struct; see the note above the shader.
    struct Params {
        float filmCurves[256][4];
        float filmCurvesCoupler[256][4];
        float paperCurves[256][4];
        float filmDensity[81][4];
        float paperDensity[81][4];
        float paperSensitivity[81][4];
        float cmfs[81][4];
        float printIlluminant[81][4];
        float viewingIlluminant[81][4];
        float filmBaseDensity[81][4];
        float paperBaseDensity[81][4];
        float filmMatrix[3][4];
        float couplersMatrix[3][4];
        float xyzD50ToSrgb[3][4];
        float filmMaxCmy[4];
        float negativeWhite[4];
        float negativeBlack[4];
        float negativeGamma[4];
        float filmAxisStart;
        float filmAxisStep;
        float paperAxisStart;
        float paperAxisStep;
        float filmMidgrayGreen;
        float printExposureFactor;
        float scanNormalization;
        float negativeGreyLuma;
        unsigned int count;
        int couplersActive;
        int filmIsPositive;
        int scanNegative;
        int gamutCompress;
        int hdrMode;
        int appleLog;
        float exposureGain;
        float contrast;
        float peakNits;
        float strength;
        float pad1;
    };

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLComputePipelineState> pipeline = nil;
    id<MTLBuffer> paramsBuffer = nil;
    id<MTLBuffer> inputBuffer = nil;
    id<MTLBuffer> outputBuffer = nil;
    // False when the device came from attach(); the owner releases it.
    bool ownsDevice = true;
    std::size_t capacity = 0;
    // Reused between frames so a video render does not allocate 42 MB per frame.
    std::vector<float> packed;
    Params params{};

    double lastMs = 0.0;
    bool initialize(std::string& error);
    bool ensureCapacity(std::size_t pixels, std::string& error);
    void releaseSizeDependent();
};

GpuFilm::GpuFilm() : impl_(std::make_unique<Impl>()) {}

double GpuFilm::lastMilliseconds() const { return impl_->lastMs; }

bool GpuFilm::Impl::initialize(std::string& error) {
    if (device == nil) {
        device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            error = "no Metal device is available";
            return false;
        }
        queue = [device newCommandQueue];
        if (queue == nil) {
            error = "could not create a Metal command queue";
            return false;
        }
    }
    if (pipeline != nil) return true;

    NSError* compileError = nil;
    id<MTLLibrary> library =
        [device newLibraryWithSource:[NSString stringWithUTF8String:kShaderSource]
                             options:nil
                               error:&compileError];
    if (library == nil) {
        error = "could not compile the Metal film shader";
        if (compileError != nil) {
            error += ": ";
            error += [compileError.localizedDescription UTF8String];
        }
        return false;
    }
    id<MTLFunction> function = [library newFunctionWithName:@"filmMain"];
    if (function == nil) {
        error = "the Metal film shader has no filmMain entry point";
        return false;
    }
    NSError* pipelineError = nil;
    pipeline = [device newComputePipelineStateWithFunction:function error:&pipelineError];
    if (pipeline == nil) {
        error = "could not create the Metal film pipeline";
        if (pipelineError != nil) {
            error += ": ";
            error += [pipelineError.localizedDescription UTF8String];
        }
        return false;
    }

    paramsBuffer = [device newBufferWithLength:sizeof(Params)
                                       options:MTLResourceStorageModeShared];
    if (paramsBuffer == nil) {
        error = "could not allocate the Metal parameter buffer";
        return false;
    }
    return true;
}

void GpuFilm::Impl::releaseSizeDependent() {
    inputBuffer = nil;
    outputBuffer = nil;
    capacity = 0;
}

bool GpuFilm::Impl::ensureCapacity(std::size_t pixels, std::string& error) {
    if (pixels <= capacity) return true;
    releaseSizeDependent();

    const NSUInteger bytes = static_cast<NSUInteger>(pixels) * 16u;
    inputBuffer = [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    outputBuffer = [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    if (inputBuffer == nil || outputBuffer == nil) {
        error = "could not allocate the Metal frame buffers";
        releaseSizeDependent();
        return false;
    }
    capacity = pixels;
    return true;
}

bool GpuFilm::available(std::string& error) {
    Impl probe;
    return probe.initialize(error);
}

bool GpuFilm::configure(const FilmModel& model, const BakeOptions& options, std::string& error) {
    if (!impl_->initialize(error)) return false;

    GpuFilmData data;
    if (!model.gpuData(data)) {
        error = "this profile uses a non-uniform curve axis; use --engine spectral";
        return false;
    }
    Impl::Params& p = impl_->params;
    std::memset(&p, 0, sizeof(p));
    for (int i = 0; i < GpuFilmData::kCurveSamples; ++i) {
        for (int c = 0; c < 3; ++c) {
            p.filmCurves[i][c] = data.filmCurves[i][c];
            p.filmCurvesCoupler[i][c] = data.filmCurvesCoupler[i][c];
            p.paperCurves[i][c] = data.paperCurves[i][c];
        }
    }
    for (int l = 0; l < GpuFilmData::kWavelengths; ++l) {
        for (int c = 0; c < 3; ++c) {
            p.filmDensity[l][c] = data.filmDensity[l][c];
            p.paperDensity[l][c] = data.paperDensity[l][c];
            p.paperSensitivity[l][c] = data.paperSensitivity[l][c];
            p.cmfs[l][c] = data.cmfs[l][c];
        }
        p.printIlluminant[l][0] = data.printIlluminant[l];
        p.viewingIlluminant[l][0] = data.viewingIlluminant[l];
        p.filmBaseDensity[l][0] = data.filmBaseDensity[l];
        p.paperBaseDensity[l][0] = data.paperBaseDensity[l];
    }
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            p.filmMatrix[r][c] = data.filmMatrix[r][c];
            p.couplersMatrix[r][c] = data.couplersMatrix[r][c];
            p.xyzD50ToSrgb[r][c] = data.xyzD50ToSrgb[r][c];
        }
        p.filmMaxCmy[r] = data.filmMaxCmy[r];
        p.negativeWhite[r] = data.negativeWhite[r];
        p.negativeBlack[r] = data.negativeBlack[r];
        p.negativeGamma[r] = data.negativeGamma[r];
    }
    p.filmAxisStart = data.filmAxisStart;
    p.filmAxisStep = data.filmAxisStep;
    p.paperAxisStart = data.paperAxisStart;
    p.paperAxisStep = data.paperAxisStep;
    p.filmMidgrayGreen = data.filmMidgrayGreen;
    p.printExposureFactor = data.printExposureFactor;
    p.scanNormalization = data.scanNormalization;
    p.negativeGreyLuma = data.negativeGreyLuma;
    p.couplersActive = data.couplersActive ? 1 : 0;
    p.filmIsPositive = data.filmIsPositive ? 1 : 0;
    p.scanNegative = options.scan == ScanMode::Negative ? 1 : 0;
    p.gamutCompress = options.gamutCompress ? 1 : 0;
    p.hdrMode = options.hdr == HdrMode::Pq ? 1 : (options.hdr == HdrMode::Hlg ? 2 : 0);
    p.appleLog = options.input == InputTransfer::AppleLog ? 1 : 0;
    p.exposureGain = static_cast<float>(std::exp2(options.exposureEv));
    p.contrast = static_cast<float>(options.contrast);
    p.peakNits = static_cast<float>(options.peakNits);
    p.count = 0;
    return true;
}

bool GpuFilm::process(const float* rgb, std::size_t pixels, float* out, float strength,
                      std::string& error) {
    if (!processToDevice(rgb, pixels, strength, error)) return false;
    return readback(out, pixels, error);
}

void* GpuFilm::deviceHandle() const {
    return impl_->device != nil ? (__bridge void*)impl_->device : nullptr;
}

bool GpuFilm::attach(void* device, std::string& error) {
    if (device == nullptr) {
        error = "cannot attach the GPU engine to a null device";
        return false;
    }
    id<MTLDevice> shared = (__bridge id<MTLDevice>)device;
    if (impl_->device != nil) return impl_->device == shared;
    impl_->device = shared;
    impl_->ownsDevice = false;
    impl_->queue = [shared newCommandQueue];
    if (impl_->queue == nil) {
        error = "could not create a Metal command queue";
        return false;
    }
    return true;
}

bool GpuFilm::processToDevice(const float* rgb, std::size_t pixels, float strength,
                              std::string& error) {
    if (pixels == 0) return true;
    if (!impl_->initialize(error)) return false;
    if (!impl_->ensureCapacity(pixels, error)) return false;

    std::vector<float>& packed = impl_->packed;
    packed.resize(pixels * 4);
    for (std::size_t i = 0; i < pixels; ++i) {
        packed[i * 4 + 0] = rgb[i * 3 + 0];
        packed[i * 4 + 1] = rgb[i * 3 + 1];
        packed[i * 4 + 2] = rgb[i * 3 + 2];
        packed[i * 4 + 3] = 1.0f;
    }
    std::memcpy(impl_->inputBuffer.contents, packed.data(), pixels * 16);

    impl_->params.count = static_cast<unsigned int>(pixels);
    impl_->params.strength = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
    std::memcpy(impl_->paramsBuffer.contents, &impl_->params, sizeof(impl_->params));

    const auto started = std::chrono::steady_clock::now();
    @autoreleasepool {
        id<MTLCommandBuffer> command = [impl_->queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:impl_->pipeline];
        [encoder setBuffer:impl_->paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:impl_->inputBuffer offset:0 atIndex:1];
        [encoder setBuffer:impl_->outputBuffer offset:0 atIndex:2];
        [encoder dispatchThreads:MTLSizeMake(pixels, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        if (command.status == MTLCommandBufferStatusError) {
            error = "the Metal film dispatch failed";
            return false;
        }
    }
    impl_->lastMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return true;
}

void* GpuFilm::resultBuffer() const {
    return impl_->outputBuffer != nil ? (__bridge void*)impl_->outputBuffer : nullptr;
}

bool GpuFilm::readback(float* out, std::size_t pixels, std::string& error) {
    if (pixels == 0) return true;
    if (impl_->outputBuffer == nil) {
        error = "the GPU spectral engine has no result to read";
        return false;
    }
    const auto started = std::chrono::steady_clock::now();
    // The command buffer completed inside processToDevice(), so the shared
    // buffer already holds the result.
    const float* gpu = static_cast<const float*>(impl_->outputBuffer.contents);
    for (std::size_t i = 0; i < pixels; ++i) {
        out[i * 3 + 0] = gpu[i * 4 + 0];
        out[i * 3 + 1] = gpu[i * 4 + 1];
        out[i * 3 + 2] = gpu[i * 4 + 2];
    }
    impl_->lastMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return true;
}

GpuFilm::~GpuFilm() {
    if (!impl_) return;
    impl_->releaseSizeDependent();
    impl_->paramsBuffer = nil;
    impl_->pipeline = nil;
    impl_->queue = nil;
    impl_->device = nil;
}

}  // namespace sr

#else  // !__APPLE__

// This file is only added to Apple targets; keep a translation unit for tools
// that compile every source file unconditionally.

namespace sr {
struct GpuFilmMetalPlaceholder {};
}  // namespace sr

#endif
