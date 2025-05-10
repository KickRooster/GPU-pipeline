#pragma once
#include <d3d12.h>
#include <wrl/client.h>

struct MeshProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUpload;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;
};