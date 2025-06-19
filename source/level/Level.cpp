#include "Level.h"
#include "../actor/CameraActor.h"
#include "../actor/StaticMeshActor.h"
#include "../mesh/MeshLoader.h"
#include "../dx12/PipelineInterface.h"
#include <map>

using namespace std;
using namespace DirectX;

int Level::InstantiateStaticMeshActors(const string& Path)
{
    vector<Mesh> Meshes;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes);

    vector<unique_ptr<Mesh>> MeshInstances;
    
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unique_ptr<Mesh> MeshInstance = make_unique<Mesh>();
        MeshInstance->Vertices = Meshes[I].Vertices;
        MeshInstance->Indices = Meshes[I].Indices;
        MeshInstance->Local2WorldMatrix = Meshes[I].Local2WorldMatrix;
        MeshInstance->Name = Meshes[I].Name;
        MeshInstance->BoundingSphere = Meshes[I].BoundingSphere;
        MeshInstances.push_back(move(MeshInstance));
    }

    map<int, vector<unique_ptr<MeshletData>>> MeshletDatasMap;
    map<int, vector<unique_ptr<MeshletDataProxy>>> MeshletDataProxiesMap;
    map<int, vector<MeshLODData>> MeshLODDatasMap;
    
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        vector<MeshLODData> LODDatas;
        MeshLoader::GetInstance().GenerateWholeMeshLODData(Meshes[I], MeshLODSettings::GetInstance(), LODDatas);

        vector<unique_ptr<MeshletData>> MeshletDatas;
        MeshLoader::GetInstance().GenerateWholeMeshletData(LODDatas, MeshletDatas);
        
        vector<unique_ptr<MeshletDataProxy>> MeshletDataProxies;
        for (size_t Index = 0; Index < MeshletDatas.size(); ++Index)
        {
            MeshletDataProxies.emplace_back(make_unique<MeshletDataProxy>());
        }
        
        MeshletDatasMap.emplace(I, move(MeshletDatas));
        MeshletDataProxiesMap.emplace(I, move(MeshletDataProxies));
        MeshLODDatasMap.emplace(I, move(LODDatas));
    }

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        for (size_t LODIndex = 0; LODIndex < MeshletDatasMap[I].size(); ++LODIndex)
        {
            PipelineInterface::GetInstance().CreateMeshletDataProxyBuffer(
                MeshLODDatasMap[I][LODIndex].Vertices,
                MeshletDatasMap[I][LODIndex].get(), 
                MeshletDataProxiesMap[I][LODIndex].get()
                );
        }
        
        unique_ptr<StaticMeshActor> ActorInstance = make_unique<StaticMeshActor>(
            move(MeshInstances[I]),
            move(MeshletDatasMap[I]),
            move(MeshletDataProxiesMap[I]));

        ActorInstance->Transform.Position.x = 0;
        ActorInstance->Transform.Position.y = 0;
        ActorInstance->Transform.Position.z = 0;
        ActorInstance->Transform.Scale.x = 1.0f;
        ActorInstance->Transform.Scale.y = 1.0f;
        ActorInstance->Transform.Scale.z = 1.0f;
        ActorInstance->Transform.Rotation.x = 0;
        ActorInstance->Transform.Rotation.y = 0;
        ActorInstance->Transform.Rotation.z = 0;
        ActorInstance->Name = Meshes[I].Name;
        
        PipelineInterface::GetInstance().CreateConstantBuffer(ActorInstance.get());

        StaticMeshActors.push_back(move(ActorInstance));
    }
    
    return Meshes.size();
}

StaticMeshActor* Level::InstantiateCullingVisualCameraActor()
{
    //  XXX:    Hard recorded file path, and it must has 1 sub mesh only.
    const string CameraPath = "D:\\GPU-pipeline\\content\\mesh\\duck.fbx";

    vector<Mesh> Meshes;
    MeshLoader::GetInstance().LoadMesh(CameraPath, Meshes);

    unique_ptr<Mesh> MeshInstance = make_unique<Mesh>();
    MeshInstance->Vertices = Meshes[0].Vertices;
    MeshInstance->Indices = Meshes[0].Indices;
    MeshInstance->Local2WorldMatrix = Meshes[0].Local2WorldMatrix;
    MeshInstance->Name = Meshes[0].Name;
    
    vector<MeshLODData> LODDatasForCamera;
    MeshLoader::GetInstance().GenerateWholeMeshLODData(Meshes[0], MeshLODSettings::GetInstance(), LODDatasForCamera);

    vector<unique_ptr<MeshletData>> MeshletDatas;
    MeshLoader::GetInstance().GenerateWholeMeshletData(LODDatasForCamera, MeshletDatas);
    
    vector<unique_ptr<MeshletDataProxy>> MeshletDataProxyInstances;
    MeshletDataProxyInstances.resize(MeshletDatas.size());
    for (size_t Index = 0; Index < MeshletDatas.size(); ++Index)
    {
        MeshletDataProxyInstances[Index] = make_unique<MeshletDataProxy>();
    }

    for (size_t Index = 0; Index < MeshletDatas.size(); ++Index)
    {
        PipelineInterface::GetInstance().CreateMeshletDataProxyBuffer(
        LODDatasForCamera[Index].Vertices,
            MeshletDatas[Index].get(), 
            MeshletDataProxyInstances[Index].get()
            );
    }

    unique_ptr<CullingVisualCameraActor> ActorInstance = make_unique<CullingVisualCameraActor>(
        move(MeshInstance),
        move(MeshletDatas),
        move(MeshletDataProxyInstances));

    PipelineInterface::GetInstance().CreateConstantBuffer(static_cast<StaticMeshActor*>(ActorInstance.get()));
    
    ActorInstance->Transform.Position.x = 25;
    ActorInstance->Transform.Position.y = 500;
    ActorInstance->Transform.Position.z = 30;
    ActorInstance->Transform.Scale.x = 1.0f;
    ActorInstance->Transform.Scale.y = 1.0f;
    ActorInstance->Transform.Scale.z = 1.0f;
    ActorInstance->Transform.Rotation.x = 0;
    ActorInstance->Transform.Rotation.y = 0;
    ActorInstance->Transform.Rotation.z = 0;

    ActorInstance->FovY = 90.f;
    ActorInstance->AspectRatio = 1.f;
    ActorInstance->NearPlane = 0.1f;
    ActorInstance->FarPlane = 10000.0f;
    
    const size_t StartPos = CameraPath.find_last_of('\\');
    const size_t EndPos = CameraPath.find_last_of('.');
    ActorInstance->Name = CameraPath.substr(StartPos + 1, EndPos - StartPos - 1);
    
    StaticMeshActors.push_back(move(ActorInstance));
    
    return StaticMeshActors.back().get();
}

CameraActor* Level::InstantiateCameraActor()
{
    unique_ptr<CameraActor> ActorInstance = make_unique<CameraActor>();
    ActorInstance->FovY = 90.f;
    ActorInstance->AspectRatio = 1.f;
    ActorInstance->NearPlane = 0.1f;
    ActorInstance->FarPlane = 10000.0f;
    ActorInstance->Transform.Position.x = 0;
    ActorInstance->Transform.Position.y = 0;
    ActorInstance->Transform.Position.z = -5.f;
    ActorInstance->LookDirection = XMFLOAT3(0, 0, 1);
    ActorInstance->UpDirection = XMFLOAT3(0, 1, 0);
    CameraActors.push_back(move(ActorInstance));

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
