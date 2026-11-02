// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// D3D11 compute implementation of the spatial texture chain (D047). Each shader
// entry is a direct translation of the matching helper in src/core/src/texture.cpp:
// the separable box blur keeps one thread per row/column so the floating point
// summation order matches the CPU reference, and the wide-blur shortcut
// (downsample, blur, bilinear upsample) uses the same factors and rounding.

#include "gpu_texture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

#if defined(_WIN32) && !defined(SR_GPU_DISABLED)
#define SR_HAVE_D3D11 1
#else
#define SR_HAVE_D3D11 0
#endif

#if SR_HAVE_D3D11
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

namespace sr {
namespace {

#if SR_HAVE_D3D11

const char* kShader = R"HLSL(
cbuffer Params : register(b0) {
    float4 tint;
    int width;
    int height;
    uint count;
    int radius;
    int factor;
    int lowWidth;
    int lowHeight;
    int blurWidth;
    int blurHeight;
    uint seed;
    int fieldIndex;
    float amount;
    float threshold;
    float phase;
    float norm;
    float scale;
    float pad0;
    float pad1;
    float pad2;
    float pad3;
    // Radius of the separabale PSF in tapsIn; the taps themselves live in a
    // structured buffer because HLSL pads constant-buffer arrays to 16 bytes
    // per element, which would not match the CPU kernel (D054).
    int kernelRadius;
    float pad4;
    float pad5;
    float pad6;
};

StructuredBuffer<float4> rgbIn : register(t0);
StructuredBuffer<float> scalarIn : register(t1);
StructuredBuffer<float> scalarIn2 : register(t2);
StructuredBuffer<float> tapsIn : register(t3);
RWStructuredBuffer<float4> rgbOut : register(u0);
RWStructuredBuffer<float> scalarOut : register(u1);

float lumaOf(float3 c) {
    return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z;
}

// --- scalar sources -------------------------------------------------------

[numthreads(64, 1, 1)] void CSHalationSource(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    float luma = lumaOf(rgbIn[i].rgb);
    float span = max(1e-3, 1.0 - threshold);
    float excess = clamp((luma - threshold) / span, 0.0, 1.0);
    scalarOut[i] = excess * excess;
}

[numthreads(64, 1, 1)] void CSLuma(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    scalarOut[i] = lumaOf(rgbIn[i].rgb);
}

// --- separable box blur ---------------------------------------------------

[numthreads(1, 64, 1)] void CSBlurH(uint3 id : SV_DispatchThreadID) {
    int y = (int)id.y;
    if (y >= blurHeight) return;
    int w = blurWidth;
    int r = radius;
    int window = 2 * r + 1;
    uint row = (uint)y * (uint)w;
    float sum = 0.0;
    for (int x = -r; x <= r; ++x) {
        sum += scalarIn[row + (uint)clamp(x, 0, w - 1)];
    }
    for (int x = 0; x < w; ++x) {
        scalarOut[row + (uint)x] = sum / float(window);
        sum -= scalarIn[row + (uint)clamp(x - r, 0, w - 1)];
        sum += scalarIn[row + (uint)clamp(x + r + 1, 0, w - 1)];
    }
}

[numthreads(64, 1, 1)] void CSBlurV(uint3 id : SV_DispatchThreadID) {
    int x = (int)id.x;
    if (x >= blurWidth) return;
    int w = blurWidth;
    int h = blurHeight;
    int r = radius;
    int window = 2 * r + 1;
    float sum = 0.0;
    for (int y = -r; y <= r; ++y) {
        sum += scalarIn[(uint)clamp(y, 0, h - 1) * (uint)w + (uint)x];
    }
    for (int y = 0; y < h; ++y) {
        scalarOut[(uint)y * (uint)w + (uint)x] = sum / float(window);
        sum -= scalarIn[(uint)clamp(y - r, 0, h - 1) * (uint)w + (uint)x];
        sum += scalarIn[(uint)clamp(y + r + 1, 0, h - 1) * (uint)w + (uint)x];
    }
}

// --- separable convolution (D054 physical spread) -------------------------

[numthreads(1, 64, 1)] void CSConvolveH(uint3 id : SV_DispatchThreadID) {
    int y = (int)id.y;
    if (y >= blurHeight) return;
    int w = blurWidth;
    int r = kernelRadius;
    uint row = (uint)y * (uint)w;
    for (int x = 0; x < w; ++x) {
        float sum = 0.0;
        for (int k = -r; k <= r; ++k) {
            sum += tapsIn[k + r] * scalarIn[row + (uint)clamp(x + k, 0, w - 1)];
        }
        scalarOut[row + (uint)x] = sum;
    }
}

[numthreads(64, 1, 1)] void CSConvolveV(uint3 id : SV_DispatchThreadID) {
    int x = (int)id.x;
    if (x >= blurWidth) return;
    int w = blurWidth;
    int h = blurHeight;
    int r = kernelRadius;
    for (int y = 0; y < h; ++y) {
        float sum = 0.0;
        for (int k = -r; k <= r; ++k) {
            sum += tapsIn[k + r] * scalarIn[(uint)clamp(y + k, 0, h - 1) * (uint)w + (uint)x];
        }
        scalarOut[(uint)y * (uint)w + (uint)x] = sum;
    }
}

// --- wide blur shortcut ---------------------------------------------------

[numthreads(64, 1, 1)] void CSDownsample(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    uint lowCount = (uint)(lowWidth * lowHeight);
    if (i >= lowCount) return;
    int lx = (int)(i % (uint)lowWidth);
    int ly = (int)(i / (uint)lowWidth);
    float inv = 1.0 / float(factor * factor);
    int x0 = lx * factor;
    int y0 = ly * factor;
    float acc = 0.0;
    for (int y = y0; y < min(y0 + factor, height); ++y) {
        for (int x = x0; x < min(x0 + factor, width); ++x) {
            acc += scalarIn[(uint)(y * width + x)] * inv;
        }
    }
    scalarOut[i] = acc;
}

[numthreads(64, 1, 1)] void CSUpsample(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    int x = (int)(i % (uint)width);
    int y = (int)(i / (uint)width);
    float fy = (float(y) + 0.5) / float(factor) - 0.5;
    int y0 = clamp((int)floor(fy), 0, lowHeight - 1);
    int y1 = min(lowHeight - 1, y0 + 1);
    float ty = clamp(fy - float(y0), 0.0, 1.0);
    float fx = (float(x) + 0.5) / float(factor) - 0.5;
    int x0 = clamp((int)floor(fx), 0, lowWidth - 1);
    int x1 = min(lowWidth - 1, x0 + 1);
    float tx = clamp(fx - float(x0), 0.0, 1.0);
    float top = scalarIn[(uint)(y0 * lowWidth + x0)] * (1.0 - tx) +
                scalarIn[(uint)(y0 * lowWidth + x1)] * tx;
    float bottom = scalarIn[(uint)(y1 * lowWidth + x0)] * (1.0 - tx) +
                   scalarIn[(uint)(y1 * lowWidth + x1)] * tx;
    scalarOut[i] = top * (1.0 - ty) + bottom * ty;
}

// --- grain ----------------------------------------------------------------

uint hash32(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint mix32(uint a, uint b) {
    return hash32(a ^ (b + 0x9e3779b9U + (a << 6) + (a >> 2)));
}

[numthreads(64, 1, 1)] void CSGrainField(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    uint x = i % (uint)width;
    uint y = i / (uint)width;
    uint h = mix32(mix32(seed, (uint)fieldIndex), mix32(x, y));
    scalarOut[i] = float(h) / 2147483648.0 - 1.0;
}

// --- composites -----------------------------------------------------------

[numthreads(64, 1, 1)] void CSHalationApply(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    float4 c = rgbIn[i];
    float a = scalarIn[i] * amount;
    rgbOut[i] = float4(min(1.0, c.x + a * tint.x), min(1.0, c.y + a * tint.y),
                       min(1.0, c.z + a * tint.z), 1.0);
}

[numthreads(64, 1, 1)] void CSGlareApply(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    float4 c = rgbIn[i];
    float v = scalarIn[i];
    float3 veiling = (amount * v) * tint.rgb;
    rgbOut[i] = float4(min(1.0, 1.0 - (1.0 - c.x) * (1.0 - veiling.x)),
                       min(1.0, 1.0 - (1.0 - c.y) * (1.0 - veiling.y)),
                       min(1.0, 1.0 - (1.0 - c.z) * (1.0 - veiling.z)), 1.0);
}

[numthreads(64, 1, 1)] void CSDiffusionApply(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    float4 c = rgbIn[i];
    float luma = lumaOf(c.rgb);
    float offset = (scalarIn[i] - luma) * amount;
    rgbOut[i] = float4(clamp(c.x + offset, 0.0, 1.0), clamp(c.y + offset, 0.0, 1.0),
                       clamp(c.z + offset, 0.0, 1.0), 1.0);
}

[numthreads(64, 1, 1)] void CSGrainApply(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    float4 c = rgbIn[i];
    float luma = lumaOf(c.rgb);
    float weight = 1.0 - abs(2.0 * luma - 1.0);
    float noise = (scalarIn[i] * (1.0 - phase) + scalarIn2[i] * phase) * norm;
    float delta = noise * scale * weight;
    rgbOut[i] = float4(clamp(c.x + delta, 0.0, 1.0), clamp(c.y + delta, 0.0, 1.0),
                       clamp(c.z + delta, 0.0, 1.0), 1.0);
}

[numthreads(64, 1, 1)] void CSCopyRgb(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    rgbOut[i] = rgbIn[i];
}
)HLSL";

#endif  // SR_HAVE_D3D11

}  // namespace

struct GpuTexture::Impl {
#if SR_HAVE_D3D11
    struct Res {
        ID3D11Buffer* buffer = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11UnorderedAccessView* uav = nullptr;
        UINT elements = 0;
    };

