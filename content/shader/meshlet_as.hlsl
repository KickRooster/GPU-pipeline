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

cbuffer cbStaticMesh : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4   gBoundingSphere;
    uint4    gPBRTextureIndices[4];  // 16 bytes per element (matches C++ layout)
    uint     gNaniteClusterCount;
    uint     gPadding0;
    uint     gPadding1;
    uint     gPadding2;
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
    uint IndexCount;
    uint UniqueVerticesOffset;
    uint UniqueVerticesCount;
    uint LocalIndicesOffset;
    float3 BoundCenter;
    float BoundRadius;
    int Refined;
    int GroupId;
};

struct GPUGroupBound
{
    float3 Center;
    float Radius;
    float Error;
};

// Nanite buffers: Vertex -> UniqueVertices -> LocalIndices -> Cluster -> GroupBounds
StructuredBuffer<NaniteVertex> NaniteVertices : register(t20);
StructuredBuffer<uint> NaniteUniqueVertices : register(t21);
ByteAddressBuffer NaniteLocalIndices : register(t22);
StructuredBuffer<GPUCluster> NaniteClusters : register(t23);
StructuredBuffer<GPUGroupBound> NaniteGroupBounds : register(t24);

struct Payload
{
    uint ASGroupID;  // AS group ID for cluster index calculation
};

groupshared Payload s_Payload;

[numthreads(1, 1, 1)]  // One thread per AS group
void main(
    uint gid : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{
    s_Payload.ASGroupID = gid;

    // Each AS group dispatches up to AS_CLUSTERS_PER_GROUP clusters
    uint startCluster = gid * AS_CLUSTERS_PER_GROUP;
    uint clustersToDispatch = min(AS_CLUSTERS_PER_GROUP, max(0, int(gNaniteClusterCount) - int(startCluster)));

    DispatchMesh(clustersToDispatch, 1, 1, s_Payload);
}
