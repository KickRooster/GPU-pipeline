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
    // Visibility Buffer encoding (32-bit):
    // ClusterIndex: 22 bits (supports 4M clusters)
    // TriangleIndex: 10 bits (supports 1024 triangles/patch)
    // Total: 32 bits
    uint packed = (primitive.ClusterIndex << 10) | (primitive.TriangleIndex & 0x3FF);
    return uint4(packed + 1, asuint(barycentrics.x), asuint(barycentrics.y), 0);
}
