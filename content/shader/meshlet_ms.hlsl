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

// Root constant for total cluster count (GPU-Driven rendering)
cbuffer cbClusterCount : register(b1)
{
    uint gNaniteClusterCount;
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
    uint PrimitiveId;            // Index to ScenePrimitiveBuffer (GPU Scene)
    uint IndexCount;
    uint UniqueVerticesOffset;
    uint UniqueVerticesCount;
    uint LocalIndicesOffset;
    float3 BoundCenter;
    float BoundRadius;
    int Refined;                 // Index to more detailed group (-1 = leaf, for LOD selection)
    int GroupId;
    uint TriangleMaterialIDsOffset;     // Offset in TriangleMaterialIDs buffer
};

// Nanite buffers: Vertex -> UniqueVertices -> LocalIndices -> Cluster
StructuredBuffer<NaniteVertex>  NaniteVertices        : register(t20);
StructuredBuffer<uint>          NaniteUniqueVertices  : register(t21);
ByteAddressBuffer               NaniteLocalIndices    : register(t22);
StructuredBuffer<GPUCluster>    NaniteClusters        : register(t23);

// GPU Scene: Primitive transform data (Root Descriptor, t26, UE5-style)
struct FPrimitiveSceneData
{
    float4x4 LocalToWorld;
    float4x4 WorldInvTranspose;
};
StructuredBuffer<FPrimitiveSceneData> ScenePrimitives : register(t26);

struct Payload
{
    uint VisibleClusterIndices[AS_CLUSTERS_PER_GROUP];
};

struct VertexOut
{
    float4 PositionHS : SV_Position;
};

struct PrimitiveOut
{
    uint ClusterIndex  : COLOR1;
    uint TriangleIndex : COLOR2;
};

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
    uint clusterIndex = payloadData.VisibleClusterIndices[gid];

    uint vertexCount = 0;
    uint triangleCount = 0;

    if (clusterIndex < gNaniteClusterCount)
    {
        GPUCluster cluster = NaniteClusters[clusterIndex];
        vertexCount = min(MS_MAX_VERTICES, cluster.UniqueVerticesCount);
        triangleCount = min(MS_MAX_PRIMITIVES, cluster.IndexCount / 3);
    }

    SetMeshOutputCounts(vertexCount, triangleCount);

    if (clusterIndex < gNaniteClusterCount && vertexCount > 0)
    {
        GPUCluster cluster = NaniteClusters[clusterIndex];
        FPrimitiveSceneData primitive = ScenePrimitives[cluster.PrimitiveId];

        // Output vertices
        for (uint i = gtid.x; i < vertexCount; i += MS_NUM_THREADS)
        {
            uint globalVertexIndex = NaniteUniqueVertices[cluster.UniqueVerticesOffset + i];
            NaniteVertex nv = NaniteVertices[globalVertexIndex];

            VertexOut vout;
            float3 posWS = mul(float4(nv.Position, 1), primitive.LocalToWorld).xyz;
            vout.PositionHS = mul(float4(posWS, 1), gViewProj);

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
            primitives[triIdx].TriangleIndex = triIdx;
        }
    }
}
