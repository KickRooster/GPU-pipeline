#pragma once
#include "../misc/Base.h"
#include <vector>
#include "meshoptimizer.h"
#include <DirectXMath.h>

struct Mesh
{
    std::string Name;
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
    DirectX::XMFLOAT4X4 Local2WorldMatrix;
    DirectX::XMFLOAT4 BoundingSphere;
};

struct MeshletDataForMeshOptimizer
{
    std::vector<meshopt_Meshlet> Meshlets;
    std::vector<unsigned int> MeshletVertices;
    std::vector<unsigned char> MeshletIndices;
    std::vector<meshopt_Bounds> MeshletBounds;
};

//  XXX:    We use unsigned int for MeshletIndices now, it's easy for developing early.
struct MeshletData
{
    std::vector<meshopt_Meshlet> Meshlets;
    std::vector<unsigned int> MeshletVertices;
    std::vector<unsigned int> MeshletIndices;
    std::vector<meshopt_Bounds> MeshletBounds;
};