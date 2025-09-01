cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
    float    gRecipTanHalfFovy;  // 1.0f / tanf(fovy * 0.5f)
    uint     gLODCount;
};

cbuffer cbStaticMesh : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4   gBoundingSphere;
    uint     gMeshletCounts[4];
    uint     gPBRTextureIndices[4];
};

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

StructuredBuffer<Vertex>     LOD0_Vertices            : register(t0);
StructuredBuffer<Meshlet>    LOD0_Meshlets            : register(t1);
StructuredBuffer<uint>       LOD0_UniqueVertexIndices : register(t2);
StructuredBuffer<uint>       LOD0_MeshletTriangles    : register(t3);
StructuredBuffer<BoundsData> LOD0_MeshletBounds       : register(t4);

StructuredBuffer<Vertex>     LOD1_Vertices            : register(t5);
StructuredBuffer<Meshlet>    LOD1_Meshlets            : register(t6);
StructuredBuffer<uint>       LOD1_UniqueVertexIndices : register(t7);
StructuredBuffer<uint>       LOD1_MeshletTriangles    : register(t8);
StructuredBuffer<BoundsData> LOD1_MeshletBounds       : register(t9);

StructuredBuffer<Vertex>     LOD2_Vertices            : register(t10);
StructuredBuffer<Meshlet>    LOD2_Meshlets            : register(t11);
StructuredBuffer<uint>       LOD2_UniqueVertexIndices : register(t12);
StructuredBuffer<uint>       LOD2_MeshletTriangles    : register(t13);
StructuredBuffer<BoundsData> LOD2_MeshletBounds       : register(t14);

StructuredBuffer<Vertex>     LOD3_Vertices            : register(t15);
StructuredBuffer<Meshlet>    LOD3_Meshlets            : register(t16);
StructuredBuffer<uint>       LOD3_UniqueVertexIndices : register(t17);
StructuredBuffer<uint>       LOD3_MeshletTriangles    : register(t18);
StructuredBuffer<BoundsData> LOD3_MeshletBounds       : register(t19);

BoundsData GetMeshletBounds(uint lodIndex, uint meshletIndex)
{
    if (lodIndex == 0)
    {
        return LOD0_MeshletBounds[meshletIndex];
    }
    
    if (lodIndex == 1)
    {
        return LOD1_MeshletBounds[meshletIndex];
    }
    
    if (lodIndex == 2)
    {
        return LOD2_MeshletBounds[meshletIndex];
    }

    return LOD3_MeshletBounds[meshletIndex];
}

// Microsoft DynamicLOD style LOD computation
uint ComputeLOD(float3 center, float radius, float3 viewPos, float recipTanHalfFovy)
{
    float3 v = viewPos - center;
    float distance = length(v);
    
    // Avoid division by zero and numerical issues
    if (distance <= radius)
        return 0; // Camera is inside the bounding sphere, use highest detail
    
    // Microsoft's formula: size = recipTanHalfFovy * r / sqrt(dot(v, v) - r * r)
    float size = recipTanHalfFovy * radius / sqrt(distance * distance - radius * radius);
    
    // Map screen size to LOD level (adjusted for better transitions)
    if (size > 0.5) return 0;      // Large on screen -> red (LOD 0) 
    if (size > 0.25) return 1;     // Medium-large -> green (LOD 1)
    if (size > 0.1) return 2;      // Medium -> blue (LOD 2)
    return 3;                      // Small -> yellow (LOD 3)
}

struct Payload
{
    uint MeshletIndices[32];
    uint LODLevel;  // Pass LOD level to mesh shader
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
    bool visible = true;
    
    // Compute LOD based on screen-space coverage (Microsoft DynamicLOD style)
    float3 worldCenter = mul(float4(gBoundingSphere.xyz, 1), gWorld).xyz;
    uint lodIndex = ComputeLOD(worldCenter, gBoundingSphere.w, gViewPosition, gRecipTanHalfFovy);
    
    if (dtid < gMeshletCounts[lodIndex])
    {
        //  XXX:    Unsupport scaling now.
        float scale = 1.0f;
        
        // 通过函数获取对应LOD的bounds数据
        visible = IsVisible(GetMeshletBounds(lodIndex, dtid), gWorld, scale, gViewPosition);
    }

    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        s_Payload.MeshletIndices[index] = dtid;
    }
    
    uint visibleCount = WaveActiveCountBits(visible);
    
    // Pass LOD level to mesh shader
    s_Payload.LODLevel = lodIndex;
    
    DispatchMesh(visibleCount, 1, 1, s_Payload);
} 