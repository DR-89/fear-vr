#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace fearvr {

class TextureRenderer {
public:
    void Initialize(ID3D11Device* device);
    void Draw(ID3D11DeviceContext* context,
              ID3D11RenderTargetView* renderTarget,
              ID3D11ShaderResourceView* source, float width,
              float height);
    void DrawAspectFit(ID3D11DeviceContext* context,
                       ID3D11RenderTargetView* renderTarget,
                       ID3D11ShaderResourceView* source, float targetWidth,
                       float targetHeight, float sourceWidth,
                       float sourceHeight);

private:
    void DrawViewport(ID3D11DeviceContext* context,
                      ID3D11RenderTargetView* renderTarget,
                      ID3D11ShaderResourceView* source,
                      const D3D11_VIEWPORT& viewport);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
};

} // namespace fearvr
