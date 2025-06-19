#pragma once
#include <string>
#include <vector>
#include <memory>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <DirectXMath.h>

#include "Mesh.h"
#include "../misc/Base.h"
#include "../misc/DesignPatterns.h"

struct MeshLODSettings : public Singleton<MeshLODSettings>
{
    friend class Singleton<MeshLODSettings>;
    MeshLODSettings() = default; 
    ~MeshLODSettings() override = default;
    
    const bool bAutoGenerateLODs = true;
    const int NumLODs = 4;
    const float BaseReductionPercentage = 0.25f;
    const float ReductionMultiplier = 1.5f;
    const float ImportanceWeight = 1.0f;
    const bool bEnableBoundaryProtection = false;
};

struct MeshLODData
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
    float ScreenSizePercentage;
    float ReductionPercentage;
    float MaxDeviation;
    float WeldingThreshold;
    int MinTriangleCount;
    bool bLockBoundaries;
    bool bLockUVBoundaries;
};

class MeshLoader : public Singleton<MeshLoader>
{
private:
    void ProcessNode(aiNode* Node, const aiScene* Scene, std::vector<Mesh>& OutMeshes, const DirectX::XMMATRIX& ParentTransform = DirectX::XMMatrixIdentity());
    void ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, std::vector<Mesh>& OutMeshes, const DirectX::XMMATRIX& NodeTransform);
    void GenerateMeshletData(const std::vector<Vertex>& Vertices, const std::vector<unsigned int>& Indices, MeshletData& OutMeshletData) const;
    
public:
    ErrorCode LoadMesh(const std::string& Path, std::vector<Mesh>& OutMeshes);
    void GenerateWholeMeshLODData(const Mesh& Mesh, const MeshLODSettings& Settings, std::vector<MeshLODData>& OutLODDatas) const;
    void GenerateWholeMeshletData(const std::vector<MeshLODData>& LODDatas, std::vector<std::unique_ptr<MeshletData>>& OutMeshletDatas) const;
};