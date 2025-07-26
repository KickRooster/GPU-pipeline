cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
    float    gRecipTanHalfFovy;  // 1.0f / tanf(fovy * 0.5f)
    uint     gLODCount;
};

cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4   gBoundingSphere;
    uint     gMeshletCounts[4];
    uint     gPBRTextureIndices[4];
};

Texture2D gBindlessTextures[] : register(t20, space0);

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

struct VertexOut
{
    float4 PositionHS : SV_Position;
    float3 PositionWS : POSITION0;
    float3 Normal     : NORMAL0;
    float3 Tangent    : TANGENT0;
    float3 Bitangent  : BINORMAL0;
    float4 Color      : COLOR0;
    float2 UV0        : TEXCOORD0;
};

struct PrimitiveOut
{
    uint MeshletIndex : COLOR1;
};

float4 main(VertexOut input, PrimitiveOut primitive) : SV_Target
{
    // PBR material parameters from textures
    float3 albedo = float3(0.5, 0.5, 0.5); // Default albedo
    float metallic = 0.0;
    float roughness = 0.5;
    
    if (gPBRTextureIndices[0] != 0xFFFFFFFF)
    {
        albedo = gBindlessTextures[gPBRTextureIndices[0]].Sample(gLinearSampler, input.UV0).rgb;
    }
    
    if (gPBRTextureIndices[1] != 0xFFFFFFFF)
    {
        // Sample normal map and transform to world space
        float3 normalTS = gBindlessTextures[gPBRTextureIndices[1]].Sample(gLinearSampler, input.UV0).rgb;
        normalTS = normalTS * 2.0 - 1.0;
        float3x3 TBN = float3x3(input.Tangent, input.Bitangent, input.Normal);
        input.Normal = mul(normalTS, TBN);
    }
    
    if (gPBRTextureIndices[2] != 0xFFFFFFFF)
    {
        metallic = gBindlessTextures[gPBRTextureIndices[2]].Sample(gLinearSampler, input.UV0).r;
    }
    
    if (gPBRTextureIndices[3] != 0xFFFFFFFF)
    {
        roughness = gBindlessTextures[gPBRTextureIndices[3]].Sample(gLinearSampler, input.UV0).r;
    }
    
    float3 N = normalize(input.Normal);
    float3 V = normalize(gViewPosition - input.PositionWS);
    
    // Calculate F0 (base reflectivity)
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    
    // Direct lighting from virtual light
    float3 L = normalize(float3(0.0, -1.0, 0.0)); // Virtual light direction
    float3 H = normalize(V + L);
    float3 lightColor = float3(1.0, 1.0, 1.0); // White light
    
    // Calculate Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    // Specular BRDF (Cook-Torrance)
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;
    
    // Energy conservation: kS = F, kD = 1 - F
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;
    
    // Diffuse BRDF (Lambert)
    float3 diffuse = kD * albedo / PI;
    
    // Calculate outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);
    float3 Lo = (diffuse + specular) * lightColor * NdotL;
    
    return float4(Lo, 1.0);
}