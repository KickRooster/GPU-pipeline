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

StructuredBuffer<Vertex>     Vertices            : register(t0);
StructuredBuffer<Meshlet>    Meshlets            : register(t1);
StructuredBuffer<uint>       UniqueVertexIndices : register(t2);
StructuredBuffer<uint>       MeshletTriangles    : register(t3);
StructuredBuffer<BoundsData> MeshletBounds       : register(t4);

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

    // Transform cone axis to world space (no need to normalize here, will normalize in the formula)
    float3 axis = mul(float4(b.cone_axis, 0), world).xyz;
    
    // Use meshoptimizer's recommended formula that uses bounding sphere center
    // instead of cone apex for better numerical stability
    float3 centerToCamera = viewPos - center.xyz;
    float distanceToCenter = length(centerToCamera);
    
    // Avoid division by zero when camera is at sphere center
    if (distanceToCenter < 1e-6)
        return true;
    
    // meshoptimizer's formula: dot(center - camera_position, cone_axis) >= cone_cutoff * length(center - camera_position) + radius
    // Rearranged: dot(camera_position - center, cone_axis) <= -cone_cutoff * distance - radius
    if (dot(centerToCamera, normalize(axis)) <= -b.cone_cutoff * distanceToCenter - radius)
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
    
    if (dtid < gMeshletCount)
    {
        //  XXX:    Unsupport scaling now.
        float scale = 1.0f;
        
        visible = IsVisible(MeshletBounds[dtid], gWorld, scale, gViewPosition);
    }

    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        s_Payload.MeshletIndices[index] = dtid;
    }
    
    uint visibleCount = WaveActiveCountBits(visible);
    
    // 分发所需数量的MS线程组来渲染可见的meshlets
    // 注意：虽然每个线程都执行这行代码，但系统会处理成只有一次实际的dispatch
    DispatchMesh(visibleCount, 1, 1, s_Payload);
} 