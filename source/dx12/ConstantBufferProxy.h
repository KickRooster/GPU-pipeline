#pragma once

#include "../misc/Base.h"
#include <d3d12.h>
#include <wrl/client.h>

struct ConstantBufferProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer[FrameNumInFlight];
    unsigned char* MappedData[FrameNumInFlight] = {nullptr, nullptr};
    unsigned int ElementByteSize = 0;
};