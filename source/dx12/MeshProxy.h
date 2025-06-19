#pragma once
#include <d3d12.h>
#include <wrl/client.h>

struct MeshletDataProxy
{
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletsBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletVerticesBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletTrianglesBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletBoundsBuffer;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletsBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletVerticesBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletTrianglesBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> MeshletBoundsBufferUpload;
};