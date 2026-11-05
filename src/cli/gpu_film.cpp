// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// D3D11 compute implementation of the per-pixel physical chain (D046). The
// shader is a direct translation of FilmModel::transformFrom; the CPU spectral
// path remains the reference and the fallback.

#include "gpu_film.hpp"

#include "sr/ocio.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// D3D11 is only compiled on Windows. Defining SR_GPU_DISABLED builds the CPU
// stub instead, which keeps the fallback path compilable (and testable) on any
// platform.
#if defined(_WIN32) && !defined(SR_GPU_DISABLED)
#define SR_HAVE_D3D11 1
#else
#define SR_HAVE_D3D11 0
#endif

#if SR_HAVE_D3D11
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#endif

namespace sr {
namespace {

#if SR_HAVE_D3D11

const char* kShader = R"HLSL(
cbuffer Params : register(b0) {
    float4 filmCurves[256];
    float4 filmCurvesCoupler[256];
    float4 paperCurves[256];
    float4 filmDensity[81];
    float4 paperDensity[81];
    float4 paperSensitivity[81];
    float4 cmfs[81];
    float4 printIlluminant[81];
    float4 viewingIlluminant[81];
    float4 printViewingIlluminant[81];
    float4 filmBaseDensity[81];
    float4 paperBaseDensity[81];
    float4 filmMatrix[3];
    float4 couplersMatrix[3];
    float4 xyzD50ToSrgb[3];
    float4 filmScanMatrix[3];
    float4 printScanMatrix[3];
    float4 filmMaxCmy;
    float4 negativeWhite;
    float4 negativeBlack;
    float4 negativeGamma;
    // D067: reversal (slide) direct scan.
    float4 positiveWhite;
    float4 positiveBlack;
    float4 positiveGamma;
    float filmAxisStart;
    float filmAxisStep;
    float paperAxisStart;
    float paperAxisStep;
    float filmMidgrayGreen;
    float printExposureFactor;
    float scanNormalization;
    float printScanNormalization;
    float negativeGreyLuma;
    float positiveGreyLuma;
    uint count;
    int couplersActive;
    int filmIsPositive;
    int scanMode;
    int gamutCompress;
    int hdrMode;
    int inputMode;
    float exposureGain;
    float contrast;
    float peakNits;
    float strength;
    // Keep the constant buffer a multiple of 16 bytes (D065/D067).
    float pad1;
    float pad2;
    float pad3;
};

StructuredBuffer<float4> inputPixels : register(t0);
// D081: when OCIO decodes the input, inputPixels already holds linear Rec.709
// and this buffer keeps the original encoded values for the strength blend.
StructuredBuffer<float4> encodedPixels : register(t1);
RWStructuredBuffer<float4> outputPixels : register(u0);

static const float kLog2Of10 = 3.321928094887362;

float srgbDecode(float v) {
    float x = clamp(v, 0.0, 1.0);
    return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

float srgbEncode(float v) {
    return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

float appleLogDecode(float encoded) {
    const float r0 = -0.05641088;
    const float rt = 0.01;
    const float sigma = 47.28711236;
    const float beta = 0.00964052;
    const float gamma = 0.08550479;
    const float delta = 0.69336945;
    const float pT = sigma * (rt - r0) * (rt - r0);
    if (encoded < 0.0) return r0;
    if (encoded < pT) return sqrt(encoded / sigma) + r0;
    return exp2((encoded - delta) / gamma) - beta;
}

float3 bt2020ToSrgb(float3 v) {
    return float3(
        dot(float3(1.660303485421445, -0.587570142531617, -0.072890060215056), v),
        dot(float3(-0.124375595308386, 1.132834481431958, -0.008359737187401), v),
        dot(float3(-0.018112279959916, -0.100583608507212, 1.118770326157098), v));
}

// Camera log decodes and gamut matrices (D060), same constants as transfer.cpp.
float slog3Decode(float y) {
    if (y >= 171.2102946929 / 1023.0) {
        return pow(10.0, (y * 1023.0 - 420.0) / 261.5) * 0.19 - 0.01;
    }
    return (y * 1023.0 - 95.0) * 0.01125 / (171.2102946929 - 95.0);
}

float cLog3Decode(float y) {
    float x;
    if (y < 0.097465473) {
        x = -(pow(10.0, (0.12783901 - y) / 0.36726845) - 1.0) / 14.98325;
    } else if (y <= 0.15277891) {
        x = (y - 0.12512219) / 1.9754798;
    } else {
        x = (pow(10.0, (y - 0.12240537) / 0.36726845) - 1.0) / 14.98325;
    }
    return x * 0.9;
}

float vLogDecode(float y) {
    if (y < 0.181) return (y - 0.125) / 5.6;
    return pow(10.0, (y - 0.598206) / 0.241514) - 0.00873;
}

// D061 camera logs. Same constants as transfer.cpp; the piecewise thresholds
// are written as the same expressions so both sides branch on the same point.
float arriLogC3Decode(float t) {
    if (t > 5.367655 * 0.010591 + 0.092809) {
        return (pow(10.0, (t - 0.385537) / 0.247190) - 0.052272) / 5.555556;
    }
    return (t - 0.092809) / 5.367655;
}

float arriLogC4Decode(float ep) {
    if (ep >= 0.0) {
        return (exp2(14.0 * ((ep - 0.09286412512218964) / 0.9071358748778103) + 6.0) - 64.0) /
               2231.8263090676883;
    }
    return ep * 0.1135972086105891 - 0.01805699611991131;
}

float bmdFilmGen5Decode(float y) {
    if (y < 8.283605932402494 * 0.005 + 0.09246575342465753) {
        return (y - 0.09246575342465753) / 8.283605932402494;
    }
    return exp((y - 0.5300133392291939) / 0.08692876065491224) - 0.005494072432257808;
}

float daVinciIntermediateDecode(float v) {
    if (v <= 0.02740668) return v / 10.44426855;
    return exp2(v / 0.07329248 - 7.0) - 0.0075;
}

float3 camMatrix(float3 v, float3 r, float3 g, float3 b) {
    return float3(dot(r, v), dot(g, v), dot(b, v));
}

// inputMode: 0 sRGB, 1 Apple Log, 2 S-Log3, 3 C-Log3, 4 V-Log,
//            5 S-Log3 + S-Gamut3, 6 LogC3, 7 LogC4, 8 Blackmagic Gen 5,
//            9 DaVinci Intermediate, 10 linear Rec.709 (OCIO already decoded).
float3 decodeInput(float3 encoded) {
    if (inputMode == 10) {
        // D081: OpenColorIO ran on the CPU before upload and already returned
        // linear Rec.709; clamp like the CPU reference does.
        return max(encoded, 0.0);
    }
    if (inputMode == 0) {
        return float3(srgbDecode(encoded.x), srgbDecode(encoded.y), srgbDecode(encoded.z));
    }
    float3 camera;
    if (inputMode == 1) {
        camera = float3(appleLogDecode(encoded.x), appleLogDecode(encoded.y),
                        appleLogDecode(encoded.z));
        return max(bt2020ToSrgb(camera), 0.0);
    }
    if (inputMode == 2) {
        camera = float3(slog3Decode(encoded.x), slog3Decode(encoded.y), slog3Decode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.626947409865913, -0.540138538842170, -0.086808870733380),
                             float3(-0.178515527182660, 1.417940927396144, -0.239425400446255),
                             float3(-0.044436114956758, -0.195919966185631, 1.240356081207883)),
                   0.0);
    }
    if (inputMode == 3) {
        camera = float3(cLog3Decode(encoded.x), cLog3Decode(encoded.y), cLog3Decode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.923861295898551, -0.798760663248431, -0.125100632650120),
                             float3(-0.204310848200921, 1.495898509750939, -0.291587661550018),
                             float3(-0.023685020970341, -0.420127011030270, 1.443812032000612)),
                   0.0);
    }
    if (inputMode == 4) {
        camera = float3(vLogDecode(encoded.x), vLogDecode(encoded.y), vLogDecode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.806573570346048, -0.695698131346300, -0.110878567820157),
                             float3(-0.170089035422901, 1.305954766066613, -0.135864821747590),
                             float3(-0.025205692416949, -0.154467764144244, 1.179673668428944)),
                   0.0);
    }
    if (inputMode == 5) {
        camera = float3(slog3Decode(encoded.x), slog3Decode(encoded.y), slog3Decode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.877703304158600, -0.794072272012260, -0.083787749334960),
                             float3(-0.176609064916940, 1.350911870533990, -0.174203656726040),
                             float3(-0.026158248305760, -0.148431249773440, 1.174663935814270)),
                   0.0);
    }
    if (inputMode == 6) {
        camera = float3(arriLogC3Decode(encoded.x), arriLogC3Decode(encoded.y),
                        arriLogC3Decode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.617341953200000, -0.537215450400000, -0.080283108000000),
                             float3(-0.070402521000000, 1.334552331200000, -0.264050721600000),
                             float3(-0.021064084400000, -0.226959489200000, 1.248098278800000)),
                   0.0);
    }
    if (inputMode == 7) {
        camera = float3(arriLogC4Decode(encoded.x), arriLogC4Decode(encoded.y),
                        arriLogC4Decode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.892909309143406, -0.780786358195487, -0.112279668273147),
                             float3(-0.205500776543893, 1.340171181030512, -0.134571255550448),
                             float3(-0.012662323539739, -0.152193809032254, 1.164930570261962)),
                   0.0);
    }
    if (inputMode == 8) {
        camera = float3(bmdFilmGen5Decode(encoded.x), bmdFilmGen5Decode(encoded.y),
                        bmdFilmGen5Decode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.568229090933787, -0.522604320339746, -0.045781487919271),
                             float3(-0.086193852150434, 1.344854361752704, -0.258561360666100),
                             float3(-0.052011751203802, -0.249161501129390, 1.301247690023161)),
                   0.0);
    }
    if (inputMode == 9) {
        camera = float3(daVinciIntermediateDecode(encoded.x), daVinciIntermediateDecode(encoded.y),
                        daVinciIntermediateDecode(encoded.z));
        return max(camMatrix(camera,
                             float3(1.898404857216630, -0.792072675468742, -0.106488899073117),
                             float3(-0.168748493825520, 1.488888136041571, -0.320040493279880),
                             float3(-0.121499307902914, -0.315689508174336, 1.437263253767220)),
                   0.0);
    }
    // Unknown mode: fall back to the sRGB curve instead of guessing a camera log.
    return float3(srgbDecode(encoded.x), srgbDecode(encoded.y), srgbDecode(encoded.z));
}

