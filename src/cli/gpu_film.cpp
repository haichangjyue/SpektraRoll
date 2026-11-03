// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// D3D11 compute implementation of the per-pixel physical chain (D046). The
// shader is a direct translation of FilmModel::transformFrom; the CPU spectral
// path remains the reference and the fallback.

#include "gpu_film.hpp"

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
    float4 filmBaseDensity[81];
    float4 paperBaseDensity[81];
    float4 filmMatrix[3];
    float4 couplersMatrix[3];
    float4 xyzD50ToSrgb[3];
    float4 filmMaxCmy;
    float4 negativeWhite;
    float4 negativeBlack;
    float4 negativeGamma;
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

StructuredBuffer<float4> inputPixels : register(t0);
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

float3 scanPositive(float3 positive) {
    float3 xyz = float3(0, 0, 0);
    [loop] for (int l = 0; l < 81; ++l) {
        float density = dot(filmDensity[l].xyz, positive);
        // Same guard as the CPU reference: undefined density contributes no light.
        float light = (density != density) ? 0.0
                                           : exp2(-density * kLog2Of10) * viewingIlluminant[l].x;
        xyz += light * cmfs[l].xyz;
    }
    xyz /= scanNormalization;
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
                                           : exp2(-density * kLog2Of10) * viewingIlluminant[l].x;
        xyz += light * cmfs[l].xyz;
    }
    xyz /= scanNormalization;
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
    float3 encoded = inputPixels[id.x].xyz;
    float3 linRgb;
    if (appleLog != 0) {
        float3 lin2020 = float3(appleLogDecode(encoded.x), appleLogDecode(encoded.y),
                                appleLogDecode(encoded.z));
        linRgb = max(bt2020ToSrgb(lin2020), 0.0);
    } else {
        linRgb = float3(srgbDecode(encoded.x), srgbDecode(encoded.y), srgbDecode(encoded.z));
    }
    linRgb *= exposureGain;

    float3 cmyFilm = filmCmy(linRgb);
    float3 rgb;
    if (scanNegative != 0) {
        float3 positive = filmMaxCmy.xyz - cmyFilm;
        rgb = scanPositive(positive);
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

    outputPixels[id.x] = float4(lerp(encoded, encodeOutput(rgb), saturate(strength)), 1.0);
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
    ID3D11ShaderResourceView* inputView = nullptr;
    ID3D11UnorderedAccessView* outputView = nullptr;
    ID3D11Buffer* staging = nullptr;
    ID3D11Query* stopQuery = nullptr;
    std::size_t capacity = 0;
    // Reused between frames so a video render does not allocate 42 MB per frame.
    std::vector<float> packed;

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
    } params;

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
    ID3D11ShaderResourceView* srvs[] = {impl_->inputView};
    ID3D11UnorderedAccessView* uavs[] = {impl_->outputView};
    impl_->context->CSSetShader(impl_->shader, nullptr, 0);
    impl_->context->CSSetConstantBuffers(0, 1, cbs);
    impl_->context->CSSetShaderResources(0, 1, srvs);
    impl_->context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    const UINT groups = static_cast<UINT>((pixels + 63) / 64);
    const auto started = std::chrono::steady_clock::now();
    impl_->context->Dispatch(groups, 1, 1);
    // Unbind before the texture backend reads the same buffer as an SRV (D049).
    ID3D11ShaderResourceView* nullSrv[1] = {nullptr};
    ID3D11UnorderedAccessView* nullUav[1] = {nullptr};
    impl_->context->CSSetShaderResources(0, 1, nullSrv);
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
        ID3D11ShaderResourceView* nullSrv[] = {nullptr};
        ID3D11UnorderedAccessView* nullUav[] = {nullptr};
        context->CSSetShaderResources(0, 1, nullSrv);
        context->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
    }
    if (inputView != nullptr) inputView->Release();
    if (outputView != nullptr) outputView->Release();
    if (staging != nullptr) staging->Release();
    if (inputBuffer != nullptr) inputBuffer->Release();
    if (outputBuffer != nullptr) outputBuffer->Release();
    inputView = nullptr;
    outputView = nullptr;
    staging = nullptr;
    inputBuffer = nullptr;
    outputBuffer = nullptr;
    capacity = 0;
}

bool GpuFilm::Impl::ensureCapacity(std::size_t pixels, std::string& error) {
    if (pixels <= capacity) return true;
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
