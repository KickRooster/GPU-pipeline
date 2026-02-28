// Material Resolve Compute Shader
// Reads Visibility Buffer (R32G32B32A32_UINT: packed ClusterId+TriangleId + barycentrics),
// reconstructs attributes via barycentrics, performs PBR+IBL shading.

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

// Nanite buffers
StructuredBuffer<NaniteVertex>       NaniteVertices       : register(t20);
StructuredBuffer<uint>               NaniteUniqueVertices : register(t21);
ByteAddressBuffer                    NaniteLocalIndices   : register(t22);
StructuredBuffer<GPUCluster>         NaniteClusters       : register(t23);
StructuredBuffer<FPrimitiveSceneData> ScenePrimitives     : register(t26);
StructuredBuffer<uint>               TriangleMaterialIDs  : register(t27);
StructuredBuffer<GPUMaterial>        MaterialTable        : register(t28);

// Visibility Buffer + HDR output
Texture2D<uint4>       VisibilityBuffer  : register(t0, space2);
RWTexture2D<float4>    OutputTexture     : register(u0);

// Bindless textures
Texture2D gBindlessTextures[]   : register(t30, space0);
TextureCube gBindlessCubemaps[] : register(t0, space1);
SamplerState gLinearSampler     : register(s0);

// Debug output mode:
//   0 = Albedo only           1 = Full PBR (IBL)
//   2 = Material ID           3 = Cluster ID
//   4 = UV coordinates        5 = Texture index
//   6 = Barycentrics          7 = Solid red (output path test)
//   8 = Gradient magnitude    9 = Albedo SampleLevel(0)
//  10 = Albedo fixed UV      11 = Raw UV (NaN/extreme check)
//  12 = Texture[0] all pixels 13 = MaterialId binary
#define DEBUG_OUTPUT 1

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max((1.0 - roughness).xxx, F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

uint LoadLocalIndex(uint byteAddr)
{
    return (NaniteLocalIndices.Load(byteAddr & ~3u) >> ((byteAddr & 3u) * 8u)) & 0xFFu;
}

// Hash-based false-color visualization
float3 HashColor(uint value)
{
    return float3(frac((value + 1) * 0.618033988749895),
                  frac((value + 1) * 0.382694821),
                  frac((value + 1) * 0.123456789));
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

    GPUCluster cluster = (GPUCluster)0;
    FPrimitiveSceneData prim = (FPrimitiveSceneData)0;
    uint materialId = 0;
    GPUMaterial material = (GPUMaterial)0;

    if (valid)
    {
        packed -= 1;
        clusterIndex  = packed >> 7;
        triangleIndex = packed & 0x7F;

        b0 = asfloat(vbData.y);
        b1 = asfloat(vbData.z);
        b2 = 1.0 - b0 - b1;

#if DEBUG_OUTPUT == 7
        OutputTexture[id.xy] = float4(1, 0, 0, 1); valid = false;
#elif DEBUG_OUTPUT == 6
        OutputTexture[id.xy] = float4(b0, b1, b2, 1.0); valid = false;
#elif DEBUG_OUTPUT == 3
        OutputTexture[id.xy] = float4(HashColor(clusterIndex), 1.0); valid = false;
#endif
    }

    if (valid)
    {
        cluster = NaniteClusters[clusterIndex];
        prim = ScenePrimitives[cluster.PrimitiveId];

        materialId = TriangleMaterialIDs[cluster.TriangleMaterialIDsOffset + triangleIndex];
        material = MaterialTable[materialId];

#if DEBUG_OUTPUT == 2
        OutputTexture[id.xy] = float4(HashColor(materialId), 1.0); valid = false;
#elif DEBUG_OUTPUT == 5
        OutputTexture[id.xy] = float4(HashColor(material.AlbedoTextureIndex), 1.0); valid = false;
#elif DEBUG_OUTPUT == 13
        OutputTexture[id.xy] = (materialId == 0) ? float4(1,0,0,1) : float4(0,1,0,1); valid = false;
#endif
    }

    if (valid)
    {
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

        // Analytical UV gradient: world → clip → screen, then 2x2 inversion
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
    }

    if (!valid)
        return;

    // ---- Debug: gradient visualization ----
#if DEBUG_OUTPUT == 8
    float gradMag = max(length(ddx_uv), length(ddy_uv));
    OutputTexture[id.xy] = float4(saturate(gradMag * 10.0), saturate(gradMag * 2.0), 0.0, 1.0);
    return;
#endif

    float3 N = normalize(mul(float4(normalLS, 0), prim.WorldInvTranspose).xyz);
    float3 T = normalize(mul(float4(tangentLS, 0), prim.LocalToWorld).xyz);
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);

    // ---- Debug: attribute visualization ----
#if DEBUG_OUTPUT == 4
    OutputTexture[id.xy] = float4(frac(uv.x), frac(uv.y), 0.0, 1.0); return;
#elif DEBUG_OUTPUT == 11
    bool uvBad = isnan(uv.x) || isnan(uv.y) || isinf(uv.x) || isinf(uv.y);
    if (uvBad) { OutputTexture[id.xy] = float4(0, 1, 0, 1); return; }
    if (abs(uv.x) > 10.0 || abs(uv.y) > 10.0) { OutputTexture[id.xy] = float4(0, 0, 1, 1); return; }
    OutputTexture[id.xy] = float4(saturate(uv.x), saturate(uv.y), 0.0, 1.0); return;
#endif

    // ---- Sample textures ----
    float3 albedo = float3(0.5, 0.5, 0.5);
    float metallic = 0.0;
    float roughness = 0.5;

    if (material.AlbedoTextureIndex != 0xFFFFFFFF)
        albedo = gBindlessTextures[NonUniformResourceIndex(material.AlbedoTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).rgb;

    // ---- Debug: texture sampling visualization ----
#if DEBUG_OUTPUT == 0
    OutputTexture[id.xy] = float4(albedo, 1.0); return;
#elif DEBUG_OUTPUT == 9
    float3 albedoLvl0 = (material.AlbedoTextureIndex != 0xFFFFFFFF)
        ? gBindlessTextures[NonUniformResourceIndex(material.AlbedoTextureIndex)].SampleLevel(gLinearSampler, uv, 0).rgb
        : float3(0.5, 0.5, 0.5);
    OutputTexture[id.xy] = float4(albedoLvl0, 1.0); return;
#elif DEBUG_OUTPUT == 10
    float3 albedoFixed = (material.AlbedoTextureIndex != 0xFFFFFFFF)
        ? gBindlessTextures[NonUniformResourceIndex(material.AlbedoTextureIndex)].SampleLevel(gLinearSampler, float2(0.5, 0.5), 0).rgb
        : float3(0.5, 0.5, 0.5);
    OutputTexture[id.xy] = float4(albedoFixed, 1.0); return;
#elif DEBUG_OUTPUT == 12
    OutputTexture[id.xy] = float4(gBindlessTextures[0].SampleLevel(gLinearSampler, float2(0.5, 0.5), 0).rgb, 1.0); return;
#endif

    // ---- Full PBR + IBL (DEBUG_OUTPUT == 1) ----
    if (material.NormalTextureIndex != 0xFFFFFFFF)
    {
        float3 normalTS = gBindlessTextures[NonUniformResourceIndex(material.NormalTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).rgb;
        normalTS = normalTS * 2.0 - 1.0;
        N = normalize(mul(normalTS, float3x3(T, B, N)));
    }

    if (material.MetallicTextureIndex != 0xFFFFFFFF)
        metallic = gBindlessTextures[NonUniformResourceIndex(material.MetallicTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).r;

    if (material.RoughnessTextureIndex != 0xFFFFFFFF)
        roughness = gBindlessTextures[NonUniformResourceIndex(material.RoughnessTextureIndex)].SampleGrad(gLinearSampler, uv, ddx_uv, ddy_uv).r;

    float3 V = normalize(gViewPosition - posWS);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float NdotV = max(dot(N, V), 0.0);
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 irradiance = gBindlessCubemaps[NonUniformResourceIndex(gIrradianceMapIndex)].SampleLevel(gLinearSampler, N, 0).rgb;
    float3 diffuse = kD * irradiance * albedo;

    float3 R = reflect(-V, N);
    float3 prefilteredColor = gBindlessCubemaps[NonUniformResourceIndex(gPrefilteredMapIndex)].SampleLevel(gLinearSampler, R, roughness * 4.0).rgb;
    float2 envBRDF = gBindlessTextures[NonUniformResourceIndex(gBRDFLUTIndex)].SampleLevel(gLinearSampler, float2(NdotV, roughness), 0).rg;
    float3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    OutputTexture[id.xy] = float4(diffuse + specular, 1.0);
}