float pqEncode(float nits) {
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    float y = max(0.0, nits) / 10000.0;
    float p = pow(y, m1);
    return pow((c1 + c2 * p) / (1.0 + c3 * p), m2);
}

float hlgEncode(float linRgb) {
    const float a = 0.17883277;
    const float b = 0.28466892;
    const float c = 0.55991073;
    float e = max(0.0, linRgb);
    if (e <= 1.0 / 12.0) return sqrt(3.0 * e);
    return a * log(12.0 * e - b) + c;
}

float sampleFilm(float x, int c) {
    if (!isfinite(x)) return 0.0;
    float end = filmAxisStart + 255.0 * filmAxisStep;
    if (x <= filmAxisStart) return filmCurves[0][c];
    if (x >= end) return filmCurves[255][c];
    float position = (x - filmAxisStart) / filmAxisStep;
    int low = min((int)position, 254);
    float t = position - low;
    return lerp(filmCurves[low][c], filmCurves[low + 1][c], t);
}

float sampleFilmCoupler(float x, int c) {
    if (!isfinite(x)) return 0.0;
    float end = filmAxisStart + 255.0 * filmAxisStep;
    if (x <= filmAxisStart) return filmCurvesCoupler[0][c];
    if (x >= end) return filmCurvesCoupler[255][c];
    float position = (x - filmAxisStart) / filmAxisStep;
    int low = min((int)position, 254);
    float t = position - low;
    return lerp(filmCurvesCoupler[low][c], filmCurvesCoupler[low + 1][c], t);
}

