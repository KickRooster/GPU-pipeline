#pragma once
#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

struct ShapeProxy
{
    ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    
    ComPtr<ID3DBlob> IndexBufferCPU  = nullptr;
    ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = VertexByteStride;
        vbv.SizeInBytes = VertexBufferByteSize;

        return vbv;
    }
    
    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;

        return ibv;
    }
  
    // ComPtr<ID3D12Resource> VertexBuffer;
    // ComPtr<ID3D12Resource> VertexBufferUpload;
    // D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
};