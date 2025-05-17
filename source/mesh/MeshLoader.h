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
    void ProcessNode(aiNode* Node, const aiScene* Scene, std::vector<Mesh>& OutMeshes);
    void ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, std::vector<Mesh>& OutMeshes);
    
public:
    ErrorCode LoadMesh(const std::string& Path, std::vector<Mesh>& OutMeshes);
    ErrorCode LoadCubeMesh(std::vector<Mesh>& OutMeshes);
    ErrorCode GenerateMeshletData(const std::vector<Mesh>& Meshes, std::vector<MeshletDataForMeshOptimizer>& OutMeshletData) const;
};