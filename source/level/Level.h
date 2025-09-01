#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../actor/StaticMesh.h"
#include "../actor/Camera.h"
#include "../actor/SkyLight.h"
#include "../misc/DesignPatterns.h"

class Level : public Singleton<Level>
{
    std::vector<std::unique_ptr<StaticMesh>> StaticMeshes;
    std::vector<std::unique_ptr<Camera>> Cameras;
    std::vector<std::unique_ptr<SkyLight>> SkyLights;
    
public:
    int InstantiateStaticMeshes(const std::string& Path);
    StaticMesh* InstantiateCullingVisualCamera();
    Camera* InstantiateCamera();
    SkyLight* InstantiateSkyLight();
    void Update(float DeletaTime) const;
    std::vector<StaticMesh*> GetStaticMeshes() const;
    std::vector<Camera*> GetCameras() const;
    std::vector<SkyLight*> GetSkyLights() const;
    ~Level();
};