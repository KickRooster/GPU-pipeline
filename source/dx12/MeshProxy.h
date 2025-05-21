#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "ConstantBufferProxy.h"

struct MeshProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUpload;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;
};

struct MeshletDataProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletsBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletVerticesBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletTrianglesBuffer;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletsBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletVerticesBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletTrianglesBufferUpload;
    
    // 新增：MeshInfo常量缓冲区代理
    std::unique_ptr<ConstantBufferProxy> MeshInfoCBProxy;
};