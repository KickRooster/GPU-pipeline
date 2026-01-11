#include "Level.h"
#include "../actor/Camera.h"
#include "../actor/StaticMesh.h"
#include "../asset/MeshLoader.h"
#include "../asset/Texture.h"
#include "../asset/TextureLoader.h"
#include "../dx12/PipelineInterface.h"
#include "../dx12/TextureProxy.h"
#include "../misc/FileTool.h"
#include <map>

using namespace std;
using namespace DirectX;

int Level::InstantiateStaticMeshes(const string& Path)
{
    vector<Mesh> Meshes;
    vector<PBRTextureNamesPatch> TextureNamesPatches;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes, TextureNamesPatches);
    
    map<int, vector<ClusterData>> ClustersMap;
    map<int, vector<CLODBound>> GroupBoundsMap;

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        vector<ClusterData> Clusters;
        vector<CLODBound> GroupBounds;
        MeshLoader::GetInstance().Nanite(Meshes[I], Clusters, GroupBounds);
        ClustersMap.emplace(I, move(Clusters));
        GroupBoundsMap.emplace(I, move(GroupBounds));
    }

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

    map<int, unique_ptr<NaniteData>> NaniteDatasMap;
    map<int, unique_ptr<NaniteClusterProxy>> NaniteClusterProxiesMap;

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unique_ptr<NaniteData> NaniteDataInstance = make_unique<NaniteData>();
        NaniteDataInstance->Clusters = move(ClustersMap[I]);
        NaniteDataInstance->GroupBounds = move(GroupBoundsMap[I]);
        NaniteDatasMap.emplace(I, move(NaniteDataInstance));

        NaniteClusterProxiesMap.emplace(I, make_unique<NaniteClusterProxy>());
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
                MaterialProxyInstance->AlbedoTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].NormalPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].NormalPath, false, TextureInstance) == ErrorCode::OK)
            {
                MaterialInstance->NormalTexture = make_unique<Texture>(std::move(TextureInstance));
                MaterialProxyInstance->NormalTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].MetallicPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].MetallicPath, false, TextureInstance) == ErrorCode::OK)
            {
                MaterialInstance->MetallicTexture = make_unique<Texture>(std::move(TextureInstance));
                MaterialProxyInstance->MetallicTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].RoughnessPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].RoughnessPath, false, TextureInstance) == ErrorCode::OK)
            {
                MaterialInstance->RoughnessTexture = make_unique<Texture>(std::move(TextureInstance));
                MaterialProxyInstance->RoughnessTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        MaterialInstances.push_back(move(MaterialInstance));
        MaterialProxyInstances.push_back(move(MaterialProxyInstance));
    }

    PipelineInterface::GetInstance().ResetUploadCommandList();

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        if (!NaniteDatasMap[I]->Clusters.empty() && !NaniteDatasMap[I]->GroupBounds.empty())
        {
            PipelineInterface::GetInstance().CreateNaniteClusterProxyBuffer(
                Meshes[I].Vertices,
                NaniteDatasMap[I]->Clusters,
                NaniteDatasMap[I]->GroupBounds,
                NaniteClusterProxiesMap[I].get(),
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
        unique_ptr<StaticMesh> ActorInstance = make_unique<StaticMesh>(
            &MeshInstances[I]->Local2WorldMatrix,
            MeshInstances[I]->BoundingSphere,
            move(NaniteDatasMap[I]),
            move(NaniteClusterProxiesMap[I])
            );

        PipelineInterface::GetInstance().CreateConstantBuffer(ActorInstance.get());

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

        StaticMeshes.push_back(move(ActorInstance));
    }
    
    return static_cast<int>(Meshes.size());
}

