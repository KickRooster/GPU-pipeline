#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../actor/StaticMesh.h"
#include "../actor/Camera.h"
#include "../actor/SkyLight.h"
#include "../actor/TerrainActor.h"
#include "../misc/DesignPatterns.h"

class Level : public Singleton<Level>
{
    std::vector<std::unique_ptr<StaticMesh>> StaticMeshes;
    std::vector<std::unique_ptr<Camera>> Cameras;
    std::vector<std::unique_ptr<SkyLight>> SkyLights;
    std::unique_ptr<TerrainActor> Terrain;

    // All materials across all meshes (keeps GPU texture resources alive)
    std::vector<std::unique_ptr<Material>> AllMaterials;
    // All material proxies (bindless texture indices), indexed by global material ID
    std::vector<MaterialProxy> AllMaterialProxies;

public:
    int InstantiateStaticMeshes(const std::string& Path);
    StaticMesh* InstantiateCullingVisualCamera();
    Camera* InstantiateCamera();
    SkyLight* InstantiateSkyLight();
    TerrainActor* InstantiateTerrain();
    void CreateGlobalMeshBuffers();
    void Update(float DeltaTime, unsigned int FrameIndex) const;
    const std::vector<std::unique_ptr<StaticMesh>>& GetStaticMeshes() const;
    std::vector<Camera*> GetCameras() const;
    std::vector<SkyLight*> GetSkyLights() const;
    TerrainActor* GetTerrain() const;
    std::vector<Actor*> GetSelectableActors() const;
    const std::vector<MaterialProxy>& GetAllMaterialProxies() const;
    void Clear();
    ~Level();
};