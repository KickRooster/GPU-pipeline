#include "Level.h"
#include "../actor/CameraActor.h"
#include "../actor/StaticMeshActor.h"
#include "../mesh/MeshLoader.h"
#include "../dx12/PipelineInterface.h"

using namespace std;
using namespace DirectX;

int Level::InstantiateStaticMeshActors(const std::string& Path)
{
    vector<Mesh> Meshes;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes);

    vector<MeshletDataForMeshOptimizer> MeshletDatas;
    MeshLoader::GetInstance().GenerateMeshletData(Meshes, MeshletDatas);

    std::vector<std::unique_ptr<Mesh>> MeshInstances;
    std::vector<std::unique_ptr<MeshProxy>> MeshProxyInstances;
    
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unique_ptr<Mesh> MeshInstance = make_unique<Mesh>();
        MeshInstance->Vertices = Meshes[I].Vertices;
        MeshInstance->Indices = Meshes[I].Indices;
        MeshInstances.push_back(std::move(MeshInstance));
        
        unique_ptr<MeshProxy> MeshProxyInstance = make_unique<MeshProxy>();
        MeshProxyInstances.push_back(std::move(MeshProxyInstance));
    }

    std::vector<std::unique_ptr<MeshletData>> MeshletDataInstances;
    std::vector<std::unique_ptr<MeshletDataProxy>> MeshletDataProxyInstances;
    
    for (unsigned int I = 0; I < MeshletDatas.size(); ++I)
    {
        unique_ptr<MeshletData> MeshletDataInstance = make_unique<MeshletData>();
        MeshletDataInstance->Meshlets = MeshletDatas[I].Meshlets;
        MeshletDataInstance->MeshletVertices = MeshletDatas[I].MeshletVertices;
        for (unsigned int J = 0; J < MeshletDatas[I].MeshletIndices.size(); ++J)
        {
            MeshletDataInstance->MeshletIndices.push_back(static_cast<unsigned int>(MeshletDatas[I].MeshletIndices[J]));
        }
        MeshletDataInstance->MeshletBounds = MeshletDatas[I].MeshletBounds;
        MeshletDataInstances.push_back(std::move(MeshletDataInstance));
        
        unique_ptr<MeshletDataProxy> MeshletDataProxyInstance = make_unique<MeshletDataProxy>();
        MeshletDataProxyInstances.push_back(std::move(MeshletDataProxyInstance));
    }

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        PipelineInterface::GetInstance().CreateMeshProxyBuffer(MeshInstances[I].get(), MeshProxyInstances[I].get());
        PipelineInterface::GetInstance().CreateMeshletDataProxyBuffer(MeshletDataInstances[I].get(), MeshletDataProxyInstances[I].get());
        
        unique_ptr<StaticMeshActor> ActorInstance = make_unique<StaticMeshActor>(
            move(MeshInstances[I]),
            move(MeshProxyInstances[I]),
            move(MeshletDataInstances[I]),
            move(MeshletDataProxyInstances[I]));

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
    //  XXX:    Hard recorded file path, and it only has 1 sub mesh by default.
    const string CameraPath = "D:\\GPU-pipeline\\content\\mesh\\jeep1.fbx";

    vector<Mesh> Meshes;
    MeshLoader::GetInstance().LoadMesh(CameraPath, Meshes);

    vector<MeshletDataForMeshOptimizer> MeshletDatas;
    MeshLoader::GetInstance().GenerateMeshletData(Meshes, MeshletDatas);

    unique_ptr<Mesh> MeshInstance = make_unique<Mesh>();
    MeshInstance->Vertices = Meshes[0].Vertices;
    MeshInstance->Indices = Meshes[0].Indices;
    unique_ptr<MeshProxy> MeshProxyInstance = make_unique<MeshProxy>();
    
    unique_ptr<MeshletData> MeshletDataInstance = make_unique<MeshletData>();
    MeshletDataInstance->Meshlets = MeshletDatas[0].Meshlets;
    MeshletDataInstance->MeshletVertices = MeshletDatas[0].MeshletVertices;
    for (unsigned int J = 0; J < MeshletDatas[0].MeshletIndices.size(); ++J)
    {
        MeshletDataInstance->MeshletIndices.push_back(static_cast<unsigned int>(MeshletDatas[0].MeshletIndices[J]));
    }
    MeshletDataInstance->MeshletBounds = MeshletDatas[0].MeshletBounds;
    
    unique_ptr<MeshletDataProxy> MeshletDataProxyInstance = make_unique<MeshletDataProxy>();
    
    PipelineInterface::GetInstance().CreateMeshProxyBuffer(MeshInstance.get(), MeshProxyInstance.get());
    PipelineInterface::GetInstance().CreateMeshletDataProxyBuffer(MeshletDataInstance.get(), MeshletDataProxyInstance.get());

    unique_ptr<CullingVisualCameraActor> ActorInstance = make_unique<CullingVisualCameraActor>(
        std::move(MeshInstance),
        std::move(MeshProxyInstance),
        std::move(MeshletDataInstance),
        std::move(MeshletDataProxyInstance));

    PipelineInterface::GetInstance().CreateConstantBuffer(static_cast<StaticMeshActor*>(ActorInstance.get()));
    
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
    ActorInstance->FarPlane = 10000.0f;
    
    const size_t StartPos = CameraPath.find_last_of('\\');
    const size_t EndPos = CameraPath.find_last_of('.');
    ActorInstance->Name = CameraPath.substr(StartPos + 1, EndPos - StartPos - 1);
    
    StaticMeshActors.push_back(std::move(ActorInstance));
    
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