    enum Shader {
        kHalationSource,
        kLuma,
        kBlurH,
        kBlurV,
        kConvolveH,
        kConvolveV,
        kDownsample,
        kUpsample,
        kHalationApply,
        kGlareApply,
        kDiffusionApply,
        kGrainField,
        kGrainApply,
        kCopy,
        kShaderCount,
    };

    struct Params {
        float tint[4];
        int width;
        int height;
        unsigned int count;
        int radius;
        int factor;
        int lowWidth;
        int lowHeight;
        int blurWidth;
        int blurHeight;
        unsigned int seed;
        int fieldIndex;
        float amount;
        float threshold;
        float phase;
        float norm;
        float scale;
        float pad0;
        float pad1;
        float pad2;
        float pad3;
        int kernelRadius;
        float pad4;
        float pad5;
        float pad6;
    };

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11ComputeShader* shaders[kShaderCount] = {};
    ID3D11Buffer* constantBuffer = nullptr;
    ID3D11Query* stopQuery = nullptr;

    Res rgbA;
    Res rgbB;
    // Scalar maps and their full-resolution scratch (glow / veil / blurred).
    Res full0;
    Res full1;
    // Wide-blur pyramid.
    Res low;
    Res lowScratch;
    // PSF taps, uploaded per effect (D054); 132 floats is the largest kernel
    // the builder can produce after the downsample factor is chosen.
    Res kernelTaps;
    // Grain fields are produced last, so they get their own pair.
    Res grain0;
    Res grain1;
    Res staging;
    // An input buffer owned by another backend (the fused film pass, D049).
    Res external;
    void* externalBuffer = nullptr;
    // False when the device came from attach(); the owner releases it.
    bool ownsDevice = true;

