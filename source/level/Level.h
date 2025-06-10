#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../actor/StaticMeshActor.h"
#include "../actor/CameraActor.h"
#include "../misc/DesignPatterns.h"

class Level : public Singleton<Level>
{
    std::vector<std::unique_ptr<StaticMeshActor>> StaticMeshActors;
    std::vector<std::unique_ptr<CameraActor>> CameraActors;
    
public:
    int InstantiateStaticMeshActors(const std::string& Path);
    StaticMeshActor* InstantiateCullingVisualCameraActor();
    CameraActor* InstantiateCameraActor();
    void Update(float DeletaTime) const;
    std::vector<StaticMeshActor*> GetStaticMeshActors() const;
    std::vector<CameraActor*> GetCameraActors() const;
    ~Level();
};