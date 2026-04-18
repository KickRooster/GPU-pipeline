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

cbuffer cbSkyLight : register(b1)
{
    uint gIrradianceMapIndex;
    uint gPrefilteredMapIndex;
    uint gBRDFLUTIndex;
};

struct NaniteVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float2 UV0;
};

struct GPUCluster
{
    uint PrimitiveId;
    uint IndexCount;
    uint UniqueVerticesOffset;
    uint UniqueVerticesCount;
    uint LocalIndicesOffset;
    float3 BoundCenter;
    float BoundRadius;
    int Refined;
    int GroupId;
    uint TriangleMaterialIDsOffset;
};

struct FPrimitiveSceneData
{
    float4x4 LocalToWorld;
    float4x4 WorldInvTranspose;
};

struct GPUMaterial
{
    uint AlbedoTextureIndex;
    uint NormalTextureIndex;
    uint MetallicTextureIndex;
    uint RoughnessTextureIndex;
};

struct TerrainPatchData
{
    float WorldOffsetX;
    float WorldOffsetZ;
    float PatchSize;
    uint  PatchIndex;
    uint  LodLevel;
    uint  NeighborLodPacked;
};

StructuredBuffer<NaniteVertex>       NaniteVertices       : register(t20);
StructuredBuffer<uint>               NaniteUniqueVertices : register(t21);
ByteAddressBuffer                    NaniteLocalIndices   : register(t22);
StructuredBuffer<GPUCluster>         NaniteClusters       : register(t23);
StructuredBuffer<TerrainPatchData>   TerrainPatches       : register(t24);
StructuredBuffer<FPrimitiveSceneData> ScenePrimitives     : register(t25);
StructuredBuffer<uint>               TriangleMaterialIDs  : register(t26);
StructuredBuffer<GPUMaterial>        MaterialTable        : register(t27);
StructuredBuffer<uint>               NeighborLodBuffer    : register(t28);

// Visibility Buffer + HDR output
Texture2D<uint4>       VisibilityBuffer  : register(t0, space2);
RWTexture2D<float4>    OutputTexture     : register(u0);

// Bindless textures
Texture2D gBindlessTextures[]   : register(t30, space0);
TextureCube gBindlessCubemaps[] : register(t0, space1);
SamplerState gLinearSampler     : register(s0);

cbuffer cbTerrainInfo : register(b2)
{
    uint  gNaniteClusterCount;
    uint  gTerrainHeightmapIndex;
    float gTerrainWorldSize;
    float gTerrainHeightScale;
    uint4 gTerrainSplatmapAndLayerInfo; // xyz=splatmap indices, w=layerCount
    uint4 gTerrainAlbedoIndices[3];     // 12 layers packed as uint4[3]
    uint4 gTerrainNormalIndices[3];
    uint4 gTerrainRoughnessIndices[3];
    float gTerrainDebugScale;
    uint  gTerrainDebugLODColors;
};

uint GetTerrainSplatmapIndex(uint splatIdx)
{
    return gTerrainSplatmapAndLayerInfo[splatIdx];
}

uint GetTerrainLayerCount()
{
    return gTerrainSplatmapAndLayerInfo.w;
}

uint GetTerrainAlbedoIndex(uint layer)
{
    return gTerrainAlbedoIndices[layer / 4][layer % 4];
}

uint GetTerrainNormalIndex(uint layer)
{
    return gTerrainNormalIndices[layer / 4][layer % 4];
}

uint GetTerrainRoughnessIndex(uint layer)
{
    return gTerrainRoughnessIndices[layer / 4][layer % 4];
}

#define TERRAIN_GRID 11
#define TERRAIN_GRID_VERTS (TERRAIN_GRID * TERRAIN_GRID)
#define TERRAIN_GRID_TRIS  ((TERRAIN_GRID - 1) * (TERRAIN_GRID - 1) * 2)
#define TERRAIN_TILING_SCALE 8.0

