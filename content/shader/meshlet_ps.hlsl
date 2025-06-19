cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
};

cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    uint gMeshletCounts[4];
};

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
    return float4(input.Normal * 0.5 + 0.5, 1.0);
}