    int width = 0;
    int height = 0;
    int lowWidth = 0;
    int lowHeight = 0;
    std::vector<float> packed;
    Params params{};
#endif

    double lastMs = 0.0;
    bool initialize(std::string& error);
    bool ensureCapacity(int w, int h, std::string& error);
    void releaseResources();
#if SR_HAVE_D3D11
    bool createRes(UINT elements, UINT stride, Res& res, std::string& error);
    // An SRV over a buffer this object does not own (fused film output, D049).
    bool createResView(ID3D11Buffer* buffer, UINT elements, Res& res, std::string& error);
    void releaseRes(Res& res);
    void releaseExternal();
    // The effect chain itself: rgbA is the input, rgbB the scratch, and the
    // result is read back from whichever buffer holds it at the end.
    bool runChain(int width, int height, const TextureParams& params, int frame, float* out,
                  std::string& error);
    bool dispatch(Shader shader, UINT groupsX, UINT groupsY, Res* srv0, Res* srv1, Res* srv2,
                  Res* srv3, int uavSlot, Res* uav, std::string& error);
    // Scalar passes read t1 (scalarIn) and write u1 (scalarOut).
    bool runScalar(Shader shader, UINT groupsX, UINT groupsY, Res* input, Res* output,
                   std::string& error);
    // RGB-to-scalar passes read t0 (rgbIn) and write u1 (scalarOut).
    bool runSource(Shader shader, UINT groupsX, UINT groupsY, Res* rgb, Res* output,
                    std::string& error);
    // Colour passes read t0 (rgbIn) plus an optional t1 scalar map and write u0.
    bool runRgb(Shader shader, UINT groupsX, UINT groupsY, Res* rgb, Res* scalar, Res* output,
                std::string& error);
    bool runGrain(Shader shader, UINT groupsX, UINT groupsY, Res* rgb, Res* fieldA, Res* fieldB,
                  Res* output, std::string& error);
    bool runConvolve(Shader shader, UINT groupsX, UINT groupsY, Res* input, Res* output,
                     std::string& error);
    bool blurWide(Res& buffer, int radius, int factor, std::string& error);
    bool spreadWide(Res& buffer, SpreadKind kind, float sigmaPx, std::string& error);
    bool setKernel(const SpreadKernel& kernel, std::string& error);
    bool buildGrainField(unsigned seed, int index, int radius, Res& target, std::string& error);
    void resetParams();
#endif
};

GpuTexture::GpuTexture() : impl_(std::make_unique<Impl>()) {}

double GpuTexture::lastMilliseconds() const { return impl_->lastMs; }

bool GpuTexture::available(std::string& error) {
#if !SR_HAVE_D3D11
    error = "the GPU texture backend is only available on Windows (D3D11)";
    return false;
#else
    Impl probe;
    return probe.initialize(error);
#endif
}

bool GpuTexture::prepare(std::string& error) {
#if !SR_HAVE_D3D11
    error = "the GPU texture backend is only available on Windows (D3D11)";
    return false;
#else
    return impl_->initialize(error);
#endif
}

bool GpuTexture::attach(void* device, std::string& error) {
#if !SR_HAVE_D3D11
    (void)device;
    error = "the GPU texture backend is only available on Windows (D3D11)";
    return false;
#else
    if (device == nullptr) {
        error = "cannot attach the texture backend to a null device";
        return false;
    }
    if (impl_->device != nullptr) return impl_->device == device;
    impl_->device = static_cast<ID3D11Device*>(device);
    impl_->ownsDevice = false;
    impl_->device->GetImmediateContext(&impl_->context);
    return impl_->context != nullptr;
#endif
}

bool GpuTexture::process(const float* rgb, int width, int height, const TextureParams& params,
                         int frame, float* out, std::string& error) {
#if !SR_HAVE_D3D11
    (void)rgb;
    (void)width;
    (void)height;
    (void)params;
    (void)frame;
    (void)out;
    error = "the GPU texture backend is only available on Windows (D3D11)";
    return false;
#else
    if (width <= 0 || height <= 0) {
        error = "bad frame size for the GPU texture backend";
        return false;
    }
    const std::size_t count = static_cast<std::size_t>(width) * height;
    const bool active = params.halation.amount > 0.0f || params.glare.amount > 0.0f ||
                        params.diffusion.amount > 0.0f || params.grain.amount > 0.0f;
    if (!active) {
        if (out != rgb) std::memcpy(out, rgb, count * 3 * sizeof(float));
        return true;
    }

    if (!impl_->initialize(error)) return false;
    if (!impl_->ensureCapacity(width, height, error)) return false;

    Impl& g = *impl_;
    g.packed.resize(count * 4);
    for (std::size_t i = 0; i < count; ++i) {
        g.packed[i * 4 + 0] = rgb[i * 3 + 0];
        g.packed[i * 4 + 1] = rgb[i * 3 + 1];
        g.packed[i * 4 + 2] = rgb[i * 3 + 2];
        g.packed[i * 4 + 3] = 1.0f;
    }
    g.context->UpdateSubresource(g.rgbA.buffer, 0, nullptr, g.packed.data(), 0, 0);
    return g.runChain(width, height, params, frame, out, error);
#endif
}

bool GpuTexture::processBuffer(void* input, int width, int height, const TextureParams& params,
                               int frame, float* out, std::string& error) {
#if !SR_HAVE_D3D11
    (void)input;
    (void)width;
    (void)height;
    (void)params;
    (void)frame;
    (void)out;
    error = "the GPU texture backend is only available on Windows (D3D11)";
    return false;
#else
    if (input == nullptr) {
        error = "fused texture input is null";
        return false;
    }
    if (width <= 0 || height <= 0) {
        error = "bad frame size for the GPU texture backend";
        return false;
    }
    const std::size_t count = static_cast<std::size_t>(width) * height;
    const bool active = params.halation.amount > 0.0f || params.glare.amount > 0.0f ||
                        params.diffusion.amount > 0.0f || params.grain.amount > 0.0f;
    if (!active) {
        error = "the fused path needs at least one texture effect";
        return false;
    }
    if (!impl_->initialize(error)) return false;
    if (!impl_->ensureCapacity(width, height, error)) return false;

    Impl& g = *impl_;
    if (g.externalBuffer != input || g.external.elements != count) {
        g.releaseExternal();
        if (!g.createResView(static_cast<ID3D11Buffer*>(input), static_cast<UINT>(count),
                             g.external, error)) {
            return false;
        }
        g.externalBuffer = input;
    }
    // Copy the film result into our own buffer so the effect chain can keep
    // ping-ponging between rgbA and rgbB exactly like the staged path.
    const UINT groups = static_cast<UINT>((count + 63) / 64);
    g.resetParams();
    if (!g.dispatch(Impl::kCopy, groups, 1, &g.external, nullptr, nullptr, nullptr, 0, &g.rgbA,
                    error)) {
        return false;
    }
    return g.runChain(width, height, params, frame, out, error);
#endif
}

#if SR_HAVE_D3D11

bool GpuTexture::Impl::runChain(int width, int height, const TextureParams& params, int frame,
                                float* out, std::string& error) {
    const std::size_t count = static_cast<std::size_t>(width) * height;
    const auto started = std::chrono::steady_clock::now();
    Impl& g = *this;
    const UINT fullGroups = static_cast<UINT>((count + 63) / 64);
    Impl::Res* current = &g.rgbA;
    Impl::Res* other = &g.rgbB;
    const auto swap = [&]() { std::swap(current, other); };

    if (params.halation.amount > 0.0f) {
        const float threshold =
            std::min(0.99f, std::max(0.0f, params.halation.threshold));
        g.resetParams();
        g.params.threshold = threshold;
        if (!g.runSource(Impl::kHalationSource, fullGroups, 1, current, &g.full0, error))
            return false;

        const int radius =
            std::max(1, static_cast<int>(std::lround(params.halation.radius * width)));
        if (params.spread.model == SpreadModel::Box) {
            if (!g.blurWide(g.full0, radius, 2, error)) return false;
        } else {
            const float sigma = spreadSigmaPixels(params.halation.spreadMicrons,
                                                  params.spread.frameWidthMm, width);
            if (!g.spreadWide(g.full0, SpreadKind::Halation, sigma, error)) return false;
        }

        g.resetParams();
        g.params.amount = params.halation.amount * 0.55f;
        g.params.tint[0] = 1.0f;
        g.params.tint[1] = 0.42f;
        g.params.tint[2] = 0.18f;
        if (!g.runRgb(Impl::kHalationApply, fullGroups, 1, current, &g.full0, other, error))
            return false;
        swap();
    }

    if (params.glare.amount > 0.0f) {
        g.resetParams();
        if (!g.runSource(Impl::kLuma, fullGroups, 1, current, &g.full0, error))
            return false;
        const int radius = std::max(1, static_cast<int>(std::lround(params.glare.radius * width)));
        if (params.spread.model == SpreadModel::Box) {
            if (!g.blurWide(g.full0, radius, 4, error)) return false;
        } else {
            const float sigma = spreadSigmaPixels(params.glare.spreadMicrons,
                                                  params.spread.frameWidthMm, width);
            if (!g.spreadWide(g.full0, SpreadKind::Glare, sigma, error)) return false;
        }

        g.resetParams();
        g.params.amount = std::min(1.0f, params.glare.amount);
        g.params.tint[0] = 1.0f;
        g.params.tint[1] = 0.96f;
        g.params.tint[2] = 0.92f;
        if (!g.runRgb(Impl::kGlareApply, fullGroups, 1, current, &g.full0, other, error))
            return false;
        swap();
    }

    if (params.diffusion.amount > 0.0f) {
        g.resetParams();
        if (!g.runSource(Impl::kLuma, fullGroups, 1, current, &g.full0, error))
            return false;
        const int radius =
            std::max(1, static_cast<int>(std::lround(params.diffusion.radius * width)));
        if (params.spread.model == SpreadModel::Box) {
            if (!g.blurWide(g.full0, radius, 2, error)) return false;
        } else {
            const float sigma = spreadSigmaPixels(params.diffusion.spreadMicrons,
                                                  params.spread.frameWidthMm, width);
            if (!g.spreadWide(g.full0, SpreadKind::Diffusion, sigma, error)) return false;
        }

        g.resetParams();
        g.params.amount = std::min(1.0f, params.diffusion.amount);
        if (!g.runRgb(Impl::kDiffusionApply, fullGroups, 1, current, &g.full0, other, error))
            return false;
        swap();
    }

    if (params.grain.amount > 0.0f) {
        const int rate = std::max(1, params.grain.rate);
        const int frameIndex = std::max(0, frame);
        const unsigned int fieldIndex = static_cast<unsigned int>(frameIndex / rate);
        const float phase =
            rate > 1 ? static_cast<float>(frameIndex % rate) / static_cast<float>(rate) : 0.0f;
        const float norm =
            1.0f / std::sqrt((1.0f - phase) * (1.0f - phase) + phase * phase);
        const int radius = std::max(0, static_cast<int>(std::lround(params.grain.size)));

        if (!g.buildGrainField(params.grain.seed, static_cast<int>(fieldIndex), radius, g.grain0,
                               error)) {
            return false;
        }
        if (rate > 1 &&
            !g.buildGrainField(params.grain.seed, static_cast<int>(fieldIndex) + 1, radius,
                               g.grain1, error)) {
            return false;
        }

        g.resetParams();
        g.params.phase = phase;
        g.params.norm = norm;
        g.params.scale = params.grain.amount * 0.14f;
        if (!g.runGrain(Impl::kGrainApply, fullGroups, 1, current, &g.grain0,
                        rate > 1 ? &g.grain1 : &g.grain0, other, error)) {
            return false;
        }
        swap();
    }

    g.context->CopyResource(g.staging.buffer, current->buffer);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(g.staging.buffer, 0, D3D11_MAP_READ, 0, &mapped))) {
        error = "could not read back the GPU texture result";
        return false;
    }
    const float* gpu = static_cast<const float*>(mapped.pData);
    for (std::size_t i = 0; i < count; ++i) {
        out[i * 3 + 0] = gpu[i * 4 + 0];
        out[i * 3 + 1] = gpu[i * 4 + 1];
        out[i * 3 + 2] = gpu[i * 4 + 2];
    }
    g.context->Unmap(g.staging.buffer, 0);
    g.lastMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return true;
}

