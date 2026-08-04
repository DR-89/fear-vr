#include "texture_renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <d3dcompiler.h>

namespace fearvr {
namespace {

using Microsoft::WRL::ComPtr;

void CheckHr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) +
                                 " failed with HRESULT=" +
                                 std::to_string(
                                     static_cast<std::uint32_t>(result)));
    }
}

ComPtr<ID3DBlob> CompileShader(const char* source, const char* entry,
                               const char* target) {
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(
        source, std::char_traits<char>::length(source), "fearvr-ipc",
        nullptr, nullptr, entry, target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        shader.ReleaseAndGetAddressOf(),
        errors.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        const std::string detail =
            errors ? std::string(
                         static_cast<const char*>(errors->GetBufferPointer()),
                         errors->GetBufferSize())
                   : "no compiler diagnostic";
        throw std::runtime_error("D3DCompile failed: " + detail);
    }
    return shader;
}

constexpr char kShader[] = R"(
Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VsMain(uint vertexId : SV_VertexID) {
    float2 corner = float2((vertexId << 1) & 2, vertexId & 2);
    VertexOutput output;
    output.position =
        float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = corner;
    return output;
}

float4 PsMain(VertexOutput input) : SV_Target {
    // The game source is often smaller than the Quest/VDXR swapchain. A
    // small unsharp pass restores edge contrast after the linear upscale,
    // improving perceived detail without inventing a heavier render pass.
    uint sourceWidth = 1;
    uint sourceHeight = 1;
    sourceTexture.GetDimensions(sourceWidth, sourceHeight);
    float2 texel = 1.0 / float2(sourceWidth, sourceHeight);
    float4 center = sourceTexture.Sample(sourceSampler, input.uv);
    float4 blur = (
        sourceTexture.Sample(sourceSampler, input.uv + float2(texel.x, 0.0)) +
        sourceTexture.Sample(sourceSampler, input.uv - float2(texel.x, 0.0)) +
        sourceTexture.Sample(sourceSampler, input.uv + float2(0.0, texel.y)) +
        sourceTexture.Sample(sourceSampler, input.uv - float2(0.0, texel.y))) *
        0.25;
    return saturate(center + (center - blur) * 0.30);
}
)";

} // namespace

void TextureRenderer::Initialize(ID3D11Device* device) {
    const ComPtr<ID3DBlob> vertex =
        CompileShader(kShader, "VsMain", "vs_4_0");
    CheckHr(device->CreateVertexShader(
                vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr,
                vertexShader_.ReleaseAndGetAddressOf()),
            "CreateVertexShader");

    const ComPtr<ID3DBlob> pixel =
        CompileShader(kShader, "PsMain", "ps_4_0");
    CheckHr(device->CreatePixelShader(
                pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr,
                pixelShader_.ReleaseAndGetAddressOf()),
            "CreatePixelShader");

    D3D11_SAMPLER_DESC samplerDescription{};
    samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    CheckHr(device->CreateSamplerState(
                &samplerDescription, sampler_.ReleaseAndGetAddressOf()),
            "CreateSamplerState");
}

void TextureRenderer::Draw(ID3D11DeviceContext* context,
                           ID3D11RenderTargetView* renderTarget,
                           ID3D11ShaderResourceView* source, float width,
                           float height) {
    D3D11_VIEWPORT viewport{};
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0F;
    viewport.MaxDepth = 1.0F;
    DrawViewport(context, renderTarget, source, viewport);
}

void TextureRenderer::DrawAspectFit(
    ID3D11DeviceContext* context, ID3D11RenderTargetView* renderTarget,
    ID3D11ShaderResourceView* source, float targetWidth, float targetHeight,
    float sourceWidth, float sourceHeight) {
    constexpr std::array<float, 4> black{0.0F, 0.0F, 0.0F, 1.0F};
    context->ClearRenderTargetView(renderTarget, black.data());

    if (targetWidth <= 0.0F || targetHeight <= 0.0F ||
        sourceWidth <= 0.0F || sourceHeight <= 0.0F) {
        return;
    }
    const float scale = (std::min)(targetWidth / sourceWidth,
                                   targetHeight / sourceHeight);
    D3D11_VIEWPORT viewport{};
    viewport.Width = sourceWidth * scale;
    viewport.Height = sourceHeight * scale;
    viewport.TopLeftX = (targetWidth - viewport.Width) * 0.5F;
    viewport.TopLeftY = (targetHeight - viewport.Height) * 0.5F;
    viewport.MinDepth = 0.0F;
    viewport.MaxDepth = 1.0F;
    DrawViewport(context, renderTarget, source, viewport);
}

void TextureRenderer::DrawViewport(
    ID3D11DeviceContext* context, ID3D11RenderTargetView* renderTarget,
    ID3D11ShaderResourceView* source, const D3D11_VIEWPORT& viewport) {
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &renderTarget, nullptr);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, &source);
    ID3D11SamplerState* sampler = sampler_.Get();
    context->PSSetSamplers(0, 1, &sampler);
    context->Draw(3, 0);

    ID3D11ShaderResourceView* nullView = nullptr;
    context->PSSetShaderResources(0, 1, &nullView);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

} // namespace fearvr