float samplePaper(float x, int c) {
    if (!isfinite(x)) return 0.0;
    float end = paperAxisStart + 255.0 * paperAxisStep;
    if (x <= paperAxisStart) return paperCurves[0][c];
    if (x >= end) return paperCurves[255][c];
    float position = (x - paperAxisStart) / paperAxisStep;
    int low = min((int)position, 254);
    float t = position - low;
    return lerp(paperCurves[low][c], paperCurves[low + 1][c], t);
}

float3 filmCmy(float3 linRgb) {
    float3 raw;
    for (int m = 0; m < 3; ++m) {
        float v = dot(float3(filmMatrix[0][m], filmMatrix[1][m], filmMatrix[2][m]), linRgb);
        raw[m] = (isnan(v) ? 0.0 : v) / filmMidgrayGreen;
    }
    float3 logRaw = log10(max(raw, 0.0) + 1e-10);
    float3 cmy = float3(sampleFilm(logRaw.x, 0), sampleFilm(logRaw.y, 1),
                        sampleFilm(logRaw.z, 2));
    if (couplersActive == 0) return cmy;

    float3 silver = filmIsPositive != 0 ? filmMaxCmy.xyz - cmy : cmy;
    float3 corrected;
    for (int m = 0; m < 3; ++m) {
        float3 column = float3(couplersMatrix[0][m], couplersMatrix[1][m], couplersMatrix[2][m]);
        corrected[m] = logRaw[m] - dot(silver, column);
    }
    return float3(sampleFilmCoupler(corrected.x, 0), sampleFilmCoupler(corrected.y, 1),
                  sampleFilmCoupler(corrected.z, 2));
}

float3 scanDye(float3 cmy, int includeBase) {
    float3 xyz = float3(0, 0, 0);
    [loop] for (int l = 0; l < 81; ++l) {
        float density = (includeBase != 0 ? filmBaseDensity[l].x : 0.0) +
                        dot(filmDensity[l].xyz, cmy);
        // Same guard as the CPU reference: undefined density contributes no light.
        float light = (density != density) ? 0.0
                                           : exp2(-density * kLog2Of10) * viewingIlluminant[l].x;
        xyz += light * cmfs[l].xyz;
    }
    xyz /= scanNormalization;
    xyz = float3(dot(filmScanMatrix[0].xyz, xyz), dot(filmScanMatrix[1].xyz, xyz),
                 dot(filmScanMatrix[2].xyz, xyz));
    return float3(dot(xyzD50ToSrgb[0].xyz, xyz), dot(xyzD50ToSrgb[1].xyz, xyz),
                  dot(xyzD50ToSrgb[2].xyz, xyz));
}

