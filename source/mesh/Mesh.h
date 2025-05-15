#pragma once
#include "../misc/Base.h"
#include <vector>
#include <assimp/scene.h>
#include "meshoptimizer.h"

struct Mesh
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
};

struct MeshletData
{
    std::vector<meshopt_Meshlet> Meshlets;
    std::vector<unsigned int> MeshletVertices;
    std::vector<unsigned char> MeshletIndices;
};