StaticMesh* Level::InstantiateCullingVisualCamera()
{
    //  XXX:    Hard recorded file path, and it must has 1 sub mesh only.
    const string CameraPath = FileTool::GetInstance().GetMeshFullPath("mp5_sil.fbx");

    vector<Mesh> Meshes;
    vector<PBRTextureNamesPatch> TextureNamesPatches;
    MeshLoader::GetInstance().LoadMesh(CameraPath, Meshes, TextureNamesPatches);
    
    vector<ClusterData> Clusters;
    vector<CLODBound> GroupBounds;
    MeshLoader::GetInstance().Nanite(Meshes[0], Clusters, GroupBounds);
    
    unique_ptr<Mesh> MeshInstance = make_unique<Mesh>();
    MeshInstance->Vertices = Meshes[0].Vertices;
    MeshInstance->Indices = Meshes[0].Indices;
    MeshInstance->Local2WorldMatrix = Meshes[0].Local2WorldMatrix;
    MeshInstance->Name = Meshes[0].Name;
    MeshInstance->BoundingSphere = Meshes[0].BoundingSphere;

    unique_ptr<NaniteData> NaniteDataInstance = make_unique<NaniteData>();
    NaniteDataInstance->Clusters = move(Clusters);
    NaniteDataInstance->GroupBounds = move(GroupBounds);

    unique_ptr<NaniteClusterProxy> NaniteClusterProxyInstance = make_unique<NaniteClusterProxy>();
    
    unique_ptr<Material> MaterialInstance = make_unique<Material>();
    unique_ptr<MaterialProxy> MaterialProxyInstance = make_unique<MaterialProxy>();

    PipelineInterface::GetInstance().ResetUploadCommandList();

    if (!NaniteDataInstance->Clusters.empty() && !NaniteDataInstance->GroupBounds.empty())
    {
        PipelineInterface::GetInstance().CreateNaniteClusterProxyBuffer(
            Meshes[0].Vertices,
            NaniteDataInstance->Clusters,
            NaniteDataInstance->GroupBounds,
            NaniteClusterProxyInstance.get(),
            false
        );
    }

    PipelineInterface::GetInstance().ExecuteAndWaitUploadCommandList();
    
    unique_ptr<CullingVisualCamera> CullingVisualInstance = make_unique<CullingVisualCamera>(
        move(MeshInstance),
        move(NaniteDataInstance),
        move(NaniteClusterProxyInstance));

    CullingVisualInstance->SetMaterial(move(MaterialInstance), move(MaterialProxyInstance));
    
    PipelineInterface::GetInstance().CreateConstantBuffer(CullingVisualInstance.get());
    
    CullingVisualInstance->Transform.Position.x = 25;
    CullingVisualInstance->Transform.Position.y = 500;
    CullingVisualInstance->Transform.Position.z = 30;
    CullingVisualInstance->Transform.Scale.x = 1.0f;
    CullingVisualInstance->Transform.Scale.y = 1.0f;
    CullingVisualInstance->Transform.Scale.z = 1.0f;
    CullingVisualInstance->Transform.Rotation.x = 0;
    CullingVisualInstance->Transform.Rotation.y = 0;
    CullingVisualInstance->Transform.Rotation.z = 0;

    CullingVisualInstance->FovY = 90.f;
    CullingVisualInstance->AspectRatio = 1.f;
    CullingVisualInstance->NearPlane = 0.1f;
    CullingVisualInstance->FarPlane = 10000.0f;
    
    const size_t StartPos = CameraPath.find_last_of('\\');
    const size_t EndPos = CameraPath.find_last_of('.');
    CullingVisualInstance->Name = CameraPath.substr(StartPos + 1, EndPos - StartPos - 1);
    
    StaticMeshes.push_back(move(CullingVisualInstance));
    
    return StaticMeshes.back().get();
}

Camera* Level::InstantiateCamera()
{
    unique_ptr<Camera> ActorInstance = make_unique<Camera>();
    ActorInstance->FovY = 90.f;
    ActorInstance->AspectRatio = 1.f;
    ActorInstance->NearPlane = 0.1f;
    ActorInstance->FarPlane = 10000.0f;
    ActorInstance->Transform.Position.x = 0;
    ActorInstance->Transform.Position.y = 0;
    ActorInstance->Transform.Position.z = -5.f;
    ActorInstance->LookDirection = XMFLOAT3(0, 0, 1);
    ActorInstance->UpDirection = XMFLOAT3(0, 1, 0);
    Cameras.push_back(move(ActorInstance));

    return Cameras.back().get();
}

