struct VertexOut
{
    float4 PositionHS : SV_Position;
};

struct PrimitiveOut
{
    uint ClusterIndex  : COLOR1;
    uint TriangleIndex : COLOR2;
};

uint4 main(VertexOut input, PrimitiveOut primitive, float3 barycentrics : SV_Barycentrics) : SV_Target
{
    uint packed = (primitive.ClusterIndex << 7) | (primitive.TriangleIndex & 0x7F);
    return uint4(packed + 1, asuint(barycentrics.x), asuint(barycentrics.y), 0);
}
