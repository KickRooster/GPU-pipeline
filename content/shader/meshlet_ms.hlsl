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

struct Vertex
{
    float3 Position;
    float3 Normal;
    float4 Color;
    float2 UV0;
};

struct Meshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};

StructuredBuffer<Vertex>  LOD0_Vertices            : register(t0);
StructuredBuffer<Meshlet> LOD0_Meshlets            : register(t1);
StructuredBuffer<uint>    LOD0_UniqueVertexIndices : register(t2);
StructuredBuffer<uint>    LOD0_MeshletTriangles    : register(t3);

StructuredBuffer<Vertex>  LOD1_Vertices            : register(t5);
StructuredBuffer<Meshlet> LOD1_Meshlets            : register(t6);
StructuredBuffer<uint>    LOD1_UniqueVertexIndices : register(t7);
StructuredBuffer<uint>    LOD1_MeshletTriangles    : register(t8);

StructuredBuffer<Vertex>  LOD2_Vertices            : register(t10);
StructuredBuffer<Meshlet> LOD2_Meshlets            : register(t11);
StructuredBuffer<uint>    LOD2_UniqueVertexIndices : register(t12);
StructuredBuffer<uint>    LOD2_MeshletTriangles    : register(t13);

StructuredBuffer<Vertex>  LOD3_Vertices            : register(t15);
StructuredBuffer<Meshlet> LOD3_Meshlets            : register(t16);
StructuredBuffer<uint>    LOD3_UniqueVertexIndices : register(t17);
StructuredBuffer<uint>    LOD3_MeshletTriangles    : register(t18);

Meshlet GetMeshlet(uint lodIndex, uint meshletIndex)
{
    if (lodIndex == 0) return LOD0_Meshlets[meshletIndex];
    if (lodIndex == 1) return LOD1_Meshlets[meshletIndex];
    if (lodIndex == 2) return LOD2_Meshlets[meshletIndex];
    return LOD3_Meshlets[meshletIndex];
}

Vertex GetVertex(uint lodIndex, uint vertexIndex)
{
    if (lodIndex == 0) return LOD0_Vertices[vertexIndex];
    if (lodIndex == 1) return LOD1_Vertices[vertexIndex];
    if (lodIndex == 2) return LOD2_Vertices[vertexIndex];
    return LOD3_Vertices[vertexIndex];
}

uint GetUniqueVertexIndex(uint lodIndex, uint index)
{
    if (lodIndex == 0) return LOD0_UniqueVertexIndices[index];
    if (lodIndex == 1) return LOD1_UniqueVertexIndices[index];
    if (lodIndex == 2) return LOD2_UniqueVertexIndices[index];
    return LOD3_UniqueVertexIndices[index];
}

uint GetMeshletTriangle(uint lodIndex, uint index)
{
    if (lodIndex == 0) return LOD0_MeshletTriangles[index];
    if (lodIndex == 1) return LOD1_MeshletTriangles[index];
    if (lodIndex == 2) return LOD2_MeshletTriangles[index];
    return LOD3_MeshletTriangles[index];
}

// Microsoft DynamicLOD style LOD color visualization
float4 GetLODColor(uint lodIndex)
{
    if (lodIndex == 0) return float4(1.0, 1.0, 1.0, 1.0);      // Red - LOD 0 (highest detail)
    if (lodIndex == 1) return float4(0.75, 0.75, 0.75, 1.0);      // Green - LOD 1
    if (lodIndex == 2) return float4(0.5, 0.5, 0.5, 1.0);      // Blue - LOD 2
    return float4(0.25, 0.25, 0.25, 1.0);                         // Yellow - LOD 3 (lowest detail)
}

struct Payload
{
    uint MeshletIndices[32];
    uint LODLevel;  // LOD level from amplification shader
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

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main(
    uint gid : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint3 dtid : SV_DispatchThreadID,
    in payload Payload payloadData,
    out vertices VertexOut vertices[64],
    out primitives PrimitiveOut primitives[124],
    out indices uint3 triangles[124])
{
    uint meshletIndex = payloadData.MeshletIndices[gid];
    
    // Use LOD level computed by amplification shader
    uint lodIndex = payloadData.LODLevel;
    
    Meshlet meshlet = GetMeshlet(lodIndex, meshletIndex);
    
    SetMeshOutputCounts(meshlet.VertexCount, meshlet.TriangleCount);

    // Output vertices
    for (uint i = gtid.x; i < meshlet.VertexCount; i += 128)
    {
        uint vertexIndex = GetUniqueVertexIndex(lodIndex, meshlet.VertexOffset + i);
        Vertex v = GetVertex(lodIndex, vertexIndex);

        VertexOut vout;
        vout.PositionWS = mul(float4(v.Position, 1), gWorld).xyz;
        vout.PositionHS = mul(float4(vout.PositionWS, 1), gViewProj);
        vout.Normal = normalize(mul(float4(v.Normal, 0), gWorldInvTranspose).xyz);
        
        float4 lodColor = GetLODColor(lodIndex);
        
        uint meshletHash = (meshletIndex + 1) * 0x9e3779b9u;
        float3 meshletColor = float3(
            float((meshletHash >> 0) & 0xFF) / 255.0,
            float((meshletHash >> 8) & 0xFF) / 255.0, 
            float((meshletHash >> 16) & 0xFF) / 255.0
        );
        
        vout.Color = float4(lodColor.rgb * meshletColor * (vout.Normal * 0.5 + 0.5), 1.0);
        
        vout.UV0 = v.UV0;

        vertices[i] = vout;
    }

    // Output primitives
    for (uint i = gtid.x; i < meshlet.TriangleCount; i += 128)
    {
        uint indexOffset = meshlet.TriangleOffset + i * 3;
        triangles[i] = uint3(
            GetMeshletTriangle(lodIndex, indexOffset + 0),
            GetMeshletTriangle(lodIndex, indexOffset + 1), 
            GetMeshletTriangle(lodIndex, indexOffset + 2));
        
        primitives[i].MeshletIndex = meshletIndex;
    }
}