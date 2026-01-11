cbuffer cbCamera : register(b0)
{
    float4x4 gViewProj;
    float4   gPlanes[6];
    float3   gViewPosition;
    float    gScreenWidth;
    float    gScreenHeight;
    float    gRecipTanHalfFovy;     // 1.0f / tanf(fovy * 0.5f)
    float    gLODErrorThreshold;    // pixels
    float    gNearPlane;
};

cbuffer cbStaticMesh : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4   gBoundingSphere;
    uint4    gPBRTextureIndices[4];  // 16 bytes per element (matches C++ layout)
    uint     gNaniteClusterCount;
    uint     gPadding0;
    uint     gPadding1;
    uint     gPadding2;
};

#define AS_CLUSTERS_PER_GROUP 32    // Each AS group dispatches 32 mesh shaders
#define MS_MAX_VERTICES 64          // Max vertices per mesh shader
#define MS_MAX_PRIMITIVES 126       // Max triangles per mesh shader
#define MS_NUM_THREADS 128          // Mesh shader thread group size

// Nanite structures
struct NaniteVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float2 UV0;
};

struct GPUCluster
{
    uint IndexCount;
    uint UniqueVerticesOffset;
    uint UniqueVerticesCount;
    uint LocalIndicesOffset;
    float3 BoundCenter;
    float BoundRadius;
    int Refined;
    int GroupId;
};

struct GPUGroupBound
{
    float3 Center;
    float Radius;
    float Error;
};

// Nanite buffers: Vertex -> UniqueVertices -> LocalIndices -> Cluster -> GroupBounds
StructuredBuffer<NaniteVertex>  NaniteVertices        : register(t20);
StructuredBuffer<uint>          NaniteUniqueVertices  : register(t21);
ByteAddressBuffer               NaniteLocalIndices    : register(t22);
StructuredBuffer<GPUCluster>    NaniteClusters        : register(t23);
StructuredBuffer<GPUGroupBound> NaniteGroupBounds     : register(t24);

struct Payload
{
    uint ASGroupID;  // AS group ID for cluster index calculation
};

struct VertexOut
{
    float4 PositionHS : SV_Position;
    float3 PositionWS : POSITION0;
    float3 Normal     : NORMAL0;
    float3 Tangent    : TANGENT0;
    float3 Bitangent  : BINORMAL0;
    float2 UV0        : TEXCOORD0;
};

struct PrimitiveOut
{
    uint ClusterIndex : COLOR1;
};

// Compute screen-space projection error for group bounds
// Based on MeshLoader.cpp:19-24 boundsError formula
float ComputeScreenError(GPUGroupBound groupBound, float3 cameraPos, float recipTanHalfFovy, float nearPlane)
{
    float3 v = groupBound.Center - cameraPos;
    float distance = length(v) - groupBound.Radius;
    distance = max(distance, nearPlane);

    // Returns screen height percentage (0-1), multiply by screen height for pixel error
    return groupBound.Error / distance * (recipTanHalfFovy * 0.5f);
}

// Determine if cluster should be rendered (Nanite continuous LOD)
// Based on meshoptimizer design (clusterlod.h:122-125)
// Render cluster if:
// 1. Current group's error > threshold (too coarse without this cluster)
// 2. Refined == -1 OR refined group's error <= threshold (refined too fine, use current)
bool ShouldRenderCluster(GPUCluster cluster, float3 cameraPos, float recipTanHalfFovy)
{
    // Screen-space error threshold (pixels / screen height)
    const float threshold = gLODErrorThreshold / gScreenHeight;

    // Condition 1: Current group error must exceed threshold
    GPUGroupBound myGroup = NaniteGroupBounds[cluster.GroupId];
    float myGroupError = ComputeScreenError(myGroup, cameraPos, recipTanHalfFovy, gNearPlane);

    if (myGroupError <= threshold)
        return false;  // Current group error below threshold, skip this cluster

    // Condition 2a: Original geometry (highest detail level, refined == -1)
    if (cluster.Refined == -1)
        return true;

    // Condition 2b: Refined group error <= threshold
    GPUGroupBound refinedGroup = NaniteGroupBounds[cluster.Refined];
    float refinedGroupError = ComputeScreenError(refinedGroup, cameraPos, recipTanHalfFovy, gNearPlane);

    return refinedGroupError <= threshold;  // Higher detail not needed, render this cluster
}

[numthreads(MS_NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void main(
    uint gid : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint3 dtid : SV_DispatchThreadID,
    in payload Payload payloadData,
    out vertices VertexOut vertices[MS_MAX_VERTICES],
    out primitives PrimitiveOut primitives[MS_MAX_PRIMITIVES],
    out indices uint3 triangles[MS_MAX_PRIMITIVES])
{
    uint clusterIndex = payloadData.ASGroupID * AS_CLUSTERS_PER_GROUP + gid;

    uint vertexCount = 0;
    uint triangleCount = 0;

    if (clusterIndex < gNaniteClusterCount)
    {
        GPUCluster cluster = NaniteClusters[clusterIndex];

        // Nanite continuous LOD: cluster selection
        bool shouldRender = ShouldRenderCluster(cluster, gViewPosition, gRecipTanHalfFovy);

        if (shouldRender)
        {
            vertexCount = min(MS_MAX_VERTICES, cluster.UniqueVerticesCount);
            triangleCount = min(MS_MAX_PRIMITIVES, cluster.IndexCount / 3);
        }
    }

    SetMeshOutputCounts(vertexCount, triangleCount);

    if (clusterIndex < gNaniteClusterCount && vertexCount > 0)
    {
        GPUCluster cluster = NaniteClusters[clusterIndex];

        // Output vertices
        for (uint i = gtid.x; i < vertexCount; i += MS_NUM_THREADS)
        {
            uint globalVertexIndex = NaniteUniqueVertices[cluster.UniqueVerticesOffset + i];
            NaniteVertex nv = NaniteVertices[globalVertexIndex];

            VertexOut vout;
            vout.PositionWS = mul(float4(nv.Position, 1), gWorld).xyz;
            vout.PositionHS = mul(float4(vout.PositionWS, 1), gViewProj);
            vout.Normal = normalize(mul(float4(nv.Normal, 0), gWorldInvTranspose).xyz);
            vout.Tangent = normalize(mul(float4(nv.Tangent, 0), gWorldInvTranspose).xyz);
            vout.Bitangent = normalize(cross(vout.Tangent, vout.Normal));
            vout.UV0 = nv.UV0;

            vertices[i] = vout;
        }

        // Output triangles
        for (uint triIdx = gtid.x; triIdx < triangleCount; triIdx += MS_NUM_THREADS)
        {
            uint indexOffset = cluster.LocalIndicesOffset + triIdx * 3;

            // Read 3 local indices (byte-packed)
            uint byteAddr0 = indexOffset;
            uint localIdx0 = (NaniteLocalIndices.Load(byteAddr0 & ~3) >> ((byteAddr0 & 3) * 8)) & 0xFF;

            uint byteAddr1 = indexOffset + 1;
            uint localIdx1 = (NaniteLocalIndices.Load(byteAddr1 & ~3) >> ((byteAddr1 & 3) * 8)) & 0xFF;

            uint byteAddr2 = indexOffset + 2;
            uint localIdx2 = (NaniteLocalIndices.Load(byteAddr2 & ~3) >> ((byteAddr2 & 3) * 8)) & 0xFF;

            triangles[triIdx] = uint3(localIdx0, localIdx1, localIdx2);
            primitives[triIdx].ClusterIndex = clusterIndex;
        }
    }
}
