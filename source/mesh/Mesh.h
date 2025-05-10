#pragma once
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../misc/Base.h"

struct Mesh
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
};