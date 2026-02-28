#pragma once
#include "../misc/Base.h"
#include <vector>
#include <string>

struct Mesh
{
    std::string Name;
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
    DirectX::XMFLOAT4X4 Local2WorldMatrix;
    DirectX::XMFLOAT4 BoundingSphere;
};

struct CLODBound
{
    float Center[3];  // Sphere center in mesh space
    float Radius;     // Sphere radius in mesh space
    float Error;      // Combined simplification error
};

struct ClusterData
{
    std::vector<unsigned int> UniqueVertices;   // Global vertex indices after per-cluster deduplication (≤64 for Mesh Shader limit)
    std::vector<unsigned char> LocalIndices;    // Cluster-local indices (0-255, references UniqueVertices)
    std::vector<unsigned int> TriangleMaterialIDs;  // Material ID for each triangle in the cluster (size = LocalIndices.size() / 3)

    CLODBound Bound;
    int Refined;  // Index to more detailed group (-1 = leaf, for LOD selection)
    int GroupId;  // Index to group this cluster belongs to
};

struct NaniteData
{
    std::vector<ClusterData> Clusters;
    std::vector<CLODBound> GroupBounds;
};