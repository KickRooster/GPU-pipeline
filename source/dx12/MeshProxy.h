#pragma once
#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

struct MeshProxy
{
    ComPtr<ID3D12Resource> VertexBuffer;
    ComPtr<ID3D12Resource> VertexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

    ComPtr<ID3D12Resource> IndexBuffer;
    ComPtr<ID3D12Resource> IndexBufferUpload;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;
};