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

struct TerrainPatchData
{
    float WorldOffsetX;
    float WorldOffsetZ;
    float PatchSize;
    uint  PatchIndex;
    uint  LodLevel;
    uint  NeighborLodPacked; // [7:0]=Top [15:8]=Bottom [23:16]=Left [31:24]=Right, 0xFF=no snap
};

StructuredBuffer<TerrainPatchData> TerrainPatches : register(t0);
Texture2D<float> gHeightmap : register(t2);
SamplerState gHeightmapSampler : register(s0);
StructuredBuffer<uint> NeighborLodBuffer : register(t4); // Per active-patch NeighborLodPacked

#define TERRAIN_PATCHES_PER_GROUP 64
#define GRID_SIZE 11
#define GRID_VERTS (GRID_SIZE * GRID_SIZE)                    // 121
#define GRID_TRIS  ((GRID_SIZE - 1) * (GRID_SIZE - 1) * 2)   // 200
#define THREAD_COUNT 128

struct Payload
{
    uint VisiblePatchIndices[TERRAIN_PATCHES_PER_GROUP];
};

struct VertexOut
{
    float4 PositionHS : SV_Position;
    float2 TerrainUV  : TEXCOORD0;  // heightmap UV [0,1], packed as UNORM16 in PS
};

struct PrimitiveOut
{
    uint ClusterIndex  : COLOR1;
    uint TriangleIndex : COLOR2;
};

float ComputeTerrainHeightMipLevel(float patchSize)
{
    uint heightmapWidth = 1;
    uint heightmapHeight = 1;
    uint heightmapMipCount = 1;
    gHeightmap.GetDimensions(0, heightmapWidth, heightmapHeight, heightmapMipCount);

    const float vertexSpacingWorld = patchSize / (float)(GRID_SIZE - 1);
    const float texelWorldX = gTerrainWorldSize / max((float)(max(heightmapWidth, 2u) - 1u), 1.0);
    const float texelWorldZ = gTerrainWorldSize / max((float)(max(heightmapHeight, 2u) - 1u), 1.0);
    const float samplingRatio = max(vertexSpacingWorld / texelWorldX, vertexSpacingWorld / texelWorldZ);
    const float mipLevel = log2(max(samplingRatio, 1.0));

    return clamp(mipLevel, 0.0, (float)(max(heightmapMipCount, 1u) - 1u));
}

uint4 UnpackNeighborLODs(uint packed)
{
    return uint4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, (packed >> 24) & 0xFF);
}

// Snap an edge vertex height to match the coarser neighbor's linear interpolation.
// edgeIndex: vertex position along edge (0..GRID_SIZE-1)
// lodDiff: neighborLod - patchLod
// edgeStartUV/edgeEndUV: heightmap UV at edge endpoints
// coarseMip: mip level the coarse neighbor uses
float ComputeStitchedHeight(uint edgeIndex, uint lodDiff, float2 edgeStartUV, float2 edgeEndUV, float coarseMip)
{
    uint step = 1u << lodDiff;
    uint lo = (edgeIndex >> lodDiff) << lodDiff;
    uint hi = min(lo + step, (uint)(GRID_SIZE - 1));

    if (edgeIndex == lo)
    {
        float tSelf = (float)lo / (float)(GRID_SIZE - 1);
        float2 hmUV = lerp(edgeStartUV, edgeEndUV, tSelf);
        return gHeightmap.SampleLevel(gHeightmapSampler, hmUV, coarseMip).r * gTerrainHeightScale;
    }

    float tLo = (float)lo / (float)(GRID_SIZE - 1);
    float tHi = (float)hi / (float)(GRID_SIZE - 1);
    float2 hmUV_lo = lerp(edgeStartUV, edgeEndUV, tLo);
    float2 hmUV_hi = lerp(edgeStartUV, edgeEndUV, tHi);

    float hLo = gHeightmap.SampleLevel(gHeightmapSampler, hmUV_lo, coarseMip).r * gTerrainHeightScale;
    float hHi = gHeightmap.SampleLevel(gHeightmapSampler, hmUV_hi, coarseMip).r * gTerrainHeightScale;

    float t = (float)(edgeIndex - lo) / (float)(hi - lo);
    
    return lerp(hLo, hHi, t);
}

