cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
    float    gScreenWidth;
    float    gScreenHeight;
    float    gRecipTanHalfFovy;
    float    gLODErrorThreshold;  // pixels
    float    gNearPlane;
};

cbuffer cbSkyLight : register(b2)
{
    uint gIrradianceMapIndex;
    uint gPrefilteredMapIndex;
    uint gBRDFLUTIndex;
};

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

// Nanite buffers
StructuredBuffer<NaniteVertex> NaniteVertices : register(t20);
StructuredBuffer<uint> NaniteUniqueVertices : register(t21);
ByteAddressBuffer NaniteLocalIndices : register(t22);
StructuredBuffer<GPUCluster> NaniteClusters : register(t23);
StructuredBuffer<GPUGroupBound> NaniteGroupBounds : register(t24);

// GPU Scene: Primitive transform data
struct FPrimitiveSceneData
{
    float4x4 LocalToWorld;
    float4x4 WorldInvTranspose;
    uint4    PBRTextureIndices[4];  // Albedo, Normal, Metallic, Roughness
};
StructuredBuffer<FPrimitiveSceneData> ScenePrimitives : register(t26);

Texture2D gBindlessTextures[] : register(t30, space0);
TextureCube gBindlessCubemaps[] : register(t0, space1);

SamplerState gLinearSampler : register(s0);

#ifndef PI
#define PI 3.14159265359
#endif

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

struct VertexOut
{
    float4 PositionHS : SV_Position;
    float3 PositionWS : POSITION0;
    float3 Normal     : NORMAL0;
    float3 Tangent    : TANGENT0;
    float3 Bitangent  : BINORMAL0;
    float2 UV0        : TEXCOORD0;
};

struct PrimitiveOut
{
    uint ClusterIndex : COLOR1;
    uint PrimitiveId  : COLOR2;  // From GPU Scene
};

#if 1
// Nanite debug: cluster color visualization
float4 main(VertexOut input, PrimitiveOut primitive) : SV_Target
{
    uint clusterID = primitive.ClusterIndex;

    // Hash function for unique color per cluster (+1 to avoid black for cluster 0)
    float r = frac((clusterID + 1) * 0.618033988749895);
    float g = frac((clusterID + 1) * 0.382694821);
    float b = frac((clusterID + 1) * 0.123456789);

    return float4(r, g, b, 1.0);
}
#else
// PBR with IBL lighting
float4 main(VertexOut input, PrimitiveOut primitive) : SV_Target
{
    // Load primitive data from GPU Scene
    FPrimitiveSceneData primitiveData = ScenePrimitives[primitive.PrimitiveId];

    // PBR material parameters from textures
    float3 albedo = float3(0.5, 0.5, 0.5);
    float metallic = 0.0;
    float roughness = 0.5;
    float ao = 1.0;

    // Sample PBR textures using indices from GPU Scene
    if (primitiveData.PBRTextureIndices[0].x != 0xFFFFFFFF)
    {
        albedo = gBindlessTextures[primitiveData.PBRTextureIndices[0].x].Sample(gLinearSampler, input.UV0).rgb;
    }

    if (primitiveData.PBRTextureIndices[1].x != 0xFFFFFFFF)
    {
        float3 normalTS = gBindlessTextures[primitiveData.PBRTextureIndices[1].x].Sample(gLinearSampler, input.UV0).rgb;
        normalTS = normalTS * 2.0 - 1.0;
        float3x3 TBN = float3x3(input.Tangent, input.Bitangent, input.Normal);
        input.Normal = mul(normalTS, TBN);
    }

    if (primitiveData.PBRTextureIndices[2].x != 0xFFFFFFFF)
    {
        metallic = gBindlessTextures[primitiveData.PBRTextureIndices[2].x].Sample(gLinearSampler, input.UV0).r;
    }

    if (primitiveData.PBRTextureIndices[3].x != 0xFFFFFFFF)
    {
        roughness = gBindlessTextures[primitiveData.PBRTextureIndices[3].x].Sample(gLinearSampler, input.UV0).r;
    }

    float3 N = normalize(input.Normal);
    float3 V = normalize(gViewPosition - input.PositionWS);

    // Calculate F0 (base reflectivity)
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    // Pure IBL lighting only
    float3 color = float3(0.0, 0.0, 0.0);

    // === IBL DIFFUSE ===
    float3 irradiance = gBindlessCubemaps[gIrradianceMapIndex].Sample(gLinearSampler, N).rgb;

    float NdotV = max(dot(N, V), 0.0);
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    float3 diffuse = kD * irradiance * albedo;

    // === IBL SPECULAR ===
    float3 R = reflect(-V, N);

    // Sample prefiltered environment map with appropriate mip level
    float MaxMipLevel = 4.0; // Should match mip levels from C++ (5 levels = 0-4)
    float MipLevel = roughness * MaxMipLevel;
    float3 prefilteredColor = gBindlessCubemaps[gPrefilteredMapIndex].SampleLevel(gLinearSampler, R, MipLevel).rgb;

    // Sample BRDF integration map
    float2 envBRDF = gBindlessTextures[gBRDFLUTIndex].Sample(gLinearSampler, float2(NdotV, roughness)).rg;

    // Split-sum approximation
    float3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    // Combine diffuse and specular
    color = (diffuse + specular) * ao;

    // Return HDR color without tone mapping (now handled in post-process)
    return float4(color, 1.0);
}
#endif