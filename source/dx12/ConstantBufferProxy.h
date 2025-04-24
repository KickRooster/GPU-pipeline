#pragma once
#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

struct ConstantBufferProxy
{
    ComPtr<ID3D12Resource> UploadBuffer;
    unsigned char* MappedData = nullptr;
    unsigned int ElementByteSize = 0;
};