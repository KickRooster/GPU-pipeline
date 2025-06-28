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

struct VertexOut
{
    float4 PositionHS : SV_Position;
    float3 PositionWS : POSITION0;
    float3 Normal     : NORMAL0;
    float4 Color      : COLOR0;
    float2 UV0        : TEXCOORD0;
};

struct PrimitiveOut
{
    uint MeshletIndex : COLOR1;
};

float4 main(VertexOut input, PrimitiveOut primitive) : SV_Target
{
    float3 finalColor = float3(1.0, 1.0, 1.0);
    
    if (gPBRTextureIndices[0] != 0xFFFFFFFF)
    {
        float4 albedo = gBindlessTextures[gPBRTextureIndices[0]].Sample(gLinearSampler, input.UV0);
        finalColor = albedo.rgb;

        return float4(finalColor, 1.0);
    }
    else
    {
        return float4(input.Normal * 0.5 + 0.5, 1.0);
    }
}