float ComputeTerrainHeightMipLevel(float patchSize, uint heightmapWidth, uint heightmapHeight, uint heightmapMipCount)
{
    const float vertexSpacingWorld = patchSize / (float)(TERRAIN_GRID - 1);
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

float ComputeStitchedHeightResolve(uint edgeIndex, uint lodDiff, float2 edgeStartUV, float2 edgeEndUV, float coarseMip)
{
    uint step = 1u << lodDiff;
    uint lo = (edgeIndex >> lodDiff) << lodDiff;
    uint hi = min(lo + step, (uint)(TERRAIN_GRID - 1));

    if (edgeIndex == lo)
    {
        float tSelf = (float)lo / (float)(TERRAIN_GRID - 1);
        float2 hmUV = lerp(edgeStartUV, edgeEndUV, tSelf);
        return gBindlessTextures[NonUniformResourceIndex(gTerrainHeightmapIndex)].SampleLevel(gLinearSampler, hmUV, coarseMip).r * gTerrainHeightScale;
    }

    float tLo = (float)lo / (float)(TERRAIN_GRID - 1);
    float tHi = (float)hi / (float)(TERRAIN_GRID - 1);
    float2 hmUV_lo = lerp(edgeStartUV, edgeEndUV, tLo);
    float2 hmUV_hi = lerp(edgeStartUV, edgeEndUV, tHi);

    float hLo = gBindlessTextures[NonUniformResourceIndex(gTerrainHeightmapIndex)].SampleLevel(gLinearSampler, hmUV_lo, coarseMip).r * gTerrainHeightScale;
    float hHi = gBindlessTextures[NonUniformResourceIndex(gTerrainHeightmapIndex)].SampleLevel(gLinearSampler, hmUV_hi, coarseMip).r * gTerrainHeightScale;

    float t = (float)(edgeIndex - lo) / (float)(hi - lo);
    
    return lerp(hLo, hHi, t);
}

float ApplyEdgeStitchingResolve(float height, uint row, uint col, TerrainPatchData patch, float halfWorld, uint hmWidth, uint hmHeight, uint hmMipCount, uint patchIdx)
{
    if (gTerrainHeightmapIndex == 0xFFFFFFFF)
    {
        return height;
    }

    uint4 nLods = UnpackNeighborLODs(NeighborLodBuffer[patchIdx]);
    bool onTop    = (row == 0);
    bool onBottom = (row == TERRAIN_GRID - 1);
    bool onLeft   = (col == 0);
    bool onRight  = (col == TERRAIN_GRID - 1);

    float2 pMinUV = float2(
        (patch.WorldOffsetX + halfWorld) / gTerrainWorldSize,
        (patch.WorldOffsetZ + halfWorld) / gTerrainWorldSize);
    
    float2 pMaxUV = float2(
        (patch.WorldOffsetX + patch.PatchSize + halfWorld) / gTerrainWorldSize,
        (patch.WorldOffsetZ + patch.PatchSize + halfWorld) / gTerrainWorldSize);

    if (onTop && !onLeft && !onRight && nLods.x != 0xFF && nLods.x > patch.LodLevel)
    {
        uint lodDiff = nLods.x - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff), hmWidth, hmHeight, hmMipCount);
        return ComputeStitchedHeightResolve(col, lodDiff, float2(pMinUV.x, pMinUV.y), float2(pMaxUV.x, pMinUV.y), cMip);
    }
    
    if (onBottom && !onLeft && !onRight && nLods.y != 0xFF && nLods.y > patch.LodLevel)
    {
        uint lodDiff = nLods.y - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff), hmWidth, hmHeight, hmMipCount);
        return ComputeStitchedHeightResolve(col, lodDiff, float2(pMinUV.x, pMaxUV.y), float2(pMaxUV.x, pMaxUV.y), cMip);
    }
    
    if (onLeft && !onTop && !onBottom && nLods.z != 0xFF && nLods.z > patch.LodLevel)
    {
        uint lodDiff = nLods.z - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff), hmWidth, hmHeight, hmMipCount);
        return ComputeStitchedHeightResolve(row, lodDiff, float2(pMinUV.x, pMinUV.y), float2(pMinUV.x, pMaxUV.y), cMip);
    }
    
    if (onRight && !onTop && !onBottom && nLods.w != 0xFF && nLods.w > patch.LodLevel)
    {
        uint lodDiff = nLods.w - patch.LodLevel;
        float cMip = ComputeTerrainHeightMipLevel(patch.PatchSize * (float)(1u << lodDiff), hmWidth, hmHeight, hmMipCount);
        return ComputeStitchedHeightResolve(row, lodDiff, float2(pMaxUV.x, pMinUV.y), float2(pMaxUV.x, pMaxUV.y), cMip);
    }

    return height;
}

