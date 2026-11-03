// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Metal implementation of the spatial texture chain (D057). The kernels are
// pass-by-pass translations of gpu_texture.cpp / sr::applyTexture: the
// separable box blur keeps one thread per row/column so the floating point
// summation order matches the CPU reference, the physical PSF taps come from
// the CPU (D054), and the wide-blur shortcut uses the same factors and rounding.
//
// Apple-only: this file replaces gpu_texture.cpp on macOS. The whole chain is
// encoded into one command buffer, and each dispatch carries its parameters
// inline with setBytes:, so the CPU never races the GPU on a shared constant
// buffer.

#include "gpu_texture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace sr {
namespace {

const char* kShaderSource = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct Params {
    float tint[4];
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
    int kernelRadius;
    float pad4;
    float pad5;
    float pad6;
};

float lumaOf(float3 c) {
    return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

kernel void halationSource(uint gid [[thread_position_in_grid]],
                           constant Params& p [[buffer(0)]],
                           device const float4* rgbIn [[buffer(1)]],
                           device const float* scalarIn [[buffer(2)]],
                           device const float* scalarIn2 [[buffer(3)]],
                           constant float* tapsIn [[buffer(4)]],
                           device float4* rgbOut [[buffer(5)]],
                           device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    float luma = lumaOf(rgbIn[gid].xyz);
    float span = max(1e-3f, 1.0f - p.threshold);
    float excess = clamp((luma - p.threshold) / span, 0.0f, 1.0f);
    scalarOut[gid] = excess * excess;
}

kernel void luma(uint gid [[thread_position_in_grid]],
                 constant Params& p [[buffer(0)]],
                 device const float4* rgbIn [[buffer(1)]],
                 device const float* scalarIn [[buffer(2)]],
                 device const float* scalarIn2 [[buffer(3)]],
                 constant float* tapsIn [[buffer(4)]],
                 device float4* rgbOut [[buffer(5)]],
                 device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    scalarOut[gid] = lumaOf(rgbIn[gid].xyz);
}

kernel void blurH(uint2 gid [[thread_position_in_grid]],
                  constant Params& p [[buffer(0)]],
                  device const float4* rgbIn [[buffer(1)]],
                  device const float* scalarIn [[buffer(2)]],
                  device const float* scalarIn2 [[buffer(3)]],
                  constant float* tapsIn [[buffer(4)]],
                  device float4* rgbOut [[buffer(5)]],
                  device float* scalarOut [[buffer(6)]]) {
    int y = int(gid.y);
    if (y >= p.blurHeight) return;
    int w = p.blurWidth;
    int r = p.radius;
    int window = 2 * r + 1;
    uint row = uint(y) * uint(w);
    float sum = 0.0f;
    for (int x = -r; x <= r; ++x) {
        sum += scalarIn[row + uint(clamp(x, 0, w - 1))];
    }
    for (int x = 0; x < w; ++x) {
        scalarOut[row + uint(x)] = sum / float(window);
        sum -= scalarIn[row + uint(clamp(x - r, 0, w - 1))];
        sum += scalarIn[row + uint(clamp(x + r + 1, 0, w - 1))];
    }
}

kernel void blurV(uint2 gid [[thread_position_in_grid]],
                  constant Params& p [[buffer(0)]],
                  device const float4* rgbIn [[buffer(1)]],
                  device const float* scalarIn [[buffer(2)]],
                  device const float* scalarIn2 [[buffer(3)]],
                  constant float* tapsIn [[buffer(4)]],
                  device float4* rgbOut [[buffer(5)]],
                  device float* scalarOut [[buffer(6)]]) {
    int x = int(gid.x);
    if (x >= p.blurWidth) return;
    int w = p.blurWidth;
    int h = p.blurHeight;
    int r = p.radius;
    int window = 2 * r + 1;
    float sum = 0.0f;
    for (int y = -r; y <= r; ++y) {
        sum += scalarIn[uint(clamp(y, 0, h - 1)) * uint(w) + uint(x)];
    }
    for (int y = 0; y < h; ++y) {
        scalarOut[uint(y) * uint(w) + uint(x)] = sum / float(window);
        sum -= scalarIn[uint(clamp(y - r, 0, h - 1)) * uint(w) + uint(x)];
        sum += scalarIn[uint(clamp(y + r + 1, 0, h - 1)) * uint(w) + uint(x)];
    }
}

kernel void convolveH(uint2 gid [[thread_position_in_grid]],
                      constant Params& p [[buffer(0)]],
                      device const float4* rgbIn [[buffer(1)]],
                      device const float* scalarIn [[buffer(2)]],
                      device const float* scalarIn2 [[buffer(3)]],
                      constant float* tapsIn [[buffer(4)]],
                      device float4* rgbOut [[buffer(5)]],
                      device float* scalarOut [[buffer(6)]]) {
    int y = int(gid.y);
    if (y >= p.blurHeight) return;
    int w = p.blurWidth;
    int r = p.kernelRadius;
    uint row = uint(y) * uint(w);
    for (int x = 0; x < w; ++x) {
        float sum = 0.0f;
        for (int k = -r; k <= r; ++k) {
            sum += tapsIn[k + r] * scalarIn[row + uint(clamp(x + k, 0, w - 1))];
        }
        scalarOut[row + uint(x)] = sum;
    }
}

kernel void convolveV(uint2 gid [[thread_position_in_grid]],
                      constant Params& p [[buffer(0)]],
                      device const float4* rgbIn [[buffer(1)]],
                      device const float* scalarIn [[buffer(2)]],
                      device const float* scalarIn2 [[buffer(3)]],
                      constant float* tapsIn [[buffer(4)]],
                      device float4* rgbOut [[buffer(5)]],
                      device float* scalarOut [[buffer(6)]]) {
    int x = int(gid.x);
    if (x >= p.blurWidth) return;
    int w = p.blurWidth;
    int h = p.blurHeight;
    int r = p.kernelRadius;
    for (int y = 0; y < h; ++y) {
        float sum = 0.0f;
        for (int k = -r; k <= r; ++k) {
            sum += tapsIn[k + r] *
                   scalarIn[uint(clamp(y + k, 0, h - 1)) * uint(w) + uint(x)];
        }
        scalarOut[uint(y) * uint(w) + uint(x)] = sum;
    }
}

kernel void downsample(uint gid [[thread_position_in_grid]],
                       constant Params& p [[buffer(0)]],
                       device const float4* rgbIn [[buffer(1)]],
                       device const float* scalarIn [[buffer(2)]],
                       device const float* scalarIn2 [[buffer(3)]],
                       constant float* tapsIn [[buffer(4)]],
                       device float4* rgbOut [[buffer(5)]],
                       device float* scalarOut [[buffer(6)]]) {
    uint lowCount = uint(p.lowWidth * p.lowHeight);
    if (gid >= lowCount) return;
    int lx = int(gid % uint(p.lowWidth));
    int ly = int(gid / uint(p.lowWidth));
    float inv = 1.0f / float(p.factor * p.factor);
    int x0 = lx * p.factor;
    int y0 = ly * p.factor;
    float acc = 0.0f;
    for (int y = y0; y < min(y0 + p.factor, p.height); ++y) {
        for (int x = x0; x < min(x0 + p.factor, p.width); ++x) {
            acc += scalarIn[uint(y * p.width + x)] * inv;
        }
    }
    scalarOut[gid] = acc;
}

kernel void upsample(uint gid [[thread_position_in_grid]],
                     constant Params& p [[buffer(0)]],
                     device const float4* rgbIn [[buffer(1)]],
                     device const float* scalarIn [[buffer(2)]],
                     device const float* scalarIn2 [[buffer(3)]],
                     constant float* tapsIn [[buffer(4)]],
                     device float4* rgbOut [[buffer(5)]],
                     device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    int x = int(gid % uint(p.width));
    int y = int(gid / uint(p.width));
    float fy = (float(y) + 0.5f) / float(p.factor) - 0.5f;
    int y0 = clamp(int(floor(fy)), 0, p.lowHeight - 1);
    int y1 = min(p.lowHeight - 1, y0 + 1);
    float ty = clamp(fy - float(y0), 0.0f, 1.0f);
    float fx = (float(x) + 0.5f) / float(p.factor) - 0.5f;
    int x0 = clamp(int(floor(fx)), 0, p.lowWidth - 1);
    int x1 = min(p.lowWidth - 1, x0 + 1);
    float tx = clamp(fx - float(x0), 0.0f, 1.0f);
    float top = scalarIn[uint(y0 * p.lowWidth + x0)] * (1.0f - tx) +
                scalarIn[uint(y0 * p.lowWidth + x1)] * tx;
    float bottom = scalarIn[uint(y1 * p.lowWidth + x0)] * (1.0f - tx) +
                   scalarIn[uint(y1 * p.lowWidth + x1)] * tx;
    scalarOut[gid] = top * (1.0f - ty) + bottom * ty;
}

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

kernel void grainField(uint gid [[thread_position_in_grid]],
                       constant Params& p [[buffer(0)]],
                       device const float4* rgbIn [[buffer(1)]],
                       device const float* scalarIn [[buffer(2)]],
                       device const float* scalarIn2 [[buffer(3)]],
                       constant float* tapsIn [[buffer(4)]],
                       device float4* rgbOut [[buffer(5)]],
                       device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    uint x = gid % uint(p.width);
    uint y = gid / uint(p.width);
    uint h = mix32(mix32(p.seed, uint(p.fieldIndex)), mix32(x, y));
    scalarOut[gid] = float(h) / 2147483648.0f - 1.0f;
}

kernel void halationApply(uint gid [[thread_position_in_grid]],
                          constant Params& p [[buffer(0)]],
                          device const float4* rgbIn [[buffer(1)]],
                          device const float* scalarIn [[buffer(2)]],
                          device const float* scalarIn2 [[buffer(3)]],
                          constant float* tapsIn [[buffer(4)]],
                          device float4* rgbOut [[buffer(5)]],
                          device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    float4 c = rgbIn[gid];
    float a = scalarIn[gid] * p.amount;
    rgbOut[gid] = float4(min(1.0f, c.x + a * p.tint[0]), min(1.0f, c.y + a * p.tint[1]),
                         min(1.0f, c.z + a * p.tint[2]), 1.0f);
}

kernel void glareApply(uint gid [[thread_position_in_grid]],
                       constant Params& p [[buffer(0)]],
                       device const float4* rgbIn [[buffer(1)]],
                       device const float* scalarIn [[buffer(2)]],
                       device const float* scalarIn2 [[buffer(3)]],
                       constant float* tapsIn [[buffer(4)]],
                       device float4* rgbOut [[buffer(5)]],
                       device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    float4 c = rgbIn[gid];
    float v = scalarIn[gid];
    float3 veiling = (p.amount * v) * float3(p.tint[0], p.tint[1], p.tint[2]);
    rgbOut[gid] = float4(min(1.0f, 1.0f - (1.0f - c.x) * (1.0f - veiling.x)),
                         min(1.0f, 1.0f - (1.0f - c.y) * (1.0f - veiling.y)),
                         min(1.0f, 1.0f - (1.0f - c.z) * (1.0f - veiling.z)), 1.0f);
}

kernel void diffusionApply(uint gid [[thread_position_in_grid]],
                           constant Params& p [[buffer(0)]],
                           device const float4* rgbIn [[buffer(1)]],
                           device const float* scalarIn [[buffer(2)]],
                           device const float* scalarIn2 [[buffer(3)]],
                           constant float* tapsIn [[buffer(4)]],
                           device float4* rgbOut [[buffer(5)]],
                           device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    float4 c = rgbIn[gid];
    float luma = lumaOf(c.xyz);
    float offset = (scalarIn[gid] - luma) * p.amount;
    rgbOut[gid] = float4(clamp(c.x + offset, 0.0f, 1.0f), clamp(c.y + offset, 0.0f, 1.0f),
                         clamp(c.z + offset, 0.0f, 1.0f), 1.0f);
}

kernel void grainApply(uint gid [[thread_position_in_grid]],
                       constant Params& p [[buffer(0)]],
                       device const float4* rgbIn [[buffer(1)]],
                       device const float* scalarIn [[buffer(2)]],
                       device const float* scalarIn2 [[buffer(3)]],
                       constant float* tapsIn [[buffer(4)]],
                       device float4* rgbOut [[buffer(5)]],
                       device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    float4 c = rgbIn[gid];
    float luma = lumaOf(c.xyz);
    float weight = 1.0f - abs(2.0f * luma - 1.0f);
    float noise = (scalarIn[gid] * (1.0f - p.phase) + scalarIn2[gid] * p.phase) * p.norm;
    float delta = noise * p.scale * weight;
    rgbOut[gid] = float4(clamp(c.x + delta, 0.0f, 1.0f), clamp(c.y + delta, 0.0f, 1.0f),
                         clamp(c.z + delta, 0.0f, 1.0f), 1.0f);
}

kernel void copyRgb(uint gid [[thread_position_in_grid]],
                    constant Params& p [[buffer(0)]],
                    device const float4* rgbIn [[buffer(1)]],
                    device const float* scalarIn [[buffer(2)]],
                    device const float* scalarIn2 [[buffer(3)]],
                    constant float* tapsIn [[buffer(4)]],
                    device float4* rgbOut [[buffer(5)]],
                    device float* scalarOut [[buffer(6)]]) {
    if (gid >= p.count) return;
    rgbOut[gid] = rgbIn[gid];
}
)MSL";

}  // namespace

