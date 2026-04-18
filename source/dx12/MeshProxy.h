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
    unsigned int IndexCount;            // Triangle count x 3
    unsigned int UniqueVerticesOffset;  // Offset in UniqueVerticesBuffer
    unsigned int UniqueVerticesCount;   // Unique vertex count (<=64 for Mesh Shader limit)
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

// Terrain patch data (matches HLSL TerrainPatchData)
struct TerrainPatchData
{
    float WorldOffsetX;
    float WorldOffsetZ;
    float PatchSize;
    unsigned int PatchIndex;
    unsigned int LodLevel;
    unsigned int NeighborLodPacked;     // [7:0]=Top [15:8]=Bottom [23:16]=Left [31:24]=Right, 0xFF=no snap
};

// Terrain per-patch AABB (matches HLSL TerrainPatchBound)
struct TerrainPatchBound
{
    float Center[3];
    float HalfExtent[3];
};

// Terrain constant buffer (matches HLSL cbTerrainParams at b1)
struct TerrainParams
{
    unsigned int ActivePatchCount;
    unsigned int ClusterBase;           // Offset to separate terrain from Nanite in VB encoding
    float PatchSize;
    float HeightScale;
    float WorldSize;
    unsigned int TotalPatchCount;
    unsigned int EdgeStitchingEnabled;
    float DebugScale;
};

// Material resolve constant buffer (matches HLSL cbTerrainInfo at b2)
// Layout designed for HLSL cbuffer uint4 packing
struct TerrainResolveConstants
{
    unsigned int NaniteClusterCount;    // float4.x
    unsigned int HeightmapIndex;        // float4.y
    float WorldSize;                    // float4.z
    float HeightScale;                  // float4.w
    unsigned int SplatmapAndLayerInfo[4]; // .xyz=splatmap indices, .w=layerCount
    unsigned int AlbedoIndices[12];     // uint4[3]
    unsigned int NormalIndices[12];     // uint4[3]
    unsigned int RoughnessIndices[12];  // uint4[3]
    float DebugScale;                   // float4.x
    unsigned int DebugLODColors;        // float4.y
    unsigned int Pad[2];                // float4.zw
};

// Terrain GPU resource proxy
struct TerrainProxy
{
    // GPU Default Heap buffers
    Microsoft::WRL::ComPtr<ID3D12Resource> PatchBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> BoundBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> Heightmap;
    Microsoft::WRL::ComPtr<ID3D12Resource> ActiveIndicesBuffer;

    // Per-frame upload buffers (triple buffered)
    Microsoft::WRL::ComPtr<ID3D12Resource> ActiveIndicesUpload[FrameNumInFlight];
    Microsoft::WRL::ComPtr<ID3D12Resource> NeighborLodUpload[FrameNumInFlight]; // Indexed by patch index, CPU-writable GPU-readable
};
