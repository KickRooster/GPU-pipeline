//***************************************************************************************
// meshlet_ps.hlsl
// 
// Pixel shader for meshlet rendering
//***************************************************************************************

// 顶点着色器输出/像素着色器输入结构
struct VertexOut
{
    float4 PositionHS   : SV_Position;
    float3 PositionVS   : POSITION0;
    float3 Normal       : NORMAL0;
    float2 UV           : TEXCOORD0;
    uint   MeshletIndex : COLOR0;    // 使用MeshletIndex
};

// Pixel shader implementation
float4 PS(VertexOut input) : SV_TARGET
{
    // 直接从meshlet索引生成颜色
    uint meshletIndex = input.MeshletIndex;
    float3 color = float3(
        float(meshletIndex & 1),
        float(meshletIndex & 3) / 4,
        float(meshletIndex & 7) / 8);

    color = max(color, float3(0.2, 0.1, 0.3));

    return float4(color, 1.0);
} 