struct GpuTexture::Impl {
    Impl() {
        for (auto& pipeline : pipelines) pipeline = nil;
    }

    struct Res {
        id<MTLBuffer> buffer = nil;
        std::size_t elements = 0;
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

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLComputePipelineState> pipelines[kShaderCount] = {};
    // One-element buffers so every dispatch can bind all argument slots.
    id<MTLBuffer> dummyRgb = nil;
    id<MTLBuffer> dummyScalar = nil;

    Res rgbA;
    Res rgbB;
    Res full0;
    Res full1;
    Res low;
    Res lowScratch;
    Res grain0;
    Res grain1;
    // An input buffer owned by another backend (the Metal film pass, D049).
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

    double lastMs = 0.0;
    bool initialize(std::string& error);
    bool ensureCapacity(int w, int h, std::string& error);
    void releaseResources();
    bool createRes(std::size_t elements, std::size_t stride, Res& res, std::string& error);
    bool runChain(int width, int height, const TextureParams& params, int frame, float* out,
                  std::string& error, Res* start = nullptr);
    bool dispatch(Shader shader, unsigned int groupsX, unsigned int groupsY,
                  unsigned int threadX, unsigned int threadY, Res* rgbIn, Res* scalarIn,
                  Res* scalarIn2, Res* rgbOut, Res* scalarOut, const float* taps,
                  std::size_t tapCount, id<MTLCommandBuffer> command);
    bool runSource(Shader shader, unsigned int groups, Res* rgb, Res* output,
                   id<MTLCommandBuffer> command);
    bool runScalar(Shader shader, unsigned int groupsX, unsigned int groupsY,
                   unsigned int threadX, unsigned int threadY, Res* input, Res* output,
                   id<MTLCommandBuffer> command);
    bool runRgb(Shader shader, unsigned int groups, Res* rgb, Res* scalar, Res* output,
                id<MTLCommandBuffer> command);
    bool runGrain(Shader shader, unsigned int groups, Res* rgb, Res* fieldA, Res* fieldB,
                  Res* output, id<MTLCommandBuffer> command);
    bool runConvolve(Shader shader, unsigned int groupsX, unsigned int groupsY,
                     unsigned int threadX, unsigned int threadY, Res* input, Res* output,
                     const float* taps, std::size_t tapCount, id<MTLCommandBuffer> command);
    bool blurWide(Res& buffer, int radius, int factor, id<MTLCommandBuffer> command);
    bool spreadWide(Res& buffer, SpreadKind kind, float sigmaPx, id<MTLCommandBuffer> command);
    bool buildGrainField(unsigned int seed, int index, int radius, Res& target,
                         id<MTLCommandBuffer> command);
    void resetParams();
};

GpuTexture::GpuTexture() : impl_(std::make_unique<Impl>()) {}

double GpuTexture::lastMilliseconds() const { return impl_->lastMs; }

bool GpuTexture::Impl::initialize(std::string& error) {
    if (device == nil) {
        device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            NSArray<id<MTLDevice>>* all = MTLCopyAllDevices();
            if (all.count > 0) device = all.firstObject;
        }
        if (device == nil) {
            error = "no Metal device is available (MTLCreateSystemDefaultDevice returned nil)";
            return false;
        }
        queue = [device newCommandQueue];
        if (queue == nil) {
            error = "could not create a Metal command queue";
            return false;
        }
    }
    if (pipelines[0] != nil) return true;

