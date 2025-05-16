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
    uint   MeshletIndex : COLOR0;
};

// Pixel shader implementation
float4 PS(VertexOut input) : SV_TARGET
{
    // Simple lighting calculation
    float3 lightDir = normalize(float3(0.5, 0.5, -0.5));
    float3 normal = normalize(input.Normal);
    float3 color = float3(0.8, 0.8, 0.8); // Base color
    
    // Colorize by meshlet index
    uint hash = input.MeshletIndex * 0x1f3d5b79;
    hash = ((hash >> 5) ^ hash) * 0x5171f31d;
    hash = ((hash >> 7) ^ hash) * 0x7841d299;
    
    float3 meshletColor = float3(
        ((hash >> 0) & 0xFF) / 255.0,
        ((hash >> 8) & 0xFF) / 255.0,
        ((hash >> 16) & 0xFF) / 255.0
    );
    
    color = meshletColor;
    
    // Ambient and diffuse components
    float3 ambient = color * 0.2;
    float3 diffuse = color * max(dot(normal, lightDir), 0.0);
    
    return float4(ambient + diffuse, 1.0);
} 