#endif

#if SR_HAVE_D3D11

bool GpuTexture::Impl::initialize(std::string& error) {
    // attach() supplies the device; the shaders still have to be compiled here.
    if (device == nullptr) {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                     D3D11_SDK_VERSION, &device, &level, &context))) {
            error = "could not create a D3D11 device for the texture backend";
            return false;
        }
    }
    if (shaders[0] != nullptr) return true;

    static const char* kEntries[kShaderCount] = {
        "CSHalationSource", "CSLuma",       "CSBlurH",      "CSBlurV",
        "CSConvolveH",      "CSConvolveV",  "CSDownsample", "CSUpsample",
        "CSHalationApply",  "CSGlareApply", "CSDiffusionApply", "CSGrainField",
        "CSGrainApply",     "CSCopyRgb",
    };
    for (int i = 0; i < kShaderCount; ++i) {
        ID3DBlob* code = nullptr;
        ID3DBlob* messages = nullptr;
        const HRESULT compiled =
            D3DCompile(kShader, std::strlen(kShader), "gpu_texture.hlsl", nullptr, nullptr,
                       kEntries[i], "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &messages);
        if (FAILED(compiled)) {
            if (messages != nullptr) {
                error.assign(static_cast<const char*>(messages->GetBufferPointer()),
                             messages->GetBufferSize());
                messages->Release();
            } else {
                error = std::string("could not compile the GPU texture shader ") + kEntries[i];
            }
            return false;
        }
        const HRESULT created = device->CreateComputeShader(code->GetBufferPointer(),
                                                            code->GetBufferSize(), nullptr,
                                                            &shaders[i]);
        code->Release();
        if (FAILED(created)) {
            error = std::string("could not create the GPU texture shader ") + kEntries[i];
            return false;
        }
    }

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(Params);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &constantBuffer))) {
        error = "could not create the GPU texture constant buffer";
        return false;
    }

    D3D11_QUERY_DESC queryDesc{};
    queryDesc.Query = D3D11_QUERY_EVENT;
    device->CreateQuery(&queryDesc, &stopQuery);
    return true;
}