// Apply edge stitching to a vertex at (row, col). Returns stitched height or original.
// patchIdx: index into NeighborLodBuffer
float ApplyEdgeStitching(float height, uint row, uint col, TerrainPatchData patch, float halfWorld, uint patchIdx)
{
    uint4 nLods = UnpackNeighborLODs(NeighborLodBuffer[patchIdx]);
    bool onTop    = (row == 0);
    bool onBottom = (row == GRID_SIZE - 1);
    bool onLeft   = (col == 0);
    bool onRight  = (col == GRID_SIZE - 1);

    float2 pMinUV = float2(
        (patch.WorldOffsetX + halfWorld) / gTerrainWorldSize,
        (patch.WorldOffsetZ + halfWorld) / gTerrainWorldSize);
    float2 pMaxUV = float2(
        (patch.WorldOffsetX + patch.PatchSize + halfWorld) / gTerrainWorldSize,
        (patch.WorldOffsetZ + patch.PatchSize + halfWorld) / gTerrainWorldSize);

    if (onTop && !onLeft && !onRight && nLods.x != 0xFF && nLods.x > patch.LodLevel)
    {
        uint lodDiff = nLods.x - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff));
        return ComputeStitchedHeight(col, lodDiff, float2(pMinUV.x, pMinUV.y), float2(pMaxUV.x, pMinUV.y), cMip);
    }
    if (onBottom && !onLeft && !onRight && nLods.y != 0xFF && nLods.y > patch.LodLevel)
    {
        uint lodDiff = nLods.y - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff));
        return ComputeStitchedHeight(col, lodDiff, float2(pMinUV.x, pMaxUV.y), float2(pMaxUV.x, pMaxUV.y), cMip);
    }
    if (onLeft && !onTop && !onBottom && nLods.z != 0xFF && nLods.z > patch.LodLevel)
    {
        uint lodDiff = nLods.z - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff));
        return ComputeStitchedHeight(row, lodDiff, float2(pMinUV.x, pMinUV.y), float2(pMinUV.x, pMaxUV.y), cMip);
    }
    if (onRight && !onTop && !onBottom && nLods.w != 0xFF && nLods.w > patch.LodLevel)
    {
        uint lodDiff = nLods.w - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff));
        return ComputeStitchedHeight(row, lodDiff, float2(pMaxUV.x, pMinUV.y), float2(pMaxUV.x, pMaxUV.y), cMip);
    }

    return height;
}

[numthreads(THREAD_COUNT, 1, 1)]
[outputtopology("triangle")]
void main(
    uint gid  : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    in payload Payload payloadData,
    out vertices VertexOut vertices[GRID_VERTS],
    out primitives PrimitiveOut primitives[GRID_TRIS],
    out indices uint3 triangles[GRID_TRIS])
{
    uint patchIdx = payloadData.VisiblePatchIndices[gid];

    uint vertexCount = 0;
    uint triangleCount = 0;

    if (patchIdx < gTerrainTotalPatchCount)
    {
        vertexCount = GRID_VERTS;
        triangleCount = GRID_TRIS;
    }

    SetMeshOutputCounts(vertexCount, triangleCount);

    if (patchIdx >= gTerrainTotalPatchCount)
    {
        return;
    }
    
    TerrainPatchData patch = TerrainPatches[patchIdx];
    float halfWorld = gTerrainWorldSize * 0.5;

    if (gtid < GRID_VERTS)
    {
        uint row = gtid / GRID_SIZE;
        uint col = gtid % GRID_SIZE;
        float2 localUV = float2(col, row) / (float)(GRID_SIZE - 1);

        float3 worldPos = float3(
            patch.WorldOffsetX + localUV.x * patch.PatchSize,
            0.0,
            patch.WorldOffsetZ + localUV.y * patch.PatchSize
        );
        float2 heightmapUV = float2(
            (worldPos.x + halfWorld) / gTerrainWorldSize,
            (worldPos.z + halfWorld) / gTerrainWorldSize
        );
        worldPos.y = gHeightmap.SampleLevel(gHeightmapSampler, heightmapUV, 0).r * gTerrainHeightScale;
        if (gEdgeStitchingEnabled)
            worldPos.y = ApplyEdgeStitching(worldPos.y, row, col, patch, halfWorld, patchIdx);

        worldPos *= gDebugScale;

        VertexOut vout;
        vout.PositionHS = mul(float4(worldPos, 1.0), gViewProj);
        vout.TerrainUV = heightmapUV;
        vertices[gtid] = vout;
    }

    uint clusterIndex = gTerrainClusterBase + patchIdx;

    for (uint pass = 0; pass < 2; ++pass)
    {
        uint triIdx = gtid + pass * THREAD_COUNT;
        if (triIdx < GRID_TRIS)
        {
            uint quadIdx = triIdx / 2;
            uint triInQuad = triIdx % 2;
            uint qRow = quadIdx / (GRID_SIZE - 1);
            uint qCol = quadIdx % (GRID_SIZE - 1);
            uint topLeft = qRow * GRID_SIZE + qCol;

            if (triInQuad == 0)
            {
                triangles[triIdx] = uint3(topLeft, topLeft + GRID_SIZE, topLeft + 1);
            }
            else
            {
                triangles[triIdx] = uint3(topLeft + 1, topLeft + GRID_SIZE, topLeft + GRID_SIZE + 1);
            }
            
            primitives[triIdx].ClusterIndex = clusterIndex;
            primitives[triIdx].TriangleIndex = triIdx;
        }
    }
}