cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
};

cbuffer cbStaticMeshActor : register(b1)
{
    float4x4 gWorld;
};

struct Meshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};

StructuredBuffer<Meshlet> Meshlets : register(t1);

[numthreads(32, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{
    // 计算此线程组处理的meshlet范围
    uint meshletCount = 0;
    uint meshletIndices[32];
    
    // 这里我们简单地每个线程组处理连续的32个meshlet
    uint startIndex = gid * 32;
    uint endIndex = startIndex + 32;
    
    // 这里简单实现一个粗略的剔除
    for (uint i = startIndex + gtid.x; i < endIndex; i += 32)
    {
        // 不进行剔除，所有meshlet都通过
        // 在实际应用中，你会在这里添加视锥体、八叉树等剔除逻辑
        // 例如，这里可以检查meshlet的包围球是否与视锥体相交
        
        // 将通过剔除的meshlet索引添加到数组
        uint idx = meshletCount++;
        meshletIndices[idx] = i;
    }
    
    // 设置要分派的网格着色器组数量，每个meshlet一个组
    DispatchMesh(meshletCount, 1, 1, meshletIndices);
} 