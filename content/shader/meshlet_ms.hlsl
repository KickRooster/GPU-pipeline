cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
};

cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
};

struct Vertex
{
    float3 Position;
    float3 Normal;
    float4 Color;
    float2 UV0;
};

struct VertexOut
{
    float4 PositionHS   : SV_Position;
    float3 PositionVS   : POSITION0;
    float3 Normal       : NORMAL0;
    float2 UV           : TEXCOORD0;
    uint   MeshletIndex : COLOR0;
};

struct Meshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};

StructuredBuffer<Vertex>  Vertices            : register(t0);
StructuredBuffer<Meshlet> Meshlets            : register(t1);
StructuredBuffer<uint>    UniqueVertexIndices : register(t2);
StructuredBuffer<uint>    MeshletTriangles    : register(t3);

uint3 GetPrimitive(Meshlet m, uint index)
{
    uint baseIndex = m.TriangleOffset + index * 3;
    
    uint idx0 = MeshletTriangles[baseIndex];
    uint idx1 = MeshletTriangles[baseIndex + 1];
    uint idx2 = MeshletTriangles[baseIndex + 2];
    
    return uint3(idx0, idx1, idx2);
}

uint GetVertexIndex(Meshlet m, uint localIndex)
{
    return UniqueVertexIndices[m.VertexOffset + localIndex];
}

VertexOut GetVertexAttributes(uint meshletIndex, uint vertexIndex)
{
    Vertex v = Vertices[vertexIndex];

    float4 posW = mul(float4(v.Position, 1), gWorld);
    
    float3x3 worldInvTranspose = transpose((float3x3)gWorld);
    float3 normalW = normalize(mul(v.Normal, worldInvTranspose));
    
    VertexOut vout;
    vout.PositionHS = mul(posW, gViewProj);
    vout.PositionVS = posW.xyz;
    vout.Normal = normalW;
    
    vout.UV = v.UV0;
    vout.MeshletIndex = meshletIndex;

    return vout;
}

[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    out vertices VertexOut verts[64],   //  identical with MaxVertexCountPerMeshlet in MeshLoader::GenerateMeshletData()
    out indices uint3 tris[124]         //  identical with MaxTriangleCountPerMeshlet in MeshLoader::GenerateMeshletData()
)
{
    uint meshletIndex = gid;
    Meshlet m = Meshlets[meshletIndex];
    
    SetMeshOutputCounts(m.VertexCount, m.TriangleCount);

    if (gtid < m.TriangleCount)
    {
        tris[gtid] = GetPrimitive(m, gtid);
    }

    if (gtid < m.VertexCount)
    {
        uint vertexIndex = GetVertexIndex(m, gtid);
        verts[gtid] = GetVertexAttributes(meshletIndex, vertexIndex);
    }
}