float3 printPath(float3 cmyFilm) {
    float3 printRaw = float3(0, 0, 0);
    [loop] for (int l = 0; l < 81; ++l) {
        float density = filmBaseDensity[l].x + dot(filmDensity[l].xyz, cmyFilm);
        float light = (density != density) ? 0.0
                                           : exp2(-density * kLog2Of10) * printIlluminant[l].x;
        printRaw += light * paperSensitivity[l].xyz;
    }
    float3 cmyPaper;
    for (int c = 0; c < 3; ++c) {
        float value = printRaw[c] * printExposureFactor;
        float logRaw = log10(max(value, 0.0) + 1e-10);
        cmyPaper[c] = samplePaper(logRaw, c);
    }
    float3 xyz = float3(0, 0, 0);
    [loop] for (int l = 0; l < 81; ++l) {
        float density = paperBaseDensity[l].x + dot(paperDensity[l].xyz, cmyPaper);
        float light = (density != density) ? 0.0
                                           : exp2(-density * kLog2Of10) * printViewingIlluminant[l].x;
        xyz += light * cmfs[l].xyz;
    }
    xyz /= printScanNormalization;
    xyz = float3(dot(printScanMatrix[0].xyz, xyz), dot(printScanMatrix[1].xyz, xyz),
                 dot(printScanMatrix[2].xyz, xyz));
    return float3(dot(xyzD50ToSrgb[0].xyz, xyz), dot(xyzD50ToSrgb[1].xyz, xyz),
                  dot(xyzD50ToSrgb[2].xyz, xyz));
}

bool inGamut(float3 rgb) {
    const float e = 1e-7;
    return !(rgb.x < -e || rgb.x > 1.0 + e || rgb.y < -e || rgb.y > 1.0 + e ||
             rgb.z < -e || rgb.z > 1.0 + e);
}

float3 linearToOkLab(float3 rgb) {
    float3 lms = float3(
        dot(float3(0.4122214708, 0.5363325363, 0.0514459929), rgb),
        dot(float3(0.2119034982, 0.6806995451, 0.1073969566), rgb),
        dot(float3(0.0883024619, 0.2817188376, 0.6299787005), rgb));
    lms = float3(sign(lms.x) * pow(abs(lms.x), 1.0 / 3.0),
                 sign(lms.y) * pow(abs(lms.y), 1.0 / 3.0),
                 sign(lms.z) * pow(abs(lms.z), 1.0 / 3.0));
    return float3(dot(float3(0.2104542553, 0.7936177850, -0.0040720468), lms),
                  dot(float3(1.9779984951, -2.4285922050, 0.4505937099), lms),
                  dot(float3(0.0259040371, 0.7827717662, -0.8086757660), lms));
}

float3 okLabToLinear(float3 lab) {
    float3 lms = float3(
        dot(float3(1.0, 0.3963377774, 0.2158037573), lab),
        dot(float3(1.0, -0.1055613458, -0.0638541728), lab),
        dot(float3(1.0, -0.0894841775, -1.2914855480), lab));
    lms = lms * lms * lms;
    return float3(dot(float3(4.0767416621, -3.3077115913, 0.2309699292), lms),
                  dot(float3(-1.2684380046, 2.6097574011, -0.3413193965), lms),
                  dot(float3(-0.0041960863, -0.7034186147, 1.7076147010), lms));
}

float maxChroma(float lightness, float hue) {
    if (lightness <= 0.0 || lightness >= 1.0) return 0.0;
    float3 rgb = okLabToLinear(float3(lightness, 0.0, 0.0));
    if (!inGamut(rgb)) return 0.0;
    float low = 0.0;
    float high = 0.5;
    while (high < 4.0) {
        float3 probe = okLabToLinear(float3(lightness, high * cos(hue), high * sin(hue)));
        if (!inGamut(probe)) break;
        low = high;
        high *= 2.0;
    }
    for (int i = 0; i < 40; ++i) {
        float mid = 0.5 * (low + high);
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
    if (!(chroma > 1e-9) || !(lightness > 0.0) || !(lightness < 1.0)) return rgb;
    float hue = atan2(b, a);
    float cMax = maxChroma(lightness, hue);
    if (!(cMax > 1e-9)) return rgb;
    float d = chroma / cMax;
    const float threshold = 0.8;
    const float limit = 1.0;
    float compressed = d;
    if (d > threshold) {
        float x = (d - threshold) / (limit - threshold);
        compressed = threshold + (limit - threshold) * (x / sqrt(1.0 + x * x));
    }
    float3 outColor = okLabToLinear(
        float3(lightness, compressed * cMax * cos(hue), compressed * cMax * sin(hue)));
    return clamp(outColor, 0.0, 1.0);
}

float3 encodeOutput(float3 linRgb) {
    if (hdrMode == 0) {
        return float3(srgbEncode(linRgb.x), srgbEncode(linRgb.y), srgbEncode(linRgb.z));
    }
    float3 clamped = clamp(linRgb, 0.0, 1.0);
    if (hdrMode == 1) {
        return float3(pqEncode(clamped.x * peakNits), pqEncode(clamped.y * peakNits),
                      pqEncode(clamped.z * peakNits));
    }
    return float3(hlgEncode(clamped.x), hlgEncode(clamped.y), hlgEncode(clamped.z));
}

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= count) return;
    const float3 encoded = inputPixels[id.x].xyz;
    const float3 linRgb = decodeInput(encoded) * exposureGain;

    float3 cmyFilm = filmCmy(linRgb);
    float3 rgb;
    if (scanMode == 2) {
        // D067: reversal direct scan — keep the base, then white/black balance.
        rgb = scanDye(cmyFilm, 1);
        float3 span = float3(positiveWhite.x - positiveBlack.x,
                             positiveWhite.y - positiveBlack.y,
                             positiveWhite.z - positiveBlack.z);
        rgb = float3(span.x > 1e-9 ? (rgb.x - positiveBlack.x) / span.x : 0.0,
                     span.y > 1e-9 ? (rgb.y - positiveBlack.y) / span.y : 0.0,
                     span.z > 1e-9 ? (rgb.z - positiveBlack.z) / span.z : 0.0);
        if (gamutCompress != 0) rgb = compressGamut(rgb);
        for (int c = 0; c < 3; ++c) {
            rgb[c] = pow(clamp(rgb[c], 0.0, 1.0), positiveGamma[c]);
        }
        if (contrast != 1.0) {
            float luma = dot(rgb, float3(0.2126, 0.7152, 0.0722));
            if (luma > 1e-9) {
                float scale = pow(luma / positiveGreyLuma, contrast - 1.0);
                rgb = min(rgb * scale, 1.0);
            }
        }
    } else if (scanMode == 1) {
        float3 positive = filmMaxCmy.xyz - cmyFilm;
        rgb = scanDye(positive, 0);
        for (int c = 0; c < 3; ++c) {
            float span = negativeWhite[c] - negativeBlack[c];
            rgb[c] = span > 1e-9 ? (rgb[c] - negativeBlack[c]) / span : 0.0;
        }
        if (gamutCompress != 0) rgb = compressGamut(rgb);
        for (int c = 0; c < 3; ++c) {
            rgb[c] = pow(clamp(rgb[c], 0.0, 1.0), negativeGamma[c]);
        }
        if (contrast != 1.0) {
            float luma = dot(rgb, float3(0.2126, 0.7152, 0.0722));
            if (luma > 1e-9) {
                float scale = pow(luma / negativeGreyLuma, contrast - 1.0);
                rgb = min(rgb * scale, 1.0);
            }
        }
    } else {
        rgb = printPath(cmyFilm);
        if (gamutCompress != 0) rgb = compressGamut(rgb);
    }

    // D081: with OCIO the shader receives decoded linear values, so blend the
    // film result with the original encoded sample instead.
    float3 source = (inputMode == 10) ? encodedPixels[id.x].xyz : encoded;
    outputPixels[id.x] = float4(lerp(source, encodeOutput(rgb), saturate(strength)), 1.0);
}
)HLSL";

