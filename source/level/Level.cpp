#include "Level.h"
#include "../actor/CameraActor.h"
#include "../actor/StaticMeshActor.h"

using namespace std;
using namespace DirectX;

StaticMeshActor* Level::InstantiateStaticMeshActor(const string& Path)
{
    unique_ptr<StaticMeshActor> ActorInstance = make_unique<StaticMeshActor>(Path);
    ActorInstance->Transform.Position.x = 0;
    ActorInstance->Transform.Position.y = 0;
    ActorInstance->Transform.Position.z = 0;
    ActorInstance->Transform.Scale.x = 1.0f;
    ActorInstance->Transform.Scale.y = 1.0f;
    ActorInstance->Transform.Scale.z = 1.0f;
    ActorInstance->Transform.Rotation.x = 0;
    ActorInstance->Transform.Rotation.y = 0;
    ActorInstance->Transform.Rotation.z = 0;
    
    const size_t StartPos = Path.find_last_of('\\');
    const size_t EndPos = Path.find_last_of('.');
    ActorInstance->Name = Path.substr(StartPos + 1, EndPos - StartPos - 1);
    
    StaticMeshActors.push_back(std::move(ActorInstance));
    
    return StaticMeshActors.back().get();
}

StaticMeshActor* Level::InstantiateCullingVisualCameraActor(const std::string& Path)
{
    unique_ptr<CullingVisualCameraActor> ActorInstance = make_unique<CullingVisualCameraActor>(Path);
    ActorInstance->Transform.Position.x = 0;
    ActorInstance->Transform.Position.y = 70;
    ActorInstance->Transform.Position.z = -100;
    ActorInstance->Transform.Scale.x = 1.0f;
    ActorInstance->Transform.Scale.y = 1.0f;
    ActorInstance->Transform.Scale.z = 1.0f;
    ActorInstance->Transform.Rotation.x = 0;
    ActorInstance->Transform.Rotation.y = 0;
    ActorInstance->Transform.Rotation.z = 0;

    ActorInstance->FovY = 90.f;
    ActorInstance->AspectRatio = 1.f;
    ActorInstance->NearPlane = 0.1f;
    ActorInstance->FarPlane = 1000.0f;
    ActorInstance->LookDirection = XMFLOAT3(0, 0, 1);
    ActorInstance->UpDirection = XMFLOAT3(0, 1, 0);
    
    const size_t StartPos = Path.find_last_of('\\');
    const size_t EndPos = Path.find_last_of('.');
    ActorInstance->Name = Path.substr(StartPos + 1, EndPos - StartPos - 1);
    
    StaticMeshActors.push_back(std::move(ActorInstance));
    
    return StaticMeshActors.back().get();
}

CameraActor* Level::InstantiateCameraActor()
{
    unique_ptr<CameraActor> ActorInstance = make_unique<CameraActor>();
    ActorInstance->FovY = 90.f;
    ActorInstance->AspectRatio = 1.f;
    ActorInstance->NearPlane = 0.1f;
    ActorInstance->FarPlane = 1000.0f;
    ActorInstance->Transform.Position.x = 0;
    ActorInstance->Transform.Position.y = 0;
    ActorInstance->Transform.Position.z = -5.f;
    ActorInstance->LookDirection = XMFLOAT3(0, 0, 1);
    ActorInstance->UpDirection = XMFLOAT3(0, 1, 0);
    CameraActors.push_back(std::move(ActorInstance));

    return CameraActors.back().get();
}

void Level::Update(float DeletaTime) const
{
    for (const unique_ptr<CameraActor>& Actor : CameraActors)
    {
        Actor->Update(DeletaTime);
    }
    
    for (const unique_ptr<StaticMeshActor>& Actor : StaticMeshActors)
    {
        Actor->Update(DeletaTime);
    }
}

vector<StaticMeshActor*> Level::GetStaticMeshActors() const
{
    vector<StaticMeshActor*> Result;

    for (int I = 0; I < StaticMeshActors.size(); ++I)
    {
        Result.push_back(StaticMeshActors[I].get());
    }
    
    return Result;    
}

vector<CameraActor*> Level::GetCameraActors() const
{
    vector<CameraActor*> Result;

    for (int I = 0; I < CameraActors.size(); ++I)
    {
        Result.push_back(CameraActors[I].get());
    }
    
    return Result;    
}

Level::~Level()
{
    StaticMeshActors.clear();
}