bool GpuTexture::Impl::createRes(UINT elements, UINT stride, Res& res, std::string& error) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = elements * stride;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = stride;
    if (FAILED(device->CreateBuffer(&desc, nullptr, &res.buffer))) {
        error = "could not allocate a GPU texture buffer";
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = elements;
    if (FAILED(device->CreateShaderResourceView(res.buffer, &srvDesc, &res.srv))) {
        error = "could not create a GPU texture view";
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = elements;
    if (FAILED(device->CreateUnorderedAccessView(res.buffer, &uavDesc, &res.uav))) {
        error = "could not create a GPU texture output view";
        return false;
    }
    res.elements = elements;
    return true;
}

void GpuTexture::Impl::releaseRes(Res& res) {
    if (res.srv != nullptr) res.srv->Release();
    if (res.uav != nullptr) res.uav->Release();
    if (res.buffer != nullptr) res.buffer->Release();
    res = Res{};
}

bool GpuTexture::Impl::createResView(ID3D11Buffer* buffer, UINT elements, Res& res,
                                     std::string& error) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    desc.Buffer.NumElements = elements;
    if (FAILED(device->CreateShaderResourceView(buffer, &desc, &res.srv))) {
        error = "could not create a view over the fused film output";
        return false;
    }
    // Borrowed: the film backend owns the buffer.
    res.buffer = buffer;
    res.elements = elements;
    return true;
}

