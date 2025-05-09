#pragma once
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../misc/Base.h"

using namespace std;

struct Mesh
{
    vector<Vertex> Vertices;
    vector<unsigned int> Indices;
};