#endif  // SR_HAVE_D3D11

}  // namespace

struct GpuFilm::Impl {
#if SR_HAVE_D3D11
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    // False when the device came from attach(); the owner releases it.
    bool ownsDevice = true;
    ID3D11ComputeShader* shader = nullptr;
    ID3D11Buffer* constantBuffer = nullptr;
    ID3D11Buffer* inputBuffer = nullptr;
    ID3D11Buffer* outputBuffer = nullptr;
    // D081: only allocated when the OCIO input transform is active.
    ID3D11Buffer* encodedBuffer = nullptr;
    ID3D11ShaderResourceView* inputView = nullptr;
    ID3D11ShaderResourceView* encodedView = nullptr;
    ID3D11UnorderedAccessView* outputView = nullptr;
    ID3D11Buffer* staging = nullptr;
    ID3D11Query* stopQuery = nullptr;
    std::size_t capacity = 0;
    // Reused between frames so a video render does not allocate 42 MB per frame.
    std::vector<float> packed;
    // Scratch RGB for the bulk OCIO decode (D081).
    std::vector<float> linearized;
    // The OCIO processor is copied (cheap shared handle) so the caller's recipe
    // may go away while this backend is still in use.
    OcioProcessor ocio;

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
        float printViewingIlluminant[81][4];
        float filmBaseDensity[81][4];
        float paperBaseDensity[81][4];
        float filmMatrix[3][4];
        float couplersMatrix[3][4];
        float xyzD50ToSrgb[3][4];
        float filmScanMatrix[3][4];
        float printScanMatrix[3][4];
        float filmMaxCmy[4];
        float negativeWhite[4];
        float negativeBlack[4];
        float negativeGamma[4];
        float positiveWhite[4];
        float positiveBlack[4];
        float positiveGamma[4];
        float filmAxisStart;
        float filmAxisStep;
        float paperAxisStart;
        float paperAxisStep;
        float filmMidgrayGreen;
        float printExposureFactor;
        float scanNormalization;
        float printScanNormalization;
        float negativeGreyLuma;
        float positiveGreyLuma;
        unsigned int count;
        int couplersActive;
        int filmIsPositive;
        int scanMode;
        int gamutCompress;
        int hdrMode;
        int inputMode;
        float exposureGain;
        float contrast;
        float peakNits;
        float strength;
        float pad1;
        float pad2;
        float pad3;
    } params;
    static_assert(sizeof(Params) % 16 == 0,
                  "D3D11 constant buffers must be a multiple of 16 bytes");

#endif

    // Declared on every platform so the non-Windows stub definitions match.
    double lastMs = 0.0;
    bool initialize(std::string& error);
    bool ensureCapacity(std::size_t pixels, std::string& error);
    void releaseSizeDependent();
};

GpuFilm::GpuFilm() : impl_(std::make_unique<Impl>()) {}

double GpuFilm::lastMilliseconds() const { return impl_->lastMs; }

bool GpuFilm::available(std::string& error) {
#if !SR_HAVE_D3D11
    error = "the GPU spectral engine is only available on Windows (D3D11) and macOS (Metal)";
    return false;
#else
    Impl probe;
    return probe.initialize(error);
#endif
}