void GpuTexture::Impl::releaseExternal() {
    if (external.srv != nullptr) external.srv->Release();
    external = Res{};
    externalBuffer = nullptr;
}

void GpuTexture::Impl::releaseResources() {
    if (context != nullptr) {
        ID3D11ShaderResourceView* nullSrv[4] = {nullptr, nullptr, nullptr, nullptr};
        ID3D11UnorderedAccessView* nullUav[1] = {nullptr};
        context->CSSetShaderResources(0, 4, nullSrv);
        context->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
    }
    releaseRes(rgbA);
    releaseRes(rgbB);
    releaseRes(full0);
    releaseRes(full1);
    releaseRes(low);
    releaseRes(lowScratch);
    releaseRes(kernelTaps);
    releaseRes(grain0);
    releaseRes(grain1);
    releaseRes(staging);
    releaseExternal();
    width = 0;
    height = 0;
    lowWidth = 0;
    lowHeight = 0;
}

bool GpuTexture::Impl::ensureCapacity(int w, int h, std::string& error) {
    if (w == width && h == height && rgbA.buffer != nullptr) return true;
    releaseResources();

    width = w;
    height = h;
    const UINT pixels = static_cast<UINT>(w) * static_cast<UINT>(h);
    const UINT rgbStride = 16;
    const UINT scalarStride = 4;

    if (!createRes(pixels, rgbStride, rgbA, error) ||
        !createRes(pixels, rgbStride, rgbB, error) ||
        !createRes(pixels, scalarStride, full0, error) ||
        !createRes(pixels, scalarStride, full1, error) ||
        !createRes(pixels, scalarStride, grain0, error) ||
        !createRes(pixels, scalarStride, grain1, error)) {
        return false;
    }

    // The pyramid is sized for the largest factor any effect asks for. Since
    // D054 the physical kernels pick their factor from the physical spread and
    // can go up to 8x, so the low-resolution scratch is allocated at full size
    // (it is only ever read, never all of it at once).
    lowWidth = w;
    lowHeight = h;
    const UINT maxLow = static_cast<UINT>(lowWidth) * static_cast<UINT>(lowHeight);
    if (!createRes(maxLow, scalarStride, low, error) ||
        !createRes(maxLow, scalarStride, lowScratch, error)) {
        return false;
    }
    if (!createRes(132, scalarStride, kernelTaps, error)) return false;

    D3D11_BUFFER_DESC stagingDesc{};
    stagingDesc.ByteWidth = pixels * rgbStride;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(device->CreateBuffer(&stagingDesc, nullptr, &staging.buffer))) {
        error = "could not allocate the GPU texture readback buffer";
        return false;
    }
    staging.elements = pixels;
    return true;
}

