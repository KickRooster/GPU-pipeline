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
};

// 添加meshlet信息常量缓冲区
cbuffer cbMeshInfo : register(b2)
{
    uint gMeshletCount;
};

// Bounds data structure exactly matching meshopt_Bounds memory layout
struct BoundsData
{
    float3 center;            // center[3]
    float  radius;            // radius
    float3 cone_apex;         // cone_apex[3]  
    float3 cone_axis;         // cone_axis[3]
    float  cone_cutoff;       // cone_cutoff
    // Note: meshopt_Bounds also has cone_axis_s8[3] + cone_cutoff_s8 (4 bytes)
    // but we don't use them, just need to account for the padding
    uint   padding;           // 4 bytes padding for cone_axis_s8[3] + cone_cutoff_s8
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
StructuredBuffer<Vertex>     Vertices            : register(t0);
StructuredBuffer<Meshlet>    Meshlets            : register(t1);
StructuredBuffer<uint>       UniqueVertexIndices : register(t2);
StructuredBuffer<uint>       MeshletTriangles    : register(t3);
StructuredBuffer<BoundsData> MeshletBounds       : register(t4);

// 定义传递给MS的payload结构
struct Payload
{
    uint MeshletIndices[32];
};

// The groupshared payload data to export to dispatched mesh shader threadgroups
groupshared Payload s_Payload;

// Check if normal cone is degenerate (spread wider than hemisphere)
bool IsConeDegenerate(BoundsData b)
{
    return b.cone_cutoff <= 0.0; // If cutoff <= 0, cone is degenerate
}

// Visibility testing function implementing frustum and backface culling
bool IsVisible(BoundsData b, float4x4 world, float scale, float3 viewPos)
{
    // Transform bounding sphere center to world space
    float4 center = mul(float4(b.center, 1), world);
    float radius = b.radius * scale;

    // Frustum culling: test bounding sphere against all 6 frustum planes
    for (int i = 0; i < 6; ++i)
    {
        if (dot(center, gPlanes[i]) < -radius)
        {
            return false; // Outside frustum, cull this meshlet
        }
    }

    // Backface culling using normal cone
    if (IsConeDegenerate(b))
        return true; // Cone is degenerate - spread is wider than a hemisphere

    // Transform cone axis to world space
    float3 axis = normalize(mul(float4(b.cone_axis, 0), world)).xyz;

    // Transform cone apex to world space and account for scaling
    float3 apex = mul(float4(b.cone_apex, 1), world).xyz;

    // Calculate view direction from apex to camera
    float3 view = normalize(viewPos - apex);

    // The cone cutoff in meshoptimizer stores sin(angle) for the normal cone
    // For backface culling, we test against the negative axis direction
    // If dot(view, -axis) > cone_cutoff, all triangles in this meshlet are backfacing
    if (dot(view, -axis) > b.cone_cutoff)
    {
        return false; // All triangles are backfacing, cull this meshlet
    }

    // All tests passed - it will merit pixels
    return true;
}

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
        // 临时使用固定scale来排除scale计算问题
        float scale = 1.0f;
        
        // 执行真正的可见性测试
        visible = IsVisible(MeshletBounds[dtid], gWorld, scale, gViewPosition);
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