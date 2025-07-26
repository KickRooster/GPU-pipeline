#include "Level.h"
#include "../actor/CameraActor.h"
#include "../actor/StaticMeshActor.h"
#include "../asset/MeshLoader.h"
#include "../asset/Texture.h"
#include "../asset/TextureLoader.h"
#include "../dx12/PipelineInterface.h"
#include "../dx12/TextureProxy.h"
#include <map>

using namespace std;
using namespace DirectX;

int Level::InstantiateStaticMeshActors(const string& Path)
{
    vector<Mesh> Meshes;
    vector<PBRTextureNamesPatch> TextureNamesPatches;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes, TextureNamesPatches);

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

    vector<unique_ptr<Material>> MaterialInstances;
    vector<unique_ptr<MaterialProxy>> MaterialProxyInstances;
    
    for (unsigned int I = 0; I < TextureNamesPatches.size(); ++I)
    {
        unique_ptr<Material> MaterialInstance = make_unique<Material>();
        unique_ptr<MaterialProxy> MaterialProxyInstance = make_unique<MaterialProxy>();

        if (!TextureNamesPatches[I].AlbedoPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].AlbedoPath, true, TextureInstance) == ErrorCode::OK)
            {
                MaterialInstance->AlbedoTexture = make_unique<Texture>(std::move(TextureInstance));
                MaterialProxyInstance->AlbedoTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].NormalPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].NormalPath, false, TextureInstance) == ErrorCode::OK)
            {
                MaterialInstance->NormalTexture = make_unique<Texture>(std::move(TextureInstance));
                MaterialProxyInstance->NormalTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].MetallicPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].MetallicPath, false, TextureInstance) == ErrorCode::OK)
            {
                MaterialInstance->MetallicTexture = make_unique<Texture>(std::move(TextureInstance));
                MaterialProxyInstance->MetallicTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].RoughnessPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].RoughnessPath, false, TextureInstance) == ErrorCode::OK)
            {
                MaterialInstance->RoughnessTexture = make_unique<Texture>(std::move(TextureInstance));
                MaterialProxyInstance->RoughnessTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
            }
        }

        MaterialInstances.push_back(move(MaterialInstance));
        MaterialProxyInstances.push_back(move(MaterialProxyInstance));
    }

    PipelineInterface::GetInstance().ResetUploadCommandAllocator();
    PipelineInterface::GetInstance().ResetUploadCommandList();

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        for (size_t LODIndex = 0; LODIndex < MeshletDatasMap[I].size(); ++LODIndex)
        {
            PipelineInterface::GetInstance().CreateMeshletDataProxyBuffer(
                MeshLODDatasMap[I][LODIndex].Vertices,
                MeshletDatasMap[I][LODIndex].get(), 
                MeshletDataProxiesMap[I][LODIndex].get(),
                false
                );
        }

        //  XXX:    MaterialInstances.size() == Meshes.size()
        if (MaterialInstances[I]->AlbedoTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->AlbedoTexture.get(),
                MaterialProxyInstances[I]->AlbedoTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[I]->AlbedoTextureProxy = move(TextureProxyInstance);
        }

        if (MaterialInstances[I]->NormalTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->NormalTexture.get(),
                MaterialProxyInstances[I]->NormalTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[I]->NormalTextureProxy = move(TextureProxyInstance);
        }

        if (MaterialInstances[I]->MetallicTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->MetallicTexture.get(),
                MaterialProxyInstances[I]->MetallicTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[I]->MetallicTextureProxy = move(TextureProxyInstance);
        }

        if (MaterialInstances[I]->RoughnessTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->RoughnessTexture.get(),
                MaterialProxyInstances[I]->RoughnessTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[I]->RoughnessTextureProxy = move(TextureProxyInstance);
        }
    }

    PipelineInterface::GetInstance().ExecuteAndWaitUploadCommandList();
    
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unique_ptr<StaticMeshActor> ActorInstance = make_unique<StaticMeshActor>(
            &MeshInstances[I]->Local2WorldMatrix,
            MeshInstances[I]->BoundingSphere,
            move(MeshletDatasMap[I]),
            move(MeshletDataProxiesMap[I]));
        
        ActorInstance->SetMaterial(move(MaterialInstances[I]), move(MaterialProxyInstances[I]));

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
    
    return static_cast<int>(Meshes.size());
}

StaticMeshActor* Level::InstantiateCullingVisualCameraActor()
{
    //  XXX:    Hard recorded file path, and it must has 1 sub mesh only.
    const string CameraPath = "D:\\GPU-pipeline\\content\\mesh\\mp5_sil.fbx";

    vector<Mesh> Meshes;
    vector<PBRTextureNamesPatch> TextureNamesPatches;
    MeshLoader::GetInstance().LoadMesh(CameraPath, Meshes, TextureNamesPatches);

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

    unique_ptr<Material> MaterialInstance = make_unique<Material>();
    unique_ptr<MaterialProxy> MaterialProxyInstance = make_unique<MaterialProxy>();
    
    if (!TextureNamesPatches[0].AlbedoPath.empty())
    {
        Texture TextureInstance;
        if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[0].AlbedoPath, true, TextureInstance) == ErrorCode::OK)
        {
            MaterialInstance->AlbedoTexture = make_unique<Texture>(std::move(TextureInstance));
            MaterialProxyInstance->AlbedoTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
        }
    }

    if (!TextureNamesPatches[0].NormalPath.empty())
    {
        Texture TextureInstance;
        if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[0].NormalPath, false, TextureInstance) == ErrorCode::OK)
        {
            MaterialInstance->NormalTexture = make_unique<Texture>(std::move(TextureInstance));
            MaterialProxyInstance->NormalTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
        }
    }

    if (!TextureNamesPatches[0].MetallicPath.empty())
    {
        Texture TextureInstance;
        if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[0].MetallicPath, false, TextureInstance) == ErrorCode::OK)
        {
            MaterialInstance->MetallicTexture = make_unique<Texture>(std::move(TextureInstance));
            MaterialProxyInstance->MetallicTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
        }
    }

    if (!TextureNamesPatches[0].RoughnessPath.empty())
    {
        Texture TextureInstance;
        if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[0].RoughnessPath, false, TextureInstance) == ErrorCode::OK)
        {
            MaterialInstance->RoughnessTexture = make_unique<Texture>(std::move(TextureInstance));
            MaterialProxyInstance->RoughnessTextureIndex = SimpleBindlessAllocator::GetInstance().AllocateRange(1);
        }
    }
    
    if (MaterialInstance->AlbedoTexture)
    {
        unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
        PipelineInterface::GetInstance().CreateTexture(
            MaterialInstance->AlbedoTexture.get(),
            MaterialProxyInstance->AlbedoTextureIndex,
            TextureProxyInstance.get()
        );

        MaterialInstance->AlbedoTextureProxy = move(TextureProxyInstance);
    }
    
    unique_ptr<CullingVisualCameraActor> ActorInstance = make_unique<CullingVisualCameraActor>(
        move(MeshInstance),
        move(MeshletDatas),
        move(MeshletDataProxyInstances));

    ActorInstance->SetMaterial(move(MaterialInstance), move(MaterialProxyInstance));

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
