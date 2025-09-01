#pragma once
#include <d3d12.h>
#include <wrl/client.h>

struct TextureProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer;
    DXGI_FORMAT Format;
    unsigned int DescriptorIndex;
    unsigned int Width;
    unsigned int Height;
};

struct CubemapTextureProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer;
    DXGI_FORMAT Format;
    unsigned int DescriptorIndex;
    unsigned int Size;
    int MipLevels;
};