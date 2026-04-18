cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
    float    gScreenWidth;
    float    gScreenHeight;
    float    gRecipTanHalfFovy;
    float    gLODErrorThreshold;
    float    gNearPlane;
};

cbuffer cbTerrainParams : register(b1)
{
    uint  gTerrainActivePatchCount;
    uint  gTerrainClusterBase;
    float gTerrainPatchSize;
    float gTerrainHeightScale;
    float gTerrainWorldSize;
    uint  gTerrainTotalPatchCount;
    uint  gEdgeStitchingEnabled;
    float gDebugScale;
};

struct TerrainPatchBound
{
    float3 Center;
    float3 HalfExtent;
};

StructuredBuffer<TerrainPatchBound> TerrainBounds : register(t1);
StructuredBuffer<uint> ActivePatchIndices : register(t3);

#define TERRAIN_PATCHES_PER_GROUP 64

struct Payload
{
    uint VisiblePatchIndices[TERRAIN_PATCHES_PER_GROUP];
};

groupshared Payload s_Payload;
groupshared uint s_VisibleCount;

bool IsPatchVisible(float3 center, float3 halfExtent, float4 planes[6])
{
    for (int i = 0; i < 6; i++)
    {
        // AABB vs plane: compute signed distance of the closest point
        float r = dot(abs(planes[i].xyz), halfExtent);
        float d = dot(planes[i].xyz, center) + planes[i].w;
        
        if (d + r < 0.0)
        {
            return false;
        }
    }
    
    return true;
}

[numthreads(TERRAIN_PATCHES_PER_GROUP, 1, 1)]
void main(
    uint gid  : SV_GroupID,
    uint gtid : SV_GroupThreadID)
{
    if (gtid == 0)
    {
        s_VisibleCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint activeIdx = gid * TERRAIN_PATCHES_PER_GROUP + gtid;
    bool shouldDispatch = false;

    if (activeIdx < gTerrainActivePatchCount)
    {
        uint patchIdx = ActivePatchIndices[activeIdx];

        TerrainPatchBound bound = TerrainBounds[patchIdx];
        float3 scaledCenter = bound.Center * gDebugScale;
        float3 scaledExtent = bound.HalfExtent * gDebugScale;

        if (IsPatchVisible(scaledCenter, scaledExtent, gPlanes))
        {
            shouldDispatch = true;
        }
    }

    if (shouldDispatch)
    {
        uint idx;
        InterlockedAdd(s_VisibleCount, 1, idx);
        s_Payload.VisiblePatchIndices[idx] = ActivePatchIndices[activeIdx];
    }

    GroupMemoryBarrierWithGroupSync();
    DispatchMesh(s_VisibleCount, 1, 1, s_Payload);
}