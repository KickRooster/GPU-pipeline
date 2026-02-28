#pragma once

#include "../misc/Base.h"
#include <d3d12.h>
#include <wrl/client.h>

struct NaniteClusterProxy
{
    // GPU Default Heap buffers (shader read-only)
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;           // Vertex data (shared by all clusters)
    Microsoft::WRL::ComPtr<ID3D12Resource> UniqueVerticesBuffer;   // Unique vertex global indices (per-cluster deduplicated)
    Microsoft::WRL::ComPtr<ID3D12Resource> LocalIndicesBuffer;     // Local indices (0-255, references UniqueVertices)
    Microsoft::WRL::ComPtr<ID3D12Resource> ClusterBuffer;          // Cluster metadata (GPUCluster structure array)
    Microsoft::WRL::ComPtr<ID3D12Resource> GroupBoundsBuffer;      // Group bounds for cluster selection (GPUGroupBound array)

    // Upload Heap buffers (CPU->GPU transfer)
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> UniqueVerticesBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> LocalIndicesBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> ClusterBufferUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> GroupBoundsBufferUpload;
};

struct GPUCluster
{
    unsigned int PrimitiveId;           // Index to ScenePrimitiveBuffer (GPU Scene)
    unsigned int IndexCount;            // Triangle count × 3
    unsigned int UniqueVerticesOffset;  // Offset in UniqueVerticesBuffer
    unsigned int UniqueVerticesCount;   // Unique vertex count (≤64 for Mesh Shader limit, limited by outside logic)
    unsigned int LocalIndicesOffset;    // Offset in LocalIndicesBuffer
    float BoundCenter[3];               // Bounding sphere center (frustum culling)
    float BoundRadius;                  // Bounding sphere radius (frustum culling)
    int Refined;                        // Index to more detailed group (-1 = leaf, for LOD selection)
    int GroupId;                        // Index to group this cluster belongs to
    unsigned int TriangleMaterialIDsOffset; // Offset in GlobalTriangleMaterialIDsBuffer
};

struct GPUGroupBound
{
    float Center[3];    // Group bounds center
    float Radius;       // Group bounds radius
    float Error;        // Simplification error (monotonic: parent > child)
};

// GPU material table entry (indexed by global material ID)
struct GPUMaterial
{
    unsigned int AlbedoTextureIndex;
    unsigned int NormalTextureIndex;
    unsigned int MetallicTextureIndex;
    unsigned int RoughnessTextureIndex;
};

// GPU Scene: Primitive transform data (UE5-style naming)
struct FPrimitiveSceneData
{
    DirectX::XMFLOAT4X4 LocalToWorld;       // Object-to-world transform
    DirectX::XMFLOAT4X4 WorldInvTranspose;  // For normal transformation
};