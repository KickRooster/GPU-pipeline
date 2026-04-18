#pragma once
#include "Actor.h"
#include "../dx12/MeshProxy.h"
#include "../dx12/TextureProxy.h"
#include "../asset/Texture.h"
#include <vector>
#include <memory>
#include <string>

static constexpr unsigned int TERRAIN_MAX_LAYERS = 12;

struct TerrainMaterialTextures
{
    unsigned int HeightmapIndex = 0xFFFFFFFF;
    unsigned int SplatmapIndices[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
    unsigned int AlbedoIndices[TERRAIN_MAX_LAYERS];
    unsigned int NormalIndices[TERRAIN_MAX_LAYERS];
    unsigned int RoughnessIndices[TERRAIN_MAX_LAYERS];
    unsigned int LayerCount = 0;

    TerrainMaterialTextures();
};

struct QuadTreeNode
{
    unsigned int Level;           // 0=root, maxLevel=leaf
    unsigned int ChildIndices[4]; // 0=no child (leaf)
    unsigned int BoundIndex;
    unsigned int PatchIndex;      // Leaf: index into Patches; non-leaf: set during generation
};

class TerrainActor : public Actor
{
private:
    void BuildQuadTree();
    void SelectActivePatches(const DirectX::XMFLOAT3& CameraPos);

public:
    float TerrainSize = 16384.0f;
    float TargetNearPatchSize = 16.0f;
    float HeightScale = 1920.0f;
    unsigned int GridCount = 0;
    unsigned int PatchCount = 0;
    unsigned int TotalNodeCount = 0;
    unsigned int MaxLevel = 0;
    float PatchSize = 0.0f;
    static constexpr unsigned int MAX_LOD_LEVELS = 16;
    float LODRangesSq[MAX_LOD_LEVELS] = {};
    std::vector<TerrainPatchData> Patches;
    std::vector<TerrainPatchBound> Bounds;
    std::vector<QuadTreeNode> QuadTreeNodes;
    std::vector<unsigned int> ActivePatchIndices;
    bool ActivePatchIndicesDirty = true;
    bool EdgeStitchingEnabled = true;
    bool DebugLODColors = false;
    float DebugScale = 1.0f;
    std::unique_ptr<Texture> HeightmapTexture;
    std::string HeightmapPath;
    std::unique_ptr<TerrainProxy> ProxyInstance;
    TerrainProxy* GetProxy() const;
    TerrainMaterialTextures MaterialTextures;
    std::vector<std::unique_ptr<TextureProxy>> MaterialTextureProxies;

    TerrainActor() = default;
    ~TerrainActor() override = default;
    ErrorCode Initialize(const std::string& HeightmapPath);
    ErrorCode LoadTerrainTextures();
    void Update(float DeltaTime, unsigned int FrameIndex) override;
    void UpdateLOD(const DirectX::XMFLOAT3& CameraPos);
};