void GpuTexture::Impl::resetParams() {
    std::memset(&params, 0, sizeof(params));
    params.width = width;
    params.height = height;
    params.count = static_cast<unsigned int>(width) * static_cast<unsigned int>(height);
    params.lowWidth = lowWidth;
    params.lowHeight = lowHeight;
    params.blurWidth = width;
    params.blurHeight = height;
}

bool GpuTexture::Impl::dispatch(Shader shader, UINT groupsX, UINT groupsY, Res* srv0, Res* srv1,
                                Res* srv2, Res* srv3, int uavSlot, Res* uav, std::string& error) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        error = "could not map the GPU texture constant buffer";
        return false;
    }
    std::memcpy(mapped.pData, &params, sizeof(params));
    context->Unmap(constantBuffer, 0);

    ID3D11Buffer* cbs[] = {constantBuffer};
    ID3D11ShaderResourceView* srvs[] = {srv0 != nullptr ? srv0->srv : nullptr,
                                        srv1 != nullptr ? srv1->srv : nullptr,
                                        srv2 != nullptr ? srv2->srv : nullptr,
                                        srv3 != nullptr ? srv3->srv : nullptr};
    ID3D11UnorderedAccessView* uavs[2] = {nullptr, nullptr};
    uavs[uavSlot] = uav != nullptr ? uav->uav : nullptr;
    context->CSSetShader(shaders[shader], nullptr, 0);
    context->CSSetConstantBuffers(0, 1, cbs);
    context->CSSetShaderResources(0, 4, srvs);
    context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
    context->Dispatch(groupsX, groupsY, 1);

    // Unbind so the same buffer can be read by the next pass.
    ID3D11ShaderResourceView* nullSrv[4] = {nullptr, nullptr, nullptr, nullptr};
    ID3D11UnorderedAccessView* nullUav[2] = {nullptr, nullptr};
    context->CSSetShaderResources(0, 4, nullSrv);
    context->CSSetUnorderedAccessViews(0, 2, nullUav, nullptr);
    return true;
}

bool GpuTexture::Impl::runScalar(Shader shader, UINT groupsX, UINT groupsY, Res* input,
                                 Res* output, std::string& error) {
    return dispatch(shader, groupsX, groupsY, nullptr, input, nullptr, nullptr, 1, output, error);
}
bool GpuTexture::Impl::runSource(Shader shader, UINT groupsX, UINT groupsY, Res* rgb,
                                 Res* output, std::string& error) {
    return dispatch(shader, groupsX, groupsY, rgb, nullptr, nullptr, nullptr, 1, output, error);
}

bool GpuTexture::Impl::runRgb(Shader shader, UINT groupsX, UINT groupsY, Res* rgb, Res* scalar,
                              Res* output, std::string& error) {
    return dispatch(shader, groupsX, groupsY, rgb, scalar, nullptr, nullptr, 0, output, error);
}

bool GpuTexture::Impl::runGrain(Shader shader, UINT groupsX, UINT groupsY, Res* rgb, Res* fieldA,
                                Res* fieldB, Res* output, std::string& error) {
    return dispatch(shader, groupsX, groupsY, rgb, fieldA, fieldB, nullptr, 0, output, error);
}

// The convolve passes read the scalar map at t1 and the taps at t3.
bool GpuTexture::Impl::runConvolve(Shader shader, UINT groupsX, UINT groupsY, Res* input,
                                   Res* output, std::string& error) {
    return dispatch(shader, groupsX, groupsY, nullptr, input, nullptr, &kernelTaps, 1, output,
                    error);
}

bool GpuTexture::Impl::blurWide(Res& buffer, int radius, int factor, std::string& error) {
    if (radius < 1) return true;
    factor = std::min(std::max(factor, 1), std::min(width, height));
    const UINT fullGroups = (params.count + 63) / 64;

    if (factor <= 1 || radius < factor * 2) {
        params.blurWidth = width;
        params.blurHeight = height;
        params.radius = radius;
        for (int pass = 0; pass < 3; ++pass) {
            if (!runScalar(kBlurH, 1, static_cast<UINT>((height + 63) / 64), &buffer,
                          &full1, error)) {
                return false;
            }
            if (!runScalar(kBlurV, static_cast<UINT>((width + 63) / 64), 1, &full1,
                          &buffer, error)) {
                return false;
            }
        }
        return true;
    }

    // Keep the low-resolution geometry in step with ensureCapacity: the buffer
    // was sized for the largest factor, so carve out this factor's slice.
    const int lw = (width + factor - 1) / factor;
    const int lh = (height + factor - 1) / factor;
    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    if (!runScalar(kDownsample, static_cast<UINT>((lw * lh + 63) / 64), 1, &buffer, &low, error)) {
        return false;
    }

    params.blurWidth = lw;
    params.blurHeight = lh;
    params.radius = std::max(1, radius / factor);
    for (int pass = 0; pass < 2; ++pass) {
        if (!runScalar(kBlurH, 1, static_cast<UINT>((lh + 63) / 64), &low, &lowScratch,
                      error)) {
            return false;
        }
        if (!runScalar(kBlurV, static_cast<UINT>((lw + 63) / 64), 1, &lowScratch, &low,
                      error)) {
            return false;
        }
    }

    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    return runScalar(kUpsample, fullGroups, 1, &low, &buffer, error);
}

