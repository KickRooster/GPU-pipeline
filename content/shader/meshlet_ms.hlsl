cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
    float    gPadding;
};

cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
};

cbuffer cbMeshInfo : register(b2)
{
    uint gMeshletCount;
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

struct Payload
{
    uint MeshletIndices[32];
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
    
    float3 normalW = normalize(mul(v.Normal, (float3x3)gWorldInvTranspose));
    
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
    uint groupID : SV_GroupID,
    uint3 groupThreadID : SV_GroupThreadID,
    uint dispatchThreadID : SV_DispatchThreadID,
    in payload Payload payload,
    out vertices VertexOut verts[64],     // identical with MaxVertexCountPerMeshlet in MeshLoader::GenerateMeshletData()
    out indices uint3 tris[124]           // identical with MaxTriangleCountPerMeshlet in MeshLoader::GenerateMeshletData()
)
{
    // 从payload获取当前线程组应该处理的meshlet索引
    uint meshletIndex = payload.MeshletIndices[groupID];
    
    // 加载对应的meshlet
    Meshlet m = Meshlets[meshletIndex];
    
    SetMeshOutputCounts(m.VertexCount, m.TriangleCount);

    if (groupThreadID.x < m.TriangleCount)
    {
        tris[groupThreadID.x] = GetPrimitive(m, groupThreadID.x);
    }

    if (groupThreadID.x < m.VertexCount)
    {
        uint vertexIndex = GetVertexIndex(m, groupThreadID.x);
        verts[groupThreadID.x] = GetVertexAttributes(meshletIndex, vertexIndex);
    }
}