SkyLight* Level::InstantiateSkyLight()
{
    unique_ptr<SkyLight> ActorInstance = make_unique<SkyLight>();

    string HDRFilePath = ActorInstance->GetHDRFilePath();
    Texture HDRTexture;
    const ErrorCode Result = TextureLoader::GetInstance().LoadTexture(HDRFilePath, false, HDRTexture);

    if (Result != ErrorCode::OK)
    {
        return nullptr;
    }

    unique_ptr<CubemapTexture> Cubemap = std::make_unique<CubemapTexture>(HDRTexture);

    ActorInstance->IrradianceMap = Cubemap->Convolution(32, 32);
    ActorInstance->IrradianceMapProxy = std::make_unique<CubemapTextureProxy>();
    ActorInstance->IrradianceMapProxy->DescriptorIndex = PipelineInterface::GetInstance().GetCubemapBindlessAllocator().AllocateRange(1);

    ActorInstance->PrefilteredMap = Cubemap->PrefilterEnvironment(5, 128, 32);
    ActorInstance->PrefilteredMapProxy = std::make_unique<CubemapTextureProxy>();
    ActorInstance->PrefilteredMapProxy->DescriptorIndex = PipelineInterface::GetInstance().GetCubemapBindlessAllocator().AllocateRange(1);

    ActorInstance->BRDFLUT = Cubemap->GenerateBRDFLUT(512, 32);
    ActorInstance->BRDFLUTProxy = make_unique<TextureProxy>();
    ActorInstance->BRDFLUTProxy->DescriptorIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
    
    PipelineInterface::GetInstance().ResetUploadCommandList();
    {
        PipelineInterface::GetInstance().CreateCubemap(
                        ActorInstance->IrradianceMap.get(),
                         ActorInstance->IrradianceMapProxy->DescriptorIndex,
                        ActorInstance->IrradianceMapProxy.get(),
                        false
                    );
        PipelineInterface::GetInstance().CreateCubemap(
                        ActorInstance->PrefilteredMap.get(),
                         ActorInstance->PrefilteredMapProxy->DescriptorIndex,
                        ActorInstance->PrefilteredMapProxy.get(),
                        false
                    );
        PipelineInterface::GetInstance().CreateTexture(
            ActorInstance->BRDFLUT.get(),
            ActorInstance->BRDFLUTProxy->DescriptorIndex,
            ActorInstance->BRDFLUTProxy.get(),
            false
        );
    }
    PipelineInterface::GetInstance().ExecuteAndWaitUploadCommandList();
    
    PipelineInterface::GetInstance().CreateConstantBuffer(ActorInstance.get());
    
    ActorInstance->Transform.Position.x = 0;
    ActorInstance->Transform.Position.y = 0;
    ActorInstance->Transform.Position.z = 0;
    SkyLights.push_back(move(ActorInstance));

    return SkyLights.back().get();
}

void Level::Update(float DeletaTime) const
{
    for (const unique_ptr<Camera>& Actor : Cameras)
    {
        Actor->Update(DeletaTime);
    }
    
    for (const unique_ptr<StaticMesh>& Actor : StaticMeshes)
    {
        Actor->Update(DeletaTime);
    }
    
    for (const unique_ptr<SkyLight>& Actor : SkyLights)
    {
        Actor->Update(DeletaTime);
    }
}

vector<StaticMesh*> Level::GetStaticMeshes() const
{
    vector<StaticMesh*> Result;

    for (int I = 0; I < StaticMeshes.size(); ++I)
    {
        Result.push_back(StaticMeshes[I].get());
    }
    
    return Result;    
}

vector<Camera*> Level::GetCameras() const
{
    vector<Camera*> Result;

    for (int I = 0; I < Cameras.size(); ++I)
    {
        Result.push_back(Cameras[I].get());
    }
    
    return Result;    
}

vector<SkyLight*> Level::GetSkyLights() const
{
    vector<SkyLight*> Result;

    for (int I = 0; I < SkyLights.size(); ++I)
    {
        Result.push_back(SkyLights[I].get());
    }
    
    return Result;    
}

Level::~Level()
{
    StaticMeshes.clear();
}
