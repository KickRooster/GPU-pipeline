struct VertexOut
{
    float4 PositionHS : SV_Position;
    float2 TerrainUV  : TEXCOORD0;
};

struct PrimitiveOut
{
    uint ClusterIndex  : COLOR1;
    uint TriangleIndex : COLOR2;
};

uint4 main(VertexOut input, PrimitiveOut primitive, float3 barycentrics : SV_Barycentrics) : SV_Target
{
    uint packed = (primitive.ClusterIndex << 10) | (primitive.TriangleIndex & 0x3FF);

    // Pack TerrainUV as UNORM16 pair: [0,1] -> [0,65535]
    uint u16x = (uint)(saturate(input.TerrainUV.x) * 65535.0 + 0.5);
    uint u16y = (uint)(saturate(input.TerrainUV.y) * 65535.0 + 0.5);
    uint packedUV = (u16x & 0xFFFF) | (u16y << 16);

    return uint4(packed + 1, asuint(barycentrics.x), asuint(barycentrics.y), packedUV);
}