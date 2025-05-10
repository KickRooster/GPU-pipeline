#pragma once
#include <d3d12.h>
#include <wrl/client.h>

struct ConstantBufferProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer;
    unsigned char* MappedData = nullptr;
    unsigned int ElementByteSize = 0;
};