std::string GpuFilm::deviceInfo() {
#if !SR_HAVE_D3D11
    return "no GPU backend on this platform\n";
#else
    ID3D11Device* device = nullptr;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                 D3D11_SDK_VERSION, &device, &level, nullptr))) {
        return "D3D11: could not create a device\n";
    }
    char header[80] = {};
    std::snprintf(header, sizeof(header), "D3D11 feature level 0x%X\n",
                  static_cast<unsigned>(level));
    std::string info = header;

    IDXGIDevice* dxgi = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice),
                                         reinterpret_cast<void**>(&dxgi)))) {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(dxgi->GetAdapter(&adapter))) {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(adapter->GetDesc(&desc))) {
                char name[256] = {};
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name,
                                    static_cast<int>(sizeof(name)) - 1, nullptr, nullptr);
                info += "adapter: ";
                info += name;
                info += "\n";
            }
            adapter->Release();
        }
        dxgi->Release();
    }
    device->Release();
    return info;
#endif
}

bool GpuFilm::configure(const FilmModel& model, const BakeOptions& options, std::string& error) {
#if !SR_HAVE_D3D11
    (void)model;
    (void)options;
    error = "the GPU spectral engine is only available on Windows (D3D11)";
    return false;
#else
    if (!impl_->initialize(error)) return false;

    // D081: keep the compiled OCIO input transform alive. configure() may run
    // again on the same backend, so an old transform must not linger.
    impl_->ocio = OcioProcessor();
    if (options.ocio != nullptr) impl_->ocio = *options.ocio;
    // An existing capacity may predate this transform; force a resize so the
    // encoded-pixel buffer is allocated (or released) as needed.
    if (impl_->capacity != 0 && impl_->ocio.valid() != (impl_->encodedBuffer != nullptr)) {
        impl_->releaseSizeDependent();
    }

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
        p.printViewingIlluminant[l][0] = data.printViewingIlluminant[l];
        p.filmBaseDensity[l][0] = data.filmBaseDensity[l];
        p.paperBaseDensity[l][0] = data.paperBaseDensity[l];
    }
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            p.filmMatrix[r][c] = data.filmMatrix[r][c];
            p.couplersMatrix[r][c] = data.couplersMatrix[r][c];
            p.xyzD50ToSrgb[r][c] = data.xyzD50ToSrgb[r][c];
            p.filmScanMatrix[r][c] = data.filmScanMatrix[r][c];
            p.printScanMatrix[r][c] = data.printScanMatrix[r][c];
        }
        p.filmMaxCmy[r] = data.filmMaxCmy[r];
        p.negativeWhite[r] = data.negativeWhite[r];
        p.negativeBlack[r] = data.negativeBlack[r];
        p.negativeGamma[r] = data.negativeGamma[r];
        p.positiveWhite[r] = data.positiveWhite[r];
        p.positiveBlack[r] = data.positiveBlack[r];
        p.positiveGamma[r] = data.positiveGamma[r];
    }
    p.filmAxisStart = data.filmAxisStart;
    p.filmAxisStep = data.filmAxisStep;
    p.paperAxisStart = data.paperAxisStart;
    p.paperAxisStep = data.paperAxisStep;
    p.filmMidgrayGreen = data.filmMidgrayGreen;
    p.printExposureFactor = data.printExposureFactor;
    p.scanNormalization = data.scanNormalization;
    p.printScanNormalization = data.printScanNormalization;
    p.negativeGreyLuma = data.negativeGreyLuma;
    p.positiveGreyLuma = data.positiveGreyLuma;
    p.couplersActive = data.couplersActive ? 1 : 0;
    p.filmIsPositive = data.filmIsPositive ? 1 : 0;
    p.scanMode = static_cast<int>(options.scan);
    p.gamutCompress = options.gamutCompress ? 1 : 0;
    p.hdrMode = options.hdr == HdrMode::Pq ? 1 : (options.hdr == HdrMode::Hlg ? 2 : 0);
    // 10 = linear Rec.709 already decoded by OpenColorIO (D081).
    p.inputMode = impl_->ocio.valid() ? 10 : static_cast<int>(options.input);
    p.exposureGain = static_cast<float>(std::exp2(options.exposureEv));
    p.contrast = static_cast<float>(options.contrast);
    p.peakNits = static_cast<float>(options.peakNits);
    p.count = 0;

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(Impl::Params);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (impl_->constantBuffer == nullptr) {
        if (FAILED(impl_->device->CreateBuffer(&cbDesc, nullptr, &impl_->constantBuffer))) {
            error = "could not create the GPU constant buffer";
            return false;
        }
    }
    return true;
#endif
}

bool GpuFilm::process(const float* rgb, std::size_t pixels, float* out, float strength,
                      std::string& error) {
    if (!processToDevice(rgb, pixels, strength, error)) return false;
    return readback(out, pixels, error);
}

void* GpuFilm::deviceHandle() const {
#if !SR_HAVE_D3D11
    return nullptr;
#else
    return impl_->device;
#endif
}