    NSError* compileError = nil;
    // Same reason as the film kernel (D056): keep IEEE semantics so the
    // summation order and edge cases match the CPU reference.
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = NO;
    id<MTLLibrary> library =
        [device newLibraryWithSource:[NSString stringWithUTF8String:kShaderSource]
                             options:options
                               error:&compileError];
    if (library == nil) {
        error = "could not compile the Metal texture shader";
        if (compileError != nil) {
            error += ": ";
            error += [compileError.localizedDescription UTF8String];
        }
        return false;
    }

    static const char* kEntries[kShaderCount] = {
        "halationSource", "luma",        "blurH",        "blurV",
        "convolveH",     "convolveV",   "downsample",   "upsample",
        "halationApply", "glareApply",  "diffusionApply", "grainField",
        "grainApply",    "copyRgb",
    };
    for (int i = 0; i < kShaderCount; ++i) {
        id<MTLFunction> function = [library newFunctionWithName:
                                                 [NSString stringWithUTF8String:kEntries[i]]];
        if (function == nil) {
            error = std::string("the Metal texture shader has no ") + kEntries[i];
            return false;
        }
        NSError* pipelineError = nil;
        pipelines[i] = [device newComputePipelineStateWithFunction:function error:&pipelineError];
        if (pipelines[i] == nil) {
            error = std::string("could not create the Metal pipeline ") + kEntries[i];
            if (pipelineError != nil) {
                error += ": ";
                error += [pipelineError.localizedDescription UTF8String];
            }
            return false;
        }
    }

