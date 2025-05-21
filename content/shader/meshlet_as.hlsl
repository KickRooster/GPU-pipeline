cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
};

cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
};

// 添加meshlet信息常量缓冲区
cbuffer cbMeshInfo : register(b2)
{
    uint gMeshletCount;
};

// Meshlet结构定义
struct Meshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};

struct Vertex
{
    float3 Position;
    float3 Normal;
    float4 Color;
    float2 UV0;
};

// 添加与MS shader相同的资源绑定声明
StructuredBuffer<Vertex>  Vertices            : register(t0);
StructuredBuffer<Meshlet> Meshlets            : register(t1);
StructuredBuffer<uint>    UniqueVertexIndices : register(t2);
StructuredBuffer<uint>    MeshletTriangles    : register(t3);

// 定义传递给MS的payload结构
struct Payload
{
    uint MeshletIndices[32];
};

// The groupshared payload data to export to dispatched mesh shader threadgroups
groupshared Payload s_Payload;

[numthreads(32, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{
    bool visible = false;
    
    // 检查是否在有效的meshlet范围内
    if (dtid < gMeshletCount)
    {
        // 在简化版本中，所有meshlet都是可见的
        visible = true;
    }

    // 将可见的meshlet索引压缩到export payload数组中
    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        s_Payload.MeshletIndices[index] = dtid;
    }
    
    // 计算可见的meshlet总数
    uint visibleCount = WaveActiveCountBits(visible);
    
    // 分发所需数量的MS线程组来渲染可见的meshlets
    // 注意：虽然每个线程都执行这行代码，但系统会处理成只有一次实际的dispatch
    DispatchMesh(visibleCount, 1, 1, s_Payload);
} 