bool GpuFilm::attach(void* device, std::string& error) {
#if !SR_HAVE_D3D11
    (void)device;
    error = "the GPU spectral engine is only available on Windows (D3D11)";
    return false;
#else
    if (device == nullptr) {
        error = "cannot attach the GPU engine to a null device";
        return false;
    }
    if (impl_->device != nullptr) return impl_->device == device;
    impl_->device = static_cast<ID3D11Device*>(device);
    impl_->ownsDevice = false;
    impl_->device->GetImmediateContext(&impl_->context);
    return impl_->context != nullptr;
#endif
}

bool GpuFilm::processToDevice(const float* rgb, std::size_t pixels, float strength,
                              std::string& error) {
#if !SR_HAVE_D3D11
    (void)rgb;
    (void)pixels;
    (void)strength;
    error = "the GPU spectral engine is only available on Windows (D3D11)";
    return false;
#else
    if (pixels == 0) return true;
    if (!impl_->ensureCapacity(pixels, error)) return false;

    std::vector<float>& packed = impl_->packed;
    packed.resize(pixels * 4);
    for (std::size_t i = 0; i < pixels; ++i) {
        packed[i * 4 + 0] = rgb[i * 3 + 0];
        packed[i * 4 + 1] = rgb[i * 3 + 1];
        packed[i * 4 + 2] = rgb[i * 3 + 2];
        packed[i * 4 + 3] = 1.0f;
    }
    if (impl_->ocio.valid()) {
        // D081: OCIO's CPU processor is vectorised and far faster than the CPU
        // spectral engine; the shader then receives linear Rec.709. Keep the
        // encoded copy for the strength blend.
        impl_->context->UpdateSubresource(impl_->encodedBuffer, 0, nullptr, packed.data(), 0, 0);
        std::vector<float>& linear = impl_->linearized;
        linear.resize(pixels * 3);
        std::memcpy(linear.data(), rgb, pixels * 3 * sizeof(float));
        impl_->ocio.apply(linear.data(), static_cast<long>(pixels));
        for (std::size_t i = 0; i < pixels; ++i) {
            packed[i * 4 + 0] = linear[i * 3 + 0];
            packed[i * 4 + 1] = linear[i * 3 + 1];
            packed[i * 4 + 2] = linear[i * 3 + 2];
        }
    }
    impl_->context->UpdateSubresource(impl_->inputBuffer, 0, nullptr, packed.data(), 0, 0);

    impl_->params.count = static_cast<unsigned int>(pixels);
    impl_->params.strength = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(impl_->context->Map(impl_->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                                   &mapped))) {
        error = "could not map the GPU constant buffer";
        return false;
    }
    std::memcpy(mapped.pData, &impl_->params, sizeof(impl_->params));
    impl_->context->Unmap(impl_->constantBuffer, 0);

    ID3D11Buffer* cbs[] = {impl_->constantBuffer};
    // t1 is a dummy binding unless OCIO supplied the original encoded frame.
    ID3D11ShaderResourceView* srvs[] = {
        impl_->inputView,
        impl_->encodedView != nullptr ? impl_->encodedView : impl_->inputView,
    };
    ID3D11UnorderedAccessView* uavs[] = {impl_->outputView};
    impl_->context->CSSetShader(impl_->shader, nullptr, 0);
    impl_->context->CSSetConstantBuffers(0, 1, cbs);
    impl_->context->CSSetShaderResources(0, 2, srvs);
    impl_->context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    const UINT groups = static_cast<UINT>((pixels + 63) / 64);
    const auto started = std::chrono::steady_clock::now();
    impl_->context->Dispatch(groups, 1, 1);
    // Unbind before the texture backend reads the same buffer as an SRV (D049).
    ID3D11ShaderResourceView* nullSrv[2] = {nullptr, nullptr};
    ID3D11UnorderedAccessView* nullUav[1] = {nullptr};
    impl_->context->CSSetShaderResources(0, 2, nullSrv);
    impl_->context->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
    impl_->context->End(impl_->stopQuery);
    impl_->context->Flush();
    BOOL done = FALSE;
    while (impl_->context->GetData(impl_->stopQuery, &done, sizeof(done), 0) != S_OK ||
           done == FALSE) {
    }
    impl_->lastMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return true;
#endif
}

void* GpuFilm::resultBuffer() const {
#if !SR_HAVE_D3D11
    return nullptr;
#else
    return impl_->outputBuffer;
#endif
}

