cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
    float    gScreenWidth;
    float    gScreenHeight;
    float    gRecipTanHalfFovy;     // 1.0f / tanf(fovy * 0.5f)
    float    gLODErrorThreshold;    // pixels
    float    gNearPlane;
};

// Root constant for total cluster count (GPU-Driven rendering)
cbuffer cbClusterCount : register(b1)
{
    uint gNaniteClusterCount;  // Total cluster count across ALL meshes
};

#define AS_CLUSTERS_PER_GROUP 32    // Each AS group dispatches 32 mesh shaders

// Nanite structures
struct NaniteVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float2 UV0;
};

struct GPUCluster
{
    uint PrimitiveId;            // Index to ScenePrimitiveBuffer (GPU Scene)
    uint IndexCount;
    uint UniqueVerticesOffset;
    uint UniqueVerticesCount;
    uint LocalIndicesOffset;
    float3 BoundCenter;
    float BoundRadius;
    int Refined;
    int GroupId;
    uint Padding;                // 16-byte alignment
};

struct GPUGroupBound
{
    float3 Center;
    float Radius;
    float Error;
};

// Nanite buffers: Vertex -> UniqueVertices -> LocalIndices -> Cluster -> GroupBounds
// GPU-Driven mesh instance data (matches C++ GPUMeshInstance)
struct GPUMeshInstance
{
    uint UniqueVerticesOffset;
    uint UniqueVerticesCount;
    uint LocalIndicesOffset;
    uint LocalIndicesCount;
    uint ClusterOffset;
    uint ClusterCount;
    uint GroupBoundsOffset;
    uint GroupBoundsCount;
    uint4 Padding;  // Padding for 16-byte alignment
};

// Global merged Nanite buffers (all meshes combined, Root Descriptors t20-t24)
StructuredBuffer<NaniteVertex> NaniteVertices : register(t20);
StructuredBuffer<uint> NaniteUniqueVertices : register(t21);
ByteAddressBuffer NaniteLocalIndices : register(t22);
StructuredBuffer<GPUCluster> NaniteClusters : register(t23);
StructuredBuffer<GPUGroupBound> NaniteGroupBounds : register(t24);

// Mesh instance buffer (Root Descriptor, t25)
StructuredBuffer<GPUMeshInstance> MeshInstanceBuffer : register(t25);

// GPU Scene: Primitive transform data (Root Descriptor, t26, UE5-style)
struct FPrimitiveSceneData
{
    float4x4 LocalToWorld;
    float4x4 WorldInvTranspose;
    uint4    PBRTextureIndices[4];  // Albedo, Normal, Metallic, Roughness (16 bytes per element)
};
StructuredBuffer<FPrimitiveSceneData> ScenePrimitives : register(t26);

struct Payload
{
    uint ASGroupID;   // AS group ID for cluster index calculation
    uint PrimitiveId; // Primitive ID for transform lookup in GPU Scene
};

groupshared Payload s_Payload;

[numthreads(1, 1, 1)]  // One thread per AS group
void main(
    uint gid : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{
    s_Payload.ASGroupID = gid;

    // GPU-Driven: Each AS group processes clusters from the global cluster buffer
    // gNaniteClusterCount is the TOTAL cluster count across all meshes
    uint startCluster = gid * AS_CLUSTERS_PER_GROUP;
    uint clustersToDispatch = min(AS_CLUSTERS_PER_GROUP, max(0, int(gNaniteClusterCount) - int(startCluster)));

    // GPU Scene: Read PrimitiveId from the first cluster in this group
    // All clusters in a group typically belong to the same primitive
    if (clustersToDispatch > 0 && startCluster < gNaniteClusterCount)
    {
        GPUCluster firstCluster = NaniteClusters[startCluster];
        s_Payload.PrimitiveId = firstCluster.PrimitiveId;
    }
    else
    {
        s_Payload.PrimitiveId = 0;
    }

    // AS shader directly accesses global NaniteClusters buffer
    // MS shader will use the global indices stored in each cluster
    DispatchMesh(clustersToDispatch, 1, 1, s_Payload);
}