float3 GetTerrainVertexWorldPos(TerrainPatchData patch, uint vertexIndex, float halfWorld, float heightMipLevel, uint hmWidth, uint hmHeight, uint hmMipCount, uint patchIdx)
{
    uint row = vertexIndex / TERRAIN_GRID;
    uint col = vertexIndex % TERRAIN_GRID;

    float2 localUV = float2(col, row) / (float)(TERRAIN_GRID - 1);
    float3 worldPos = float3(
        patch.WorldOffsetX + localUV.x * patch.PatchSize,
        0.0,
        patch.WorldOffsetZ + localUV.y * patch.PatchSize
    );

    float2 hmUV = float2(
        (worldPos.x + halfWorld) / gTerrainWorldSize,
        (worldPos.z + halfWorld) / gTerrainWorldSize
    );

    if (gTerrainHeightmapIndex != 0xFFFFFFFF)
    {
        worldPos.y = gBindlessTextures[NonUniformResourceIndex(gTerrainHeightmapIndex)].SampleLevel(gLinearSampler, hmUV, heightMipLevel).r * gTerrainHeightScale;
        worldPos.y = ApplyEdgeStitchingResolve(worldPos.y, row, col, patch, halfWorld, hmWidth, hmHeight, hmMipCount, patchIdx);
    }

    return worldPos;
}

float SampleTerrainHeightWorld(float2 hmUV, float heightMipLevel)
{
    if (gTerrainHeightmapIndex == 0xFFFFFFFF)
    {
        return 0.0;
    }

    return gBindlessTextures[NonUniformResourceIndex(gTerrainHeightmapIndex)].SampleLevel(gLinearSampler, saturate(hmUV), heightMipLevel).r * gTerrainHeightScale;
}