bool GpuFilm::readback(float* out, std::size_t pixels, std::string& error) {
#if !SR_HAVE_D3D11
    (void)out;
    (void)pixels;
    error = "the GPU spectral engine is only available on Windows (D3D11)";
    return false;
#else
    if (pixels == 0) return true;
    const auto started = std::chrono::steady_clock::now();
    impl_->context->CopyResource(impl_->staging, impl_->outputBuffer);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(impl_->context->Map(impl_->staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        error = "could not read back the GPU result";
        return false;
    }
    const float* gpu = static_cast<const float*>(mapped.pData);
    for (std::size_t i = 0; i < pixels; ++i) {
        out[i * 3 + 0] = gpu[i * 4 + 0];
        out[i * 3 + 1] = gpu[i * 4 + 1];
        out[i * 3 + 2] = gpu[i * 4 + 2];
    }
    impl_->context->Unmap(impl_->staging, 0);
    impl_->lastMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return true;
#endif
}

#if SR_HAVE_D3D11
bool GpuFilm::Impl::initialize(std::string& error) {
    if (device == nullptr) {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                     D3D11_SDK_VERSION, &device, &level, &context))) {
            error = "could not create a D3D11 device";
            return false;
        }
    }
    if (shader != nullptr) return true;
    ID3DBlob* code = nullptr;
    ID3DBlob* messages = nullptr;
    const HRESULT compiled =
        D3DCompile(kShader, std::strlen(kShader), "gpu_film.hlsl", nullptr, nullptr, "CSMain",
                   "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &messages);
    if (FAILED(compiled)) {
        if (messages != nullptr) {
            error.assign(static_cast<const char*>(messages->GetBufferPointer()),
                         messages->GetBufferSize());
            messages->Release();
        } else {
            error = "could not compile the GPU shader";
        }
        return false;
    }
    if (FAILED(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(),
                                           nullptr, &shader))) {
        code->Release();
        error = "could not create the GPU compute shader";
        return false;
    }
    code->Release();

    D3D11_QUERY_DESC queryDesc{};
    queryDesc.Query = D3D11_QUERY_EVENT;
    device->CreateQuery(&queryDesc, &stopQuery);
    return true;
}

void GpuFilm::Impl::releaseSizeDependent() {
    if (context != nullptr) {
        ID3D11ShaderResourceView* nullSrv[] = {nullptr, nullptr};
        ID3D11UnorderedAccessView* nullUav[] = {nullptr};
        context->CSSetShaderResources(0, 2, nullSrv);
        context->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
    }
    if (inputView != nullptr) inputView->Release();
    if (encodedView != nullptr) encodedView->Release();
    if (outputView != nullptr) outputView->Release();
    if (staging != nullptr) staging->Release();
    if (inputBuffer != nullptr) inputBuffer->Release();
    if (encodedBuffer != nullptr) encodedBuffer->Release();
    if (outputBuffer != nullptr) outputBuffer->Release();
    inputView = nullptr;
    encodedView = nullptr;
    outputView = nullptr;
    staging = nullptr;
    inputBuffer = nullptr;
    encodedBuffer = nullptr;
    outputBuffer = nullptr;
    capacity = 0;
}

bool GpuFilm::Impl::ensureCapacity(std::size_t pixels, std::string& error) {
    // The OCIO path needs the extra encoded-pixel buffer; a plain run does not.
    if (pixels <= capacity && (encodedBuffer != nullptr) == ocio.valid()) return true;
    releaseSizeDependent();

    D3D11_BUFFER_DESC inputDesc{};
    inputDesc.ByteWidth = static_cast<UINT>(pixels * 16);
    inputDesc.Usage = D3D11_USAGE_DEFAULT;
    inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    inputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    inputDesc.StructureByteStride = 16;
    if (FAILED(device->CreateBuffer(&inputDesc, nullptr, &inputBuffer))) {
        error = "could not allocate the GPU input buffer";
        return false;
    }

    D3D11_BUFFER_DESC outputDesc = inputDesc;
    // SHADER_RESOURCE lets the texture backend read this buffer directly (D049).
    outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateBuffer(&outputDesc, nullptr, &outputBuffer))) {
        error = "could not allocate the GPU output buffer";
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = static_cast<UINT>(pixels);
    if (FAILED(device->CreateShaderResourceView(inputBuffer, &srvDesc, &inputView))) {
        error = "could not create the GPU input view";
        return false;
    }

    if (ocio.valid()) {
        // The encoded original is only uploaded on the OCIO path (D081).
        if (FAILED(device->CreateBuffer(&inputDesc, nullptr, &encodedBuffer))) {
            error = "could not allocate the GPU encoded-input buffer";
            return false;
        }
        if (FAILED(device->CreateShaderResourceView(encodedBuffer, &srvDesc, &encodedView))) {
            error = "could not create the GPU encoded-input view";
            return false;
        }
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = static_cast<UINT>(pixels);
    if (FAILED(device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputView))) {
        error = "could not create the GPU output view";
        return false;
    }

    D3D11_BUFFER_DESC stagingDesc = outputDesc;
    stagingDesc.BindFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(device->CreateBuffer(&stagingDesc, nullptr, &staging))) {
        error = "could not allocate the GPU readback buffer";
        return false;
    }
    capacity = pixels;
    return true;
}

#else
bool GpuFilm::Impl::initialize(std::string&) { return false; }
bool GpuFilm::Impl::ensureCapacity(std::size_t, std::string&) { return false; }
void GpuFilm::Impl::releaseSizeDependent() {}
#endif

GpuFilm::~GpuFilm() {
    if (!impl_) return;
#if SR_HAVE_D3D11
    impl_->releaseSizeDependent();
    if (impl_->constantBuffer != nullptr) impl_->constantBuffer->Release();
    if (impl_->stopQuery != nullptr) impl_->stopQuery->Release();
    if (impl_->shader != nullptr) impl_->shader->Release();
    if (impl_->context != nullptr) impl_->context->Release();
    if (impl_->device != nullptr && impl_->ownsDevice) impl_->device->Release();
#endif
}

}  // namespace sr
