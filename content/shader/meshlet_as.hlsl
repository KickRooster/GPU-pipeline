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

struct GPUCluster
{
    uint PrimitiveId;            // Index to ScenePrimitiveBuffer (GPU Scene)
    uint IndexCount;
    uint UniqueVerticesOffset;
    uint UniqueVerticesCount;
    uint LocalIndicesOffset;
    float3 BoundCenter;
    float BoundRadius;
    int Refined;                 // Index to more detailed group (-1 = leaf, for LOD selection)
    int GroupId;
    uint TriangleMaterialIDsOffset;     // Offset in TriangleMaterialIDs buffer
};

struct GPUGroupBound
{
    float3 Center;
    float Radius;
    float Error;
};

// Nanite buffers
StructuredBuffer<GPUCluster>    NaniteClusters    : register(t23);
StructuredBuffer<GPUGroupBound> NaniteGroupBounds : register(t24);

// GPU Scene: Primitive transform data (Root Descriptor, t26, UE5-style)
struct FPrimitiveSceneData
{
    float4x4 LocalToWorld;
    float4x4 WorldInvTranspose;
};
StructuredBuffer<FPrimitiveSceneData> ScenePrimitives : register(t26);

struct Payload
{
    uint VisibleClusterIndices[AS_CLUSTERS_PER_GROUP];
};

groupshared Payload s_Payload;
groupshared uint s_VisibleCount;

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

float ComputeScreenError(GPUGroupBound groupBound, float3 cameraPos, float recipTanHalfFovy, float nearPlane, float4x4 localToWorld)
{
    float3 worldCenter = mul(float4(groupBound.Center, 1.0), localToWorld).xyz;
    float maxScale = max(max(length(localToWorld[0].xyz), length(localToWorld[1].xyz)), length(localToWorld[2].xyz));
    float worldRadius = groupBound.Radius * maxScale;
    float worldDist = length(worldCenter - cameraPos) - worldRadius;
    float localDist = max(worldDist, nearPlane) / maxScale;
    return groupBound.Error / localDist * (recipTanHalfFovy * 0.5f);
}

bool ShouldRenderCluster(GPUCluster cluster, float3 cameraPos, float recipTanHalfFovy, float4x4 localToWorld)
{
    const float threshold = gLODErrorThreshold / gScreenHeight;

    GPUGroupBound myGroup = NaniteGroupBounds[cluster.GroupId];
    float myGroupError = ComputeScreenError(myGroup, cameraPos, recipTanHalfFovy, gNearPlane, localToWorld);
    if (myGroupError <= threshold)
        return false;

    if (cluster.Refined == -1)
        return true;

    GPUGroupBound refinedGroup = NaniteGroupBounds[cluster.Refined];
    float refinedGroupError = ComputeScreenError(refinedGroup, cameraPos, recipTanHalfFovy, gNearPlane, localToWorld);
    return refinedGroupError <= threshold;
}

[numthreads(AS_CLUSTERS_PER_GROUP, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID)
{
    if (gtid == 0)
        s_VisibleCount = 0;
    GroupMemoryBarrierWithGroupSync();

    uint clusterIdx = gid * AS_CLUSTERS_PER_GROUP + gtid;
    bool shouldDispatch = false;

    if (clusterIdx < gNaniteClusterCount)
    {
        GPUCluster cluster = NaniteClusters[clusterIdx];
        FPrimitiveSceneData primitiveData = ScenePrimitives[cluster.PrimitiveId];

        float3 worldCenter = mul(float4(cluster.BoundCenter, 1.0), primitiveData.LocalToWorld).xyz;
        float maxScale = max(max(length(primitiveData.LocalToWorld[0].xyz),
                                 length(primitiveData.LocalToWorld[1].xyz)),
                                 length(primitiveData.LocalToWorld[2].xyz));
        float worldRadius = cluster.BoundRadius * maxScale;

        // Frustum culling + cluster selection
        if (IsClusterVisible(worldCenter, worldRadius, gPlanes) &&
            ShouldRenderCluster(cluster, gViewPosition, gRecipTanHalfFovy, primitiveData.LocalToWorld))
            shouldDispatch = true;
    }

    if (shouldDispatch)
    {
        uint idx;
        InterlockedAdd(s_VisibleCount, 1, idx);
        s_Payload.VisibleClusterIndices[idx] = clusterIdx;
    }

    GroupMemoryBarrierWithGroupSync();
    DispatchMesh(s_VisibleCount, 1, 1, s_Payload);
}
