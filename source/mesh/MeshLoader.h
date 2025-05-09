#pragma once
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include "../misc/Base.h"
#include "../misc/DesignPatterns.h"

class MeshLoader : public Singleton<MeshLoader>
{
    void ProcessNode(aiNode* Node, const aiScene* Scene, vector<Mesh>& OutMeshes);
    void ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, vector<Mesh>& OutMeshes);
    
public:
    ErrorCode LoadMesh(const std::string& Path, vector<Mesh>& OutMeshes);
};