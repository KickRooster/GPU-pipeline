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
    uint VisibleClusterIndices[AS_CLUSTERS_PER_GROUP];
};

groupshared Payload s_Payload;

bool IsClusterVisible(float3 worldCenter, float worldRadius, float4 planes[6])
{
    for (int i = 0; i < 6; i++)
    {
        float distance = dot(planes[i].xyz, worldCenter) + planes[i].w;
        if (distance < -worldRadius)
            return false;
    }
    return true;
}

[numthreads(1, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{
    uint startCluster = gid * AS_CLUSTERS_PER_GROUP;
    uint endCluster = min(startCluster + AS_CLUSTERS_PER_GROUP, gNaniteClusterCount);
    uint visibleCount = 0;

    for (uint i = 0; i < (endCluster - startCluster); i++)
    {
        uint clusterIdx = startCluster + i;
        GPUCluster cluster = NaniteClusters[clusterIdx];
        FPrimitiveSceneData primitiveData = ScenePrimitives[cluster.PrimitiveId];

        float4 localCenter = float4(cluster.BoundCenter, 1.0);
        float3 worldCenter = mul(localCenter, primitiveData.LocalToWorld).xyz;
        float maxScale = max(max(length(primitiveData.LocalToWorld[0].xyz),
                                 length(primitiveData.LocalToWorld[1].xyz)),
                                 length(primitiveData.LocalToWorld[2].xyz));
        float worldRadius = cluster.BoundRadius * maxScale;

        if (IsClusterVisible(worldCenter, worldRadius, gPlanes))
        {
            s_Payload.VisibleClusterIndices[visibleCount] = clusterIdx;
            visibleCount++;
        }
    }
    DispatchMesh(visibleCount, 1, 1, s_Payload);
}
