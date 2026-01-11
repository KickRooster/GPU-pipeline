#pragma once
#include "../misc/Base.h"
#include "../misc/DesignPatterns.h"
#include "Mesh.h"
#include "Texture.h"
#include <string>
#include <vector>
#include <memory>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

struct PBRTextureNamesPatch;

class MeshLoader : public Singleton<MeshLoader>
{
private:
    void ExtractPBRTextures(const aiMaterial* Material, PBRTextureNamesPatch& OutTextureNamesPatch);
    void ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, const DirectX::XMMATRIX& NodeTransform, std::vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches);
    void ProcessNode(aiNode* Node, const aiScene* Scene, const DirectX::XMMATRIX& ParentTransform, std::vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches);

public:
    ErrorCode LoadMesh(const std::string& Path, std::vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches);
    ErrorCode Nanite(const Mesh& Mesh, std::vector<ClusterData>& OutClusters, std::vector<CLODBound>& OutGroupBounds);
};