float3 SampleTerrainNormalTS(uint textureIndex, float2 uv, float2 ddx_uv, float2 ddy_uv)
{
    if (textureIndex == 0xFFFFFFFF)
    {
        return float3(0.0, 0.0, 1.0);
    }

    float3 normalTS = gBindlessTextures[NonUniformResourceIndex(textureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).rgb;
    return normalize(normalTS * 2.0 - 1.0);
}

float SampleTerrainRoughness(uint textureIndex, float2 uv, float2 ddx_uv, float2 ddy_uv, float fallbackValue)
{
    if (textureIndex == 0xFFFFFFFF)
    {
        return fallbackValue;
    }

    return gBindlessTextures[NonUniformResourceIndex(textureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).r;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max((1.0 - roughness).xxx, F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

uint LoadLocalIndex(uint byteAddr)
{
    return (NaniteLocalIndices.Load(byteAddr & ~3u) >> ((byteAddr & 3u) * 8u)) & 0xFFu;
}

float3 TerrainLODColor(uint lodLevel)
{
    static const float3 palette[10] = {
        float3(1.0, 0.0, 0.0),   // 0: Red
        float3(0.0, 0.8, 0.0),   // 1: Green
        float3(0.2, 0.2, 1.0),   // 2: Blue
        float3(1.0, 0.0, 1.0),   // 3: Magenta
        float3(0.0, 1.0, 1.0),   // 4: Cyan
        float3(1.0, 1.0, 0.0),   // 5: Yellow
        float3(1.0, 1.0, 1.0),   // 6: White
        float3(0.0, 0.3, 0.8),   // 7: Navy
        float3(0.5, 0.0, 1.0),   // 8: Purple
        float3(0.0, 0.5, 0.0),   // 9: Dark Green
    };
    uint idx = lodLevel % 10u;
    float dim = (lodLevel >= 10u) ? 0.6 : 1.0;
    
    return palette[idx] * dim;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    bool valid = (id.x < (uint)gScreenWidth && id.y < (uint)gScreenHeight);
    uint4 vbData = (uint4)0;
    uint packed = 0;
    
    if (valid)
    {
        vbData = VisibilityBuffer[id.xy];
        packed = vbData.x;
        
        if (packed == 0)
        {
            OutputTexture[id.xy] = float4(0, 0, 0, 1);
            valid = false;
        }
    }

    uint clusterIndex = 0;
    uint triangleIndex = 0;
    float b0 = 0, b1 = 0, b2 = 0;
    float2 uv = float2(0, 0);
    float2 ddx_uv = float2(0, 0);
    float2 ddy_uv = float2(0, 0);
    float3 normalLS = float3(0, 0, 1);
    float3 tangentLS = float3(1, 0, 0);
    float3 posWS = float3(0, 0, 0);
    float3 N = float3(0, 1, 0);
    float3 T = float3(1, 0, 0);
    float3 B = float3(0, 0, 1);
    float3 albedo = float3(0.5, 0.5, 0.5);
    float metallic = 0.0;
    float roughness = 0.9;

    GPUCluster cluster = (GPUCluster)0;
    FPrimitiveSceneData prim = (FPrimitiveSceneData)0;
    uint materialId = 0;
    GPUMaterial material = (GPUMaterial)0;

    if (valid)
    {
        packed -= 1;
        // Visibility Buffer decoding (32-bit):
        // ClusterIndex: 22 bits (bits 10-31)
        // TriangleIndex: 10 bits (bits 0-9)
        clusterIndex  = packed >> 10;
        triangleIndex = packed & 0x3FF;

        b0 = asfloat(vbData.y);
        b1 = asfloat(vbData.z);
        b2 = 1.0 - b0 - b1;
    }

    bool isTerrain = false;
    if (valid)
    {
        isTerrain = (clusterIndex >= gNaniteClusterCount);
    }

    if (valid && isTerrain)
    {
        uint packedUV = vbData.w;
        float2 hmUV = float2(
            (packedUV & 0xFFFF) / 65535.0,
            (packedUV >> 16)    / 65535.0
        );

        float halfWorld = gTerrainWorldSize * 0.5;
        float worldX = hmUV.x * gTerrainWorldSize - halfWorld;
        float worldZ = hmUV.y * gTerrainWorldSize - halfWorld;

        float2 tilingUV = float2(worldX, worldZ) / TERRAIN_TILING_SCALE;
        uv = tilingUV;

        uint terrainPatchIndex = clusterIndex - gNaniteClusterCount;
        TerrainPatchData patch = TerrainPatches[terrainPatchIndex];

        float2 ddx_tiling = float2(0, 0);
        float2 ddy_tiling = float2(0, 0);
        float2 ddx_hmUV = float2(0, 0);
        float2 ddy_hmUV = float2(0, 0);
        uint heightmapWidth = 1;
        uint heightmapHeight = 1;
        uint heightmapMipCount = 1;
        if (gTerrainHeightmapIndex != 0xFFFFFFFF)
        {
            gBindlessTextures[NonUniformResourceIndex(gTerrainHeightmapIndex)].GetDimensions(0, heightmapWidth, heightmapHeight, heightmapMipCount);
        }

        const float heightMipLevel = ComputeTerrainHeightMipLevel(patch.PatchSize, heightmapWidth, heightmapHeight, heightmapMipCount);
        posWS = float3(worldX, SampleTerrainHeightWorld(hmUV, heightMipLevel), worldZ);
        posWS *= gTerrainDebugScale;

        if (triangleIndex < TERRAIN_GRID_TRIS)
        {
            uint vi0 = 0;
            uint vi1 = 0;
            uint vi2 = 0;

            uint quadIdx = triangleIndex / 2;
            uint triInQuad = triangleIndex % 2;
            uint qRow = quadIdx / (TERRAIN_GRID - 1);
            uint qCol = quadIdx % (TERRAIN_GRID - 1);
            uint topLeft = qRow * TERRAIN_GRID + qCol;

            if (triInQuad == 0)
            {
                vi0 = topLeft;     vi1 = topLeft + TERRAIN_GRID; vi2 = topLeft + 1;
            }
            else
            {
                vi0 = topLeft + 1; vi1 = topLeft + TERRAIN_GRID; vi2 = topLeft + TERRAIN_GRID + 1;
            }

            float3 p0 = GetTerrainVertexWorldPos(patch, vi0, halfWorld, heightMipLevel, heightmapWidth, heightmapHeight, heightmapMipCount, terrainPatchIndex);
            float3 p1 = GetTerrainVertexWorldPos(patch, vi1, halfWorld, heightMipLevel, heightmapWidth, heightmapHeight, heightmapMipCount, terrainPatchIndex);
            float3 p2 = GetTerrainVertexWorldPos(patch, vi2, halfWorld, heightMipLevel, heightmapWidth, heightmapHeight, heightmapMipCount, terrainPatchIndex);
            float3 posUnscaled = b0 * p0 + b1 * p1 + b2 * p2;
            worldX = posUnscaled.x;
            worldZ = posUnscaled.z;
            posWS = posUnscaled * gTerrainDebugScale;
            tilingUV = posWS.xz / TERRAIN_TILING_SCALE;
            uv = tilingUV;

            float4 p0CS = mul(float4(p0 * gTerrainDebugScale, 1), gViewProj);
            float4 p1CS = mul(float4(p1 * gTerrainDebugScale, 1), gViewProj);
            float4 p2CS = mul(float4(p2 * gTerrainDebugScale, 1), gViewProj);

            float2 p0SS = float2((p0CS.x / p0CS.w * 0.5 + 0.5) * gScreenWidth, (0.5 - p0CS.y / p0CS.w * 0.5) * gScreenHeight);
            float2 p1SS = float2((p1CS.x / p1CS.w * 0.5 + 0.5) * gScreenWidth, (0.5 - p1CS.y / p1CS.w * 0.5) * gScreenHeight);
            float2 p2SS = float2((p2CS.x / p2CS.w * 0.5 + 0.5) * gScreenWidth, (0.5 - p2CS.y / p2CS.w * 0.5) * gScreenHeight);

            float2 tuv0 = p0.xz * gTerrainDebugScale / TERRAIN_TILING_SCALE;
            float2 tuv1 = p1.xz * gTerrainDebugScale / TERRAIN_TILING_SCALE;
            float2 tuv2 = p2.xz * gTerrainDebugScale / TERRAIN_TILING_SCALE;

            float2 e1 = p1SS - p0SS;
            float2 e2 = p2SS - p0SS;
            float2 duv1 = tuv1 - tuv0;
            float2 duv2 = tuv2 - tuv0;

            float det = e1.x * e2.y - e1.y * e2.x;
            float inv_det = (abs(det) > 1e-7) ? (1.0 / det) : 0.0;

            ddx_tiling = ( duv1 * e2.y - duv2 * e1.y) * inv_det;
            ddy_tiling = (-duv1 * e2.x + duv2 * e1.x) * inv_det;
        }
        ddx_uv = ddx_tiling;
        ddy_uv = ddy_tiling;

        hmUV = float2((worldX + halfWorld) / gTerrainWorldSize, (worldZ + halfWorld) / gTerrainWorldSize);

        if (gTerrainWorldSize > 1e-6)
        {
            const float tilingToHeightmap = TERRAIN_TILING_SCALE / (gTerrainWorldSize * gTerrainDebugScale);
            ddx_hmUV = ddx_tiling * tilingToHeightmap;
            ddy_hmUV = ddy_tiling * tilingToHeightmap;
        }

        uint layerCount = GetTerrainLayerCount();
        float totalWeight = 0.0;
        float3 blendedNormalTS = float3(0, 0, 0);
        albedo = float3(0, 0, 0);
        roughness = 0.0;

        for (uint layer = 0; layer < layerCount; ++layer)
        {
            uint splatIdx = layer / 4;
            uint channel = layer % 4;
            uint splatTexIdx = GetTerrainSplatmapIndex(splatIdx);
            if (splatTexIdx == 0xFFFFFFFF)
            {
                continue;
            }

            float4 splatSample = gBindlessTextures[NonUniformResourceIndex(splatTexIdx)].SampleGrad(gLinearSampler, hmUV, ddx_hmUV, ddy_hmUV);
            float weight = splatSample[channel];
            if (weight < 0.01)
            {
                continue;
            }

            uint albedoIdx = GetTerrainAlbedoIndex(layer);
            uint normalIdx = GetTerrainNormalIndex(layer);
            uint roughIdx = GetTerrainRoughnessIndex(layer);

            if (albedoIdx != 0xFFFFFFFF)
            {
                albedo += weight * gBindlessTextures[NonUniformResourceIndex(albedoIdx)].SampleGrad(gLinearSampler, tilingUV, ddx_tiling, ddy_tiling).rgb;
            }

            blendedNormalTS += weight * SampleTerrainNormalTS(normalIdx, tilingUV, ddx_tiling, ddy_tiling);
            roughness += weight * SampleTerrainRoughness(roughIdx, tilingUV, ddx_tiling, ddy_tiling, 0.9);
            totalWeight += weight;
        }
        if (totalWeight > 0.001)
        {
            albedo /= totalWeight;
            blendedNormalTS /= totalWeight;
            roughness /= totalWeight;
        }
        metallic = 0.0;

        // Normal from heightmap finite differences — always use mip 0 for consistency across LOD boundaries
        float2 heightTexel = 1.0 / float2(max(heightmapWidth, 2u) - 1u, max(heightmapHeight, 2u) - 1u);
        float heightLeft  = SampleTerrainHeightWorld(hmUV - float2(heightTexel.x, 0.0), 0.0);
        float heightRight = SampleTerrainHeightWorld(hmUV + float2(heightTexel.x, 0.0), 0.0);
        float heightDown  = SampleTerrainHeightWorld(hmUV - float2(0.0, heightTexel.y), 0.0);
        float heightUp    = SampleTerrainHeightWorld(hmUV + float2(0.0, heightTexel.y), 0.0);

        float worldStepX = heightTexel.x * gTerrainWorldSize;
        float worldStepZ = heightTexel.y * gTerrainWorldSize;
        float3 dPosdX = float3(2.0 * worldStepX, heightRight - heightLeft, 0.0);
        float3 dPosdZ = float3(0.0, heightUp - heightDown, 2.0 * worldStepZ);

        N = normalize(cross(dPosdZ, dPosdX));
        T = normalize(dPosdX - dot(dPosdX, N) * N);
        B = normalize(cross(N, T));

        float3 terrainNormalTS = normalize(blendedNormalTS);
        N = normalize(mul(terrainNormalTS, float3x3(T, B, N)));
        T = normalize(T - dot(T, N) * N);
        B = normalize(cross(N, T));

        if (gTerrainDebugLODColors)
        {
            OutputTexture[id.xy] = float4(TerrainLODColor(patch.LodLevel), 1.0);
            return;
        }
    }

    if (valid && !isTerrain)
    {
        cluster = NaniteClusters[clusterIndex];
        prim = ScenePrimitives[cluster.PrimitiveId];

        materialId = TriangleMaterialIDs[cluster.TriangleMaterialIDsOffset + triangleIndex];
        material = MaterialTable[materialId];

        uint baseLocalIdx = cluster.LocalIndicesOffset + triangleIndex * 3;
        uint li0 = LoadLocalIndex(baseLocalIdx);
        uint li1 = LoadLocalIndex(baseLocalIdx + 1);
        uint li2 = LoadLocalIndex(baseLocalIdx + 2);

        NaniteVertex v0 = NaniteVertices[NaniteUniqueVertices[cluster.UniqueVerticesOffset + li0]];
        NaniteVertex v1 = NaniteVertices[NaniteUniqueVertices[cluster.UniqueVerticesOffset + li1]];
        NaniteVertex v2 = NaniteVertices[NaniteUniqueVertices[cluster.UniqueVerticesOffset + li2]];

        uv       = b0 * v0.UV0     + b1 * v1.UV0     + b2 * v2.UV0;
        normalLS = b0 * v0.Normal  + b1 * v1.Normal  + b2 * v2.Normal;
        tangentLS = b0 * v0.Tangent + b1 * v1.Tangent + b2 * v2.Tangent;

        float3 p0WS = mul(float4(v0.Position, 1), prim.LocalToWorld).xyz;
        float3 p1WS = mul(float4(v1.Position, 1), prim.LocalToWorld).xyz;
        float3 p2WS = mul(float4(v2.Position, 1), prim.LocalToWorld).xyz;
        posWS = b0 * p0WS + b1 * p1WS + b2 * p2WS;

        float4 p0CS = mul(float4(p0WS, 1), gViewProj);
        float4 p1CS = mul(float4(p1WS, 1), gViewProj);
        float4 p2CS = mul(float4(p2WS, 1), gViewProj);

        float2 p0SS = float2((p0CS.x / p0CS.w * 0.5 + 0.5) * gScreenWidth,
                             (0.5 - p0CS.y / p0CS.w * 0.5) * gScreenHeight);
        float2 p1SS = float2((p1CS.x / p1CS.w * 0.5 + 0.5) * gScreenWidth,
                             (0.5 - p1CS.y / p1CS.w * 0.5) * gScreenHeight);
        float2 p2SS = float2((p2CS.x / p2CS.w * 0.5 + 0.5) * gScreenWidth,
                             (0.5 - p2CS.y / p2CS.w * 0.5) * gScreenHeight);

        float2 e1 = p1SS - p0SS;
        float2 e2 = p2SS - p0SS;
        float2 duv1 = v1.UV0 - v0.UV0;
        float2 duv2 = v2.UV0 - v0.UV0;

        float det = e1.x * e2.y - e1.y * e2.x;
        float inv_det = (abs(det) > 1e-7) ? (1.0 / det) : 0.0;

        ddx_uv = ( duv1 * e2.y - duv2 * e1.y) * inv_det;
        ddy_uv = (-duv1 * e2.x + duv2 * e1.x) * inv_det;

        N = normalize(mul(float4(normalLS, 0), prim.WorldInvTranspose).xyz);
        T = normalize(mul(float4(tangentLS, 0), prim.LocalToWorld).xyz);
        T = normalize(T - dot(T, N) * N);
        B = cross(N, T);

        if (material.AlbedoTextureIndex != 0xFFFFFFFF)
        {
            albedo = gBindlessTextures[NonUniformResourceIndex(material.AlbedoTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).rgb;
        }
        if (material.NormalTextureIndex != 0xFFFFFFFF)
        {
            float3 normalTS = gBindlessTextures[NonUniformResourceIndex(material.NormalTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).rgb;
            normalTS = normalTS * 2.0 - 1.0;
            N = normalize(mul(normalTS, float3x3(T, B, N)));
        }
        if (material.MetallicTextureIndex != 0xFFFFFFFF)
        {
            metallic = gBindlessTextures[NonUniformResourceIndex(material.MetallicTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).r;
        }
        if (material.RoughnessTextureIndex != 0xFFFFFFFF)
        {
            roughness = gBindlessTextures[NonUniformResourceIndex(material.RoughnessTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).r;
        }
    }

    if (!valid)
    {
        return;
    }

    float3 V = normalize(gViewPosition - posWS);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float NdotV = max(dot(N, V), 0.0);
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 irradiance = gBindlessCubemaps[NonUniformResourceIndex(gIrradianceMapIndex)].SampleLevel(gLinearSampler, N, 0).rgb;
    if (isTerrain)
    {
        float lum = max(dot(irradiance, float3(0.2126, 0.7152, 0.0722)), 0.001);
        irradiance = irradiance / lum * min(lum, 1.0);
    }
    float3 diffuse = kD * irradiance * albedo;

    float3 R = reflect(-V, N);
    float3 prefilteredColor = gBindlessCubemaps[NonUniformResourceIndex(gPrefilteredMapIndex)].SampleLevel(gLinearSampler, R, roughness * 4.0).rgb;
    float2 envBRDF = gBindlessTextures[NonUniformResourceIndex(gBRDFLUTIndex)].SampleLevel(gLinearSampler, float2(NdotV, roughness), 0).rg;
    float3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    float3 finalColor = diffuse + (isTerrain ? float3(0,0,0) : specular);
    OutputTexture[id.xy] = float4(finalColor, 1.0);
}
