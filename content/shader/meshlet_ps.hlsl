// 添加与AS和MS着色器相同的常量缓冲区声明
cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
};

cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
};

cbuffer cbMeshInfo : register(b2)
{
    uint gMeshletCount;
};

struct VertexOut
{
    float4 PositionHS   : SV_Position;
    float3 PositionVS   : POSITION0;
    float3 Normal       : NORMAL0;
    float2 UV           : TEXCOORD0;
    uint   MeshletIndex : COLOR0;
};

float4 PS(VertexOut input) : SV_TARGET
{
    uint meshletIndex = input.MeshletIndex;
    float3 color = float3(
        float(meshletIndex & 1),
        float(meshletIndex & 3) / 4,
        float(meshletIndex & 7) / 8);

    color = max(color, float3(0.2, 0.1, 0.3));

    return float4(color, 1.0);
}