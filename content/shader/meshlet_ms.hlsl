//***************************************************************************************
// meshlet_ms.hlsl
// 
// Mesh shader for meshlet rendering
//***************************************************************************************

// 相机常量缓冲区 (b0) - 与vertex shader pipeline保持一致
cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
};

// 对象常量缓冲区 (b1) - 与vertex shader pipeline保持一致
cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
};

struct Vertex
{
    float3 Position;
    float3 Normal;
    float4 Color;     // 添加颜色支持
    float2 UV0;       // 添加UV支持
};

struct VertexOut
{
    float4 PositionHS   : SV_Position;
    float3 PositionVS   : POSITION0;
    float3 Normal       : NORMAL0;
    float2 UV           : TEXCOORD0;
    uint   MeshletIndex : COLOR0;    // 使用MeshletIndex
};

// 确保与 CreateMeshletDataProxyBuffer 中的 Meshlet 结构定义一致
struct Meshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};

// 资源绑定
StructuredBuffer<Vertex>  Vertices            : register(t0);
StructuredBuffer<Meshlet> Meshlets            : register(t1);
StructuredBuffer<uint>    UniqueVertexIndices : register(t2);
StructuredBuffer<uint>    MeshletTriangles    : register(t3); // 修改为StructuredBuffer<uint>

// Data Loaders
uint3 GetPrimitive(Meshlet m, uint index)
{
    // 三角形索引的基础偏移量
    uint baseIndex = m.TriangleOffset + index * 3;
    
    // 直接读取三个连续的32位索引
    uint idx0 = MeshletTriangles[baseIndex];
    uint idx1 = MeshletTriangles[baseIndex + 1];
    uint idx2 = MeshletTriangles[baseIndex + 2];
    
    return uint3(idx0, idx1, idx2);
}

uint GetVertexIndex(Meshlet m, uint localIndex)
{
    // 直接访问32位索引
    return UniqueVertexIndices[m.VertexOffset + localIndex];
}

VertexOut GetVertexAttributes(uint meshletIndex, uint vertexIndex)
{
    Vertex v = Vertices[vertexIndex];

    // 计算世界空间位置
    float4 posW = mul(float4(v.Position, 1), gWorld);
    
    // 计算法线
    float3x3 worldInvTranspose = transpose((float3x3)gWorld);
    float3 normalW = normalize(mul(v.Normal, worldInvTranspose));
    
    // 输出顶点
    VertexOut vout;
    vout.PositionHS = mul(posW, gViewProj);
    vout.PositionVS = posW.xyz; // 保存世界空间位置
    vout.Normal = normalW;
    
    vout.UV = v.UV0;
    // 传递meshlet索引作为颜色
    vout.MeshletIndex = meshletIndex;

    return vout;
}

// Mesh Shader 入口点
[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    out vertices VertexOut verts[64],
    out indices uint3 tris[124]  // 修改为124以匹配MaxTriangleCountPerMeshlet
)
{
    // 使用gid作为meshlet索引
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