    dummyRgb = [device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    dummyScalar = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    if (dummyRgb == nil || dummyScalar == nil) {
        error = "could not allocate the Metal texture scratch buffers";
        return false;
    }
    return true;
}

bool GpuTexture::Impl::createRes(std::size_t elements, std::size_t stride, Res& res,
                                 std::string& error) {
    const NSUInteger bytes = static_cast<NSUInteger>(elements * stride);
    res.buffer = [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    if (res.buffer == nil) {
        error = "could not allocate a Metal texture buffer";
        return false;
    }
    res.elements = elements;
    return true;
}

void GpuTexture::Impl::releaseResources() {
    rgbA = Res{};
    rgbB = Res{};
    full0 = Res{};
    full1 = Res{};
    low = Res{};
    lowScratch = Res{};
    grain0 = Res{};
    grain1 = Res{};
    external = Res{};
    externalBuffer = nullptr;
    width = 0;
    height = 0;
    lowWidth = 0;
    lowHeight = 0;
}

bool GpuTexture::Impl::ensureCapacity(int w, int h, std::string& error) {
    if (w == width && h == height && rgbA.buffer != nil) return true;
    releaseResources();

    width = w;
    height = h;
    const std::size_t pixels = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

    if (!createRes(pixels, 16, rgbA, error) || !createRes(pixels, 16, rgbB, error) ||
        !createRes(pixels, sizeof(float), full0, error) ||
        !createRes(pixels, sizeof(float), full1, error) ||
        !createRes(pixels, sizeof(float), grain0, error) ||
        !createRes(pixels, sizeof(float), grain1, error)) {
        return false;
    }

    // The pyramid is sized for the largest factor any effect asks for (up to 8x
    // since D054), so the low-resolution scratch is allocated at full size.
    lowWidth = w;
    lowHeight = h;
    const std::size_t maxLow = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    if (!createRes(maxLow, sizeof(float), low, error) ||
        !createRes(maxLow, sizeof(float), lowScratch, error)) {
        return false;
    }
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

bool GpuTexture::Impl::dispatch(Shader shader, unsigned int groupsX, unsigned int groupsY,
                                unsigned int threadX, unsigned int threadY, Res* rgbIn,
                                Res* scalarIn, Res* scalarIn2, Res* rgbOut, Res* scalarOut,
                                const float* taps, std::size_t tapCount,
                                id<MTLCommandBuffer> command) {
    id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
    if (encoder == nil) return false;
    [encoder setComputePipelineState:pipelines[shader]];
    [encoder setBytes:&params length:sizeof(params) atIndex:0];
    [encoder setBuffer:(rgbIn != nullptr ? rgbIn->buffer : dummyRgb) offset:0 atIndex:1];
    [encoder setBuffer:(scalarIn != nullptr ? scalarIn->buffer : dummyScalar) offset:0 atIndex:2];
    [encoder setBuffer:(scalarIn2 != nullptr ? scalarIn2->buffer : dummyScalar) offset:0
               atIndex:3];
    // The PSF taps travel inline with the dispatch: a shared buffer would be
    // overwritten by the next effect before this command buffer executes.
    static const float kZeroTap[1] = {0.0f};
    const void* tapData = taps != nullptr ? static_cast<const void*>(taps) : kZeroTap;
    const NSUInteger tapBytes =
        taps != nullptr ? static_cast<NSUInteger>(tapCount * sizeof(float)) : sizeof(float);
    [encoder setBytes:tapData length:tapBytes atIndex:4];
    [encoder setBuffer:(rgbOut != nullptr ? rgbOut->buffer : dummyRgb) offset:0 atIndex:5];
    [encoder setBuffer:(scalarOut != nullptr ? scalarOut->buffer : dummyScalar) offset:0
               atIndex:6];
    // Group counts (like the D3D11 dispatch), not total threads: the kernels
    // already guard the tail with `gid >= count`.
    [encoder dispatchThreadgroups:MTLSizeMake(groupsX, groupsY, 1)
            threadsPerThreadgroup:MTLSizeMake(threadX, threadY, 1)];
    [encoder endEncoding];
    return true;
}

bool GpuTexture::Impl::runSource(Shader shader, unsigned int groups, Res* rgb, Res* output,
                                 id<MTLCommandBuffer> command) {
    return dispatch(shader, groups, 1, 64, 1, rgb, nullptr, nullptr, nullptr, output, nullptr, 0,
                    command);
}

bool GpuTexture::Impl::runScalar(Shader shader, unsigned int groupsX, unsigned int groupsY,
                                 unsigned int threadX, unsigned int threadY, Res* input,
                                 Res* output, id<MTLCommandBuffer> command) {
    return dispatch(shader, groupsX, groupsY, threadX, threadY, nullptr, input, nullptr, nullptr,
                    output, nullptr, 0, command);
}

bool GpuTexture::Impl::runRgb(Shader shader, unsigned int groups, Res* rgb, Res* scalar,
                              Res* output, id<MTLCommandBuffer> command) {
    return dispatch(shader, groups, 1, 64, 1, rgb, scalar, nullptr, output, nullptr, nullptr, 0,
                    command);
}

bool GpuTexture::Impl::runGrain(Shader shader, unsigned int groups, Res* rgb, Res* fieldA,
                                Res* fieldB, Res* output, id<MTLCommandBuffer> command) {
    return dispatch(shader, groups, 1, 64, 1, rgb, fieldA, fieldB, output, nullptr, nullptr, 0,
                    command);
}

bool GpuTexture::Impl::runConvolve(Shader shader, unsigned int groupsX, unsigned int groupsY,
                                   unsigned int threadX, unsigned int threadY, Res* input,
                                   Res* output, const float* taps, std::size_t tapCount,
                                   id<MTLCommandBuffer> command) {
    return dispatch(shader, groupsX, groupsY, threadX, threadY, nullptr, input, nullptr, nullptr,
                    output, taps, tapCount, command);
}

bool GpuTexture::Impl::blurWide(Res& buffer, int radius, int factor,
                                id<MTLCommandBuffer> command) {
    if (radius < 1) return true;
    factor = std::min(std::max(factor, 1), std::min(width, height));
    const unsigned int fullGroups =
        static_cast<unsigned int>((params.count + 63) / 64);

    if (factor <= 1 || radius < factor * 2) {
        params.blurWidth = width;
        params.blurHeight = height;
        params.radius = radius;
        for (int pass = 0; pass < 3; ++pass) {
            if (!runScalar(kBlurH, 1, static_cast<unsigned int>((height + 63) / 64), 1, 64, &buffer,
                           &full1, command)) {
                return false;
            }
            if (!runScalar(kBlurV, static_cast<unsigned int>((width + 63) / 64), 1, 64, 1, &full1,
                           &buffer, command)) {
                return false;
            }
        }
        return true;
    }

    const int lw = (width + factor - 1) / factor;
    const int lh = (height + factor - 1) / factor;
    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    if (!runScalar(kDownsample, static_cast<unsigned int>((lw * lh + 63) / 64), 1, 64, 1, &buffer,
                   &low, command)) {
        return false;
    }

    params.blurWidth = lw;
    params.blurHeight = lh;
    params.radius = std::max(1, radius / factor);
    for (int pass = 0; pass < 2; ++pass) {
        if (!runScalar(kBlurH, 1, static_cast<unsigned int>((lh + 63) / 64), 1, 64, &low, &lowScratch,
                       command)) {
            return false;
        }
        if (!runScalar(kBlurV, static_cast<unsigned int>((lw + 63) / 64), 1, 64, 1, &lowScratch, &low,
                       command)) {
            return false;
        }
    }

    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    return runScalar(kUpsample, fullGroups, 1, 64, 1, &low, &buffer, command);
}

bool GpuTexture::Impl::spreadWide(Res& buffer, SpreadKind kind, float sigmaPx,
                                  id<MTLCommandBuffer> command) {
    const int radiusFull = spreadKernelRadius(kind, sigmaPx);
    if (radiusFull < 1) return true;

    const int factor = spreadDownsampleFactor(kind, sigmaPx);
    const unsigned int fullGroups =
        static_cast<unsigned int>((params.count + 63) / 64);

    if (factor <= 1 || radiusFull < factor * 2) {
        const SpreadKernel kernel = buildSpreadKernel(kind, sigmaPx);
        params.blurWidth = width;
        params.blurHeight = height;
        params.kernelRadius = kernel.radius;
        const float* taps = kernel.taps.data();
        const std::size_t tapCount = kernel.taps.size();
        if (!runConvolve(kConvolveH, 1, static_cast<unsigned int>((height + 63) / 64), 1, 64,
                         &buffer, &full1, taps, tapCount, command)) {
            return false;
        }
        return runConvolve(kConvolveV, static_cast<unsigned int>((width + 63) / 64), 1, 64, 1,
                           &full1, &buffer, taps, tapCount, command);
    }

    const int lw = (width + factor - 1) / factor;
    const int lh = (height + factor - 1) / factor;
    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    if (!runScalar(kDownsample, static_cast<unsigned int>((lw * lh + 63) / 64), 1, 64, 1, &buffer,
                   &low, command)) {
        return false;
    }

    const SpreadKernel kernel = buildSpreadKernel(kind, sigmaPx / static_cast<float>(factor));
    params.blurWidth = lw;
    params.blurHeight = lh;
    params.kernelRadius = kernel.radius;
    const float* taps = kernel.taps.data();
    const std::size_t tapCount = kernel.taps.size();
    if (!runConvolve(kConvolveH, 1, static_cast<unsigned int>((lh + 63) / 64), 1, 64, &low,
                     &lowScratch, taps, tapCount, command)) {
        return false;
    }
    if (!runConvolve(kConvolveV, static_cast<unsigned int>((lw + 63) / 64), 1, 64, 1, &lowScratch,
                     &low, taps, tapCount, command)) {
        return false;
    }

    params.factor = factor;
    params.lowWidth = lw;
    params.lowHeight = lh;
    return runScalar(kUpsample, fullGroups, 1, 64, 1, &low, &buffer, command);
}

bool GpuTexture::Impl::buildGrainField(unsigned int seed, int index, int radius, Res& target,
                                       id<MTLCommandBuffer> command) {
    resetParams();
    params.seed = seed;
    params.fieldIndex = index;
    const unsigned int fullGroups =
        static_cast<unsigned int>((params.count + 63) / 64);
    if (!runScalar(kGrainField, fullGroups, 1, 64, 1, nullptr, &target, command)) return false;

    if (radius < 1) return true;
    params.blurWidth = width;
    params.blurHeight = height;
    params.radius = radius;
    for (int pass = 0; pass < 2; ++pass) {
        if (!runScalar(kBlurH, 1, static_cast<unsigned int>((height + 63) / 64), 1, 64, &target,
                       &full1, command)) {
            return false;
        }
        if (!runScalar(kBlurV, static_cast<unsigned int>((width + 63) / 64), 1, 64, 1, &full1,
                       &target, command)) {
            return false;
        }
    }
    return true;
}

bool GpuTexture::Impl::runChain(int w, int h, const TextureParams& tex, int frame, float* out,
                                std::string& error, Res* start) {
    const std::size_t count = static_cast<std::size_t>(w) * h;
    const auto started = std::chrono::steady_clock::now();
    const unsigned int fullGroups = static_cast<unsigned int>((count + 63) / 64);
    Res* current = start != nullptr ? start : &rgbA;
    Res* other = (current == &rgbA) ? &rgbB : &rgbA;
    // The fused path starts on a buffer owned by the film backend: the first
    // pass may read it, but every later pass must ping-pong between our own
    // buffers so the film result is never overwritten.
    bool borrowed = start != nullptr;
    const auto swap = [&]() {
        std::swap(current, other);
        if (borrowed) {
            borrowed = false;
            other = (current == &rgbA) ? &rgbB : &rgbA;
        }
    };

    id<MTLCommandBuffer> command = [queue commandBuffer];
    if (command == nil) {
        error = "could not create a Metal command buffer";
        return false;
    }

    if (tex.halation.amount > 0.0f) {
        const float threshold = std::min(0.99f, std::max(0.0f, tex.halation.threshold));
        resetParams();
        params.threshold = threshold;
        if (!runSource(kHalationSource, fullGroups, current, &full0, command)) return false;

        const int radius =
            std::max(1, static_cast<int>(std::lround(tex.halation.radius * w)));
        if (tex.spread.model == SpreadModel::Box) {
            if (!blurWide(full0, radius, 2, command)) return false;
        } else {
            const float sigma = spreadSigmaPixels(tex.halation.spreadMicrons,
                                                  tex.spread.frameWidthMm, w);
            if (!spreadWide(full0, SpreadKind::Halation, sigma, command)) return false;
        }

        resetParams();
        params.amount = tex.halation.amount * 0.55f;
        params.tint[0] = 1.0f;
        params.tint[1] = 0.42f;
        params.tint[2] = 0.18f;
        if (!runRgb(kHalationApply, fullGroups, current, &full0, other, command)) return false;
        swap();
    }

    if (tex.glare.amount > 0.0f) {
        resetParams();
        if (!runSource(kLuma, fullGroups, current, &full0, command)) return false;
        const int radius = std::max(1, static_cast<int>(std::lround(tex.glare.radius * w)));
        if (tex.spread.model == SpreadModel::Box) {
            if (!blurWide(full0, radius, 4, command)) return false;
        } else {
            const float sigma = spreadSigmaPixels(tex.glare.spreadMicrons,
                                                  tex.spread.frameWidthMm, w);
            if (!spreadWide(full0, SpreadKind::Glare, sigma, command)) return false;
        }

        resetParams();
        params.amount = std::min(1.0f, tex.glare.amount);
        params.tint[0] = 1.0f;
        params.tint[1] = 0.96f;
        params.tint[2] = 0.92f;
        if (!runRgb(kGlareApply, fullGroups, current, &full0, other, command)) return false;
        swap();
    }

    if (tex.diffusion.amount > 0.0f) {
        resetParams();
        if (!runSource(kLuma, fullGroups, current, &full0, command)) return false;
        const int radius = std::max(1, static_cast<int>(std::lround(tex.diffusion.radius * w)));
        if (tex.spread.model == SpreadModel::Box) {
            if (!blurWide(full0, radius, 2, command)) return false;
        } else {
            const float sigma = spreadSigmaPixels(tex.diffusion.spreadMicrons,
                                                  tex.spread.frameWidthMm, w);
            if (!spreadWide(full0, SpreadKind::Diffusion, sigma, command)) return false;
        }

        resetParams();
        params.amount = std::min(1.0f, tex.diffusion.amount);
        if (!runRgb(kDiffusionApply, fullGroups, current, &full0, other, command)) return false;
        swap();
    }

    if (tex.grain.amount > 0.0f) {
        const int rate = std::max(1, tex.grain.rate);
        const int frameIndex = std::max(0, frame);
        const unsigned int fieldIndex = static_cast<unsigned int>(frameIndex / rate);
        const float phase = rate > 1 ? static_cast<float>(frameIndex % rate) /
                                           static_cast<float>(rate)
                                     : 0.0f;
        const float norm =
            1.0f / std::sqrt((1.0f - phase) * (1.0f - phase) + phase * phase);
        const int radius = std::max(0, static_cast<int>(std::lround(tex.grain.size)));

        if (!buildGrainField(tex.grain.seed, static_cast<int>(fieldIndex), radius, grain0,
                             command)) {
            return false;
        }
        if (rate > 1 && !buildGrainField(tex.grain.seed, static_cast<int>(fieldIndex) + 1,
                                         radius, grain1, command)) {
            return false;
        }

        resetParams();
        params.phase = phase;
        params.norm = norm;
        params.scale = tex.grain.amount * 0.14f;
        if (!runGrain(kGrainApply, fullGroups, current, &grain0,
                      rate > 1 ? &grain1 : &grain0, other, command)) {
            return false;
        }
        swap();
    }

    [command commit];
    [command waitUntilCompleted];
    if (command.status == MTLCommandBufferStatusError) {
        error = "the Metal texture dispatch failed";
        return false;
    }

    const float* gpu = static_cast<const float*>(current->buffer.contents);
    for (std::size_t i = 0; i < count; ++i) {
        out[i * 3 + 0] = gpu[i * 4 + 0];
        out[i * 3 + 1] = gpu[i * 4 + 1];
        out[i * 3 + 2] = gpu[i * 4 + 2];
    }
    lastMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return true;
}

bool GpuTexture::available(std::string& error) {
    Impl probe;
    return probe.initialize(error);
}

bool GpuTexture::prepare(std::string& error) { return impl_->initialize(error); }

bool GpuTexture::attach(void* device, std::string& error) {
    if (device == nullptr) {
        error = "cannot attach the texture backend to a null device";
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

bool GpuTexture::process(const float* rgb, int width, int height, const TextureParams& params,
                         int frame, float* out, std::string& error) {
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
    std::memcpy(g.rgbA.buffer.contents, g.packed.data(), count * 16);
    return g.runChain(width, height, params, frame, out, error);
}

bool GpuTexture::processBuffer(void* input, int width, int height, const TextureParams& params,
                               int frame, float* out, std::string& error) {
    if (input == nullptr) {
        error = "fused texture input is null";
        return false;
    }
    if (width <= 0 || height <= 0) {
        error = "bad frame size for the GPU texture backend";
        return false;
    }
    const bool active = params.halation.amount > 0.0f || params.glare.amount > 0.0f ||
                        params.diffusion.amount > 0.0f || params.grain.amount > 0.0f;
    if (!active) {
        error = "the fused path needs at least one texture effect";
        return false;
    }
    if (!impl_->initialize(error)) return false;
    if (!impl_->ensureCapacity(width, height, error)) return false;

    Impl& g = *impl_;
    // The film pass produced an MTLBuffer of float4 samples; borrow it as the
    // first input instead of copying tens of megabytes through the CPU.
    g.external.buffer = (__bridge id<MTLBuffer>)input;
    g.external.elements = static_cast<std::size_t>(width) * height;
    g.externalBuffer = input;
    return g.runChain(width, height, params, frame, out, error, &g.external);
}

GpuTexture::~GpuTexture() {
    if (!impl_) return;
    // Drop the big buffers now; the pipelines are released explicitly too
    // (array elements cannot be iterated by non-const reference under ARC).
    impl_->releaseResources();
    for (int i = 0; i < Impl::kShaderCount; ++i) impl_->pipelines[i] = nil;
    impl_->dummyRgb = nil;
    impl_->dummyScalar = nil;
    impl_->queue = nil;
    impl_->device = nil;
}

}  // namespace sr

#else  // !__APPLE__

namespace sr {
struct GpuTextureMetalPlaceholder {};
}  // namespace sr

#endif
