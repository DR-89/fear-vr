#include "startup_splash.h"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <objbase.h>
#include <wincodec.h>

namespace fearvr {
namespace {

using Microsoft::WRL::ComPtr;

std::string HResultText(const char* operation, HRESULT result) {
    std::ostringstream message;
    message << operation << " failed with HRESULT=0x" << std::hex
            << static_cast<std::uint32_t>(result);
    return message.str();
}

void CheckHr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(HResultText(operation, result));
    }
}

class ComApartment {
public:
    ComApartment() {
        result_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        usable_ = SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
        ownsInitialization_ = SUCCEEDED(result_);
    }

    ~ComApartment() {
        if (ownsInitialization_) {
            CoUninitialize();
        }
    }

    bool IsUsable() const noexcept {
        return usable_;
    }
    HRESULT Result() const noexcept {
        return result_;
    }

private:
    HRESULT result_{E_FAIL};
    bool usable_{false};
    bool ownsInitialization_{false};
};

} // namespace

bool StartupSplash::Load(ID3D11Device* device,
                         const std::filesystem::path& path,
                         std::string& error) noexcept {
    view_.Reset();
    width_ = 0;
    height_ = 0;
    error.clear();

    try {
        if (device == nullptr || path.empty()) {
            throw std::runtime_error(
                "startup image needs a D3D11 device and a file path");
        }

        ComApartment apartment;
        if (!apartment.IsUsable()) {
            throw std::runtime_error(
                HResultText("CoInitializeEx", apartment.Result()));
        }

        ComPtr<IWICImagingFactory> factory;
        CheckHr(CoCreateInstance(
                    CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())),
                "CoCreateInstance(WICImagingFactory)");

        ComPtr<IWICBitmapDecoder> decoder;
        CheckHr(factory->CreateDecoderFromFilename(
                    path.c_str(), nullptr, GENERIC_READ,
                    WICDecodeMetadataCacheOnLoad,
                    decoder.ReleaseAndGetAddressOf()),
                "IWICImagingFactory::CreateDecoderFromFilename");

        ComPtr<IWICBitmapFrameDecode> frame;
        CheckHr(decoder->GetFrame(0, frame.ReleaseAndGetAddressOf()),
                "IWICBitmapDecoder::GetFrame");

        UINT width = 0;
        UINT height = 0;
        CheckHr(frame->GetSize(&width, &height),
                "IWICBitmapFrameDecode::GetSize");
        if (width == 0 || height == 0 || width > 16384 ||
            height > 16384) {
            throw std::runtime_error(
                "startup image dimensions are invalid or too large");
        }

        ComPtr<IWICFormatConverter> converter;
        CheckHr(factory->CreateFormatConverter(
                    converter.ReleaseAndGetAddressOf()),
                "IWICImagingFactory::CreateFormatConverter");
        CheckHr(converter->Initialize(
                    frame.Get(), GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone, nullptr, 0.0,
                    WICBitmapPaletteTypeCustom),
                "IWICFormatConverter::Initialize");

        const std::uint64_t rowBytes =
            static_cast<std::uint64_t>(width) * 4U;
        const std::uint64_t imageBytes =
            rowBytes * static_cast<std::uint64_t>(height);
        if (rowBytes > (std::numeric_limits<UINT>::max)() ||
            imageBytes > (std::numeric_limits<UINT>::max)()) {
            throw std::runtime_error("startup image exceeds WIC limits");
        }

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(imageBytes));
        CheckHr(converter->CopyPixels(
                    nullptr, static_cast<UINT>(rowBytes),
                    static_cast<UINT>(imageBytes), pixels.data()),
                "IWICBitmapSource::CopyPixels");

        D3D11_TEXTURE2D_DESC textureDescription{};
        textureDescription.Width = width;
        textureDescription.Height = height;
        textureDescription.MipLevels = 1;
        textureDescription.ArraySize = 1;
        textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        textureDescription.SampleDesc.Count = 1;
        textureDescription.Usage = D3D11_USAGE_IMMUTABLE;
        textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initialData{};
        initialData.pSysMem = pixels.data();
        initialData.SysMemPitch = static_cast<UINT>(rowBytes);

        ComPtr<ID3D11Texture2D> texture;
        CheckHr(device->CreateTexture2D(
                    &textureDescription, &initialData,
                    texture.ReleaseAndGetAddressOf()),
                "ID3D11Device::CreateTexture2D(startup image)");
        CheckHr(device->CreateShaderResourceView(
                    texture.Get(), nullptr, view_.ReleaseAndGetAddressOf()),
                "ID3D11Device::CreateShaderResourceView(startup image)");

        width_ = width;
        height_ = height;
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        view_.Reset();
        width_ = 0;
        height_ = 0;
        return false;
    } catch (...) {
        error = "unknown startup image load failure";
        view_.Reset();
        width_ = 0;
        height_ = 0;
        return false;
    }
}

} // namespace fearvr