bool GpuTexture::Impl::setKernel(const SpreadKernel& kernel, std::string& error) {
    constexpr std::size_t kMaxTaps = 132;
    params.kernelRadius = kernel.radius;
    float taps[kMaxTaps] = {};
    const std::size_t count = std::min(kernel.taps.size(), kMaxTaps);
    for (std::size_t i = 0; i < count; ++i) taps[i] = kernel.taps[i];
    if (kernelTaps.buffer == nullptr) {
        error = "the GPU kernel buffer is not allocated";
        return false;
    }
    // Structured buffer: HLSL pads constant-buffer arrays to 16 bytes per
    // element, so the taps cannot ride along with the scalar parameters.
    context->UpdateSubresource(kernelTaps.buffer, 0, nullptr, taps, 0, 0);
    return true;
}

bool GpuTexture::Impl::spreadWide(Res& buffer, SpreadKind kind, float sigmaPx,
                                  std::string& error) {
    const int radiusFull = spreadKernelRadius(kind, sigmaPx);
    if (radiusFull < 1) return true;

    const int factor = spreadDownsampleFactor(kind, sigmaPx);
    const UINT fullGroups = (params.count + 63) / 64;

    if (factor <= 1 || radiusFull < factor * 2) {
        params.blurWidth = width;
        params.blurHeight = height;
        if (!setKernel(buildSpreadKernel(kind, sigmaPx), error)) return false;
        if (!runConvolve(kConvolveH, 1, static_cast<UINT>((height + 63) / 64), &buffer, &full1,
                         error)) {
            return false;
        }
        return runConvolve(kConvolveV, static_cast<UINT>((width + 63) / 64), 1, &full1, &buffer,
                           error);
    }

    const int lw = (width + factor - 1) / factor;
    const int lh = (height + factor - 1) / factor;
    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    if (!runScalar(kDownsample, static_cast<UINT>((lw * lh + 63) / 64), 1, &buffer, &low, error)) {
        return false;
    }

    params.blurWidth = lw;
    params.blurHeight = lh;
    if (!setKernel(buildSpreadKernel(kind, sigmaPx / static_cast<float>(factor)), error)) {
        return false;
    }
    if (!runConvolve(kConvolveH, 1, static_cast<UINT>((lh + 63) / 64), &low, &lowScratch, error)) {
        return false;
    }
    if (!runConvolve(kConvolveV, static_cast<UINT>((lw + 63) / 64), 1, &lowScratch, &low, error)) {
        return false;
    }

    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    return runScalar(kUpsample, fullGroups, 1, &low, &buffer, error);
}

bool GpuTexture::Impl::buildGrainField(unsigned seed, int index, int radius, Res& target,
                                       std::string& error) {
    resetParams();
    params.seed = seed;
    params.fieldIndex = index;
    const UINT fullGroups = (params.count + 63) / 64;
    if (!runScalar(kGrainField, fullGroups, 1, nullptr, &target, error)) return false;

    // Same two blur passes as buildGrainField() on the CPU. Radius 0 returns
    // early there, so the raw hash field is the field in that case.
    if (radius < 1) return true;
    params.blurWidth = width;
    params.blurHeight = height;
    params.radius = radius;
    for (int pass = 0; pass < 2; ++pass) {
        if (!runScalar(kBlurH, 1, static_cast<UINT>((height + 63) / 64), &target, &full1,
                      error)) {
            return false;
        }
        if (!runScalar(kBlurV, static_cast<UINT>((width + 63) / 64), 1, &full1, &target,
                      error)) {
            return false;
        }
    }
    return true;
}

#else

bool GpuTexture::Impl::initialize(std::string&) { return false; }
bool GpuTexture::Impl::ensureCapacity(int, int, std::string&) { return false; }
void GpuTexture::Impl::releaseResources() {}

#endif

GpuTexture::~GpuTexture() {
    if (!impl_) return;
#if SR_HAVE_D3D11
    impl_->releaseResources();
    for (ID3D11ComputeShader*& shader : impl_->shaders) {
        if (shader != nullptr) shader->Release();
        shader = nullptr;
    }
    if (impl_->constantBuffer != nullptr) impl_->constantBuffer->Release();
    if (impl_->stopQuery != nullptr) impl_->stopQuery->Release();
    if (impl_->context != nullptr) impl_->context->Release();
    if (impl_->device != nullptr && impl_->ownsDevice) impl_->device->Release();
#endif
}

}  // namespace sr
