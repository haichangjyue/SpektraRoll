// SPDX-License-Identifier: GPL-3.0-or-later
// Throwaway D3D11 compute probe for the Arc A750: how long does one 81-sample
// spectral scan pass take on 2.6 MP? This decides whether porting the full
// FilmModel chain to the GPU is worth it. Not part of the product build.

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

template <class T>
void release(T*& object) {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

const char* kShader = R"HLSL(
cbuffer Params : register(b0) {
    float4 density[81];
    float4 cmfs[81];
    float4 illum[81];
    uint count;
    float norm;
    float pad0;
    float pad1;
};

StructuredBuffer<float4> input : register(t0);
RWStructuredBuffer<float4> output : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= count) return;
    float3 cmy = input[id.x].xyz;
    float3 xyz = float3(0, 0, 0);
    [loop] for (int l = 0; l < 81; ++l) {
        float d = dot(cmy, density[l].xyz);
        float light = exp2(-d * 3.321928094887362) * illum[l].x;
        xyz += light * cmfs[l].xyz;
    }
    output[id.x] = float4(xyz / norm, 1.0);
}
)HLSL";

struct alignas(16) Params {
    float density[81][4];
    float cmfs[81][4];
    float illum[81][4];
    unsigned int count;
    float norm;
    float pad0;
    float pad1;
};

}  // namespace

int main() {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    const HRESULT created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                              nullptr, 0, D3D11_SDK_VERSION, &device, &level,
                                              &context);
    if (FAILED(created) || device == nullptr || context == nullptr) {
        std::printf("D3D11CreateDevice failed: 0x%08lx\n", static_cast<unsigned long>(created));
        return 1;
    }
    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    DXGI_ADAPTER_DESC desc{};
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice),
                                         reinterpret_cast<void**>(&dxgiDevice)))) {
        dxgiDevice->GetAdapter(&adapter);
        if (adapter != nullptr) adapter->GetDesc(&desc);
    }
    std::wprintf(L"adapter: %ls\n", desc.Description);
    std::printf("feature level: 0x%x\n", static_cast<unsigned>(level));

    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT compiled =
        D3DCompile(kShader, std::strlen(kShader), "gpu_probe.hlsl", nullptr, nullptr, "CSMain",
                   "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(compiled)) {
        if (errors != nullptr) {
            std::printf("%s\n", static_cast<const char*>(errors->GetBufferPointer()));
            release(errors);
        }
        return 1;
    }

    ID3D11ComputeShader* shader = nullptr;
    device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                &shader);
    release(code);
    release(errors);

    constexpr size_t kPixels = 2160u * 1214u;
    std::vector<float> input(kPixels * 4);
    for (size_t i = 0; i < kPixels; ++i) {
        input[i * 4 + 0] = 0.2f + 0.4f * static_cast<float>((i * 37) % 100) / 100.0f;
        input[i * 4 + 1] = 0.2f + 0.4f * static_cast<float>((i * 53) % 100) / 100.0f;
        input[i * 4 + 2] = 0.2f + 0.4f * static_cast<float>((i * 71) % 100) / 100.0f;
        input[i * 4 + 3] = 1.0f;
    }

    D3D11_BUFFER_DESC inputDesc{};
    inputDesc.ByteWidth = static_cast<UINT>(input.size() * sizeof(float));
    inputDesc.Usage = D3D11_USAGE_DEFAULT;
    inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    inputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    inputDesc.StructureByteStride = 16;
    D3D11_SUBRESOURCE_DATA initial{};
    initial.pSysMem = input.data();
    ID3D11Buffer* inputBuffer = nullptr;
    if (FAILED(device->CreateBuffer(&inputDesc, &initial, &inputBuffer))) return 1;

    D3D11_BUFFER_DESC outputDesc = inputDesc;
    outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    ID3D11Buffer* outputBuffer = nullptr;
    if (FAILED(device->CreateBuffer(&outputDesc, nullptr, &outputBuffer))) return 1;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = static_cast<UINT>(kPixels);
    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(device->CreateShaderResourceView(inputBuffer, &srvDesc, &srv))) return 1;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = static_cast<UINT>(kPixels);
    ID3D11UnorderedAccessView* uav = nullptr;
    if (FAILED(device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &uav))) return 1;

    Params params{};
    for (int l = 0; l < 81; ++l) {
        params.density[l][0] = 0.20f + 0.012f * l;
        params.density[l][1] = 0.18f + 0.010f * l;
        params.density[l][2] = 0.22f + 0.011f * l;
        params.cmfs[l][0] = 1.0f + 0.3f * std::sin(l * 0.2f);
        params.cmfs[l][1] = 1.1f + 0.2f * std::cos(l * 0.13f);
        params.cmfs[l][2] = 0.8f + 0.4f * std::sin(l * 0.31f);
        params.illum[l][0] = 1.0f;
    }
    params.count = static_cast<unsigned int>(kPixels);
    params.norm = 1.0f;

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(Params);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* cbuffer = nullptr;
    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &cbuffer))) return 1;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    context->Map(cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    std::memcpy(mapped.pData, &params, sizeof(params));
    context->Unmap(cbuffer, 0);

    D3D11_QUERY_DESC queryDesc{};
    queryDesc.Query = D3D11_QUERY_EVENT;
    ID3D11Query* stop = nullptr;
    device->CreateQuery(&queryDesc, &stop);

    context->CSSetShader(shader, nullptr, 0);
    context->CSSetConstantBuffers(0, 1, &cbuffer);
    context->CSSetShaderResources(0, 1, &srv);
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    const UINT groups = static_cast<UINT>((kPixels + 63) / 64);

    // Warm up.
    context->Dispatch(groups, 1, 1);
    context->End(stop);
    context->Flush();
    BOOL done = FALSE;
    while (context->GetData(stop, &done, sizeof(done), 0) != S_OK || done == FALSE) {
    }

    // Wall clock around submit + GPU completion. Event queries only signal
    // completion, so this is the honest end-to-end number.
    constexpr int kRuns = 5;
    double gpuMs = 0.0;
    for (int run = 0; run < kRuns; ++run) {
        const auto t0 = std::chrono::steady_clock::now();
        context->Dispatch(groups, 1, 1);
        context->End(stop);
        context->Flush();
        BOOL done = FALSE;
        while (context->GetData(stop, &done, sizeof(done), 0) != S_OK || done == FALSE) {
        }
        const auto t1 = std::chrono::steady_clock::now();
        gpuMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    // Read back one pixel to prove the dispatch wrote something.
    D3D11_BUFFER_DESC stagingDesc = outputDesc;
    stagingDesc.BindFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Buffer* staging = nullptr;
    device->CreateBuffer(&stagingDesc, nullptr, &staging);
    context->CopyResource(staging, outputBuffer);
    context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    const float* out = static_cast<const float*>(mapped.pData);
    std::printf("pixel0: %.6f %.6f %.6f\n", out[0], out[1], out[2]);
    context->Unmap(staging, 0);

    std::printf("pixels: %zu, groups: %u, runs: %d\n", kPixels, groups, kRuns);
    std::printf("gpu query readback avg: %.3f ms\n", gpuMs / kRuns);

    release(staging);
    release(stop);
    release(cbuffer);
    release(uav);
    release(srv);
    release(outputBuffer);
    release(inputBuffer);
    release(shader);
    release(adapter);
    release(dxgiDevice);
    release(context);
    release(device);
    return 0;
}
