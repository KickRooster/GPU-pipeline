#include "Level.h"
#include "../actor/Camera.h"
#include "../actor/StaticMesh.h"
#include "../asset/MeshLoader.h"
#include "../asset/Texture.h"
#include "../asset/TextureLoader.h"
#include "../dx12/PipelineInterface.h"
#include "../dx12/TextureProxy.h"
#include "../misc/FileTool.h"

using namespace std;
using namespace DirectX;

int Level::InstantiateStaticMeshes(const string& Path)
{
    vector<Mesh> Meshes;
    vector<PBRTextureNamesPatch> TextureNamesPatches;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes, TextureNamesPatches);

    // Merge all submesh vertices and indices (transforms already baked by MeshLoader)
    vector<Vertex> MergedVertices;
    vector<unsigned int> MergedIndices;

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unsigned int BaseVertex = static_cast<unsigned int>(MergedVertices.size());

        // Append vertices (already in unified coordinate system)
        MergedVertices.insert(MergedVertices.end(),
                             Meshes[I].Vertices.begin(),
                             Meshes[I].Vertices.end());

        // Append indices with base vertex offset
        for (size_t IdxOffset = 0; IdxOffset < Meshes[I].Indices.size(); ++IdxOffset)
        {
            MergedIndices.push_back(Meshes[I].Indices[IdxOffset] + BaseVertex);
        }
    }

    // Run Nanite ONCE on merged data
    vector<ClusterData> MergedClusters;
    vector<CLODBound> MergedGroupBounds;

    if (!MergedVertices.empty() && !MergedIndices.empty())
    {
        Mesh MergedMesh;
        MergedMesh.Vertices = MergedVertices;
        MergedMesh.Indices = MergedIndices;
        MergedMesh.Local2WorldMatrix = Meshes[0].Local2WorldMatrix;
        MergedMesh.BoundingSphere = Meshes[0].BoundingSphere;

        MeshLoader::GetInstance().Nanite(MergedMesh, MergedClusters, MergedGroupBounds);
    }

    // Create single NaniteData and NaniteClusterProxy for the merged mesh
    unique_ptr<NaniteData> MergedNaniteData = make_unique<NaniteData>();
    MergedNaniteData->Clusters = move(MergedClusters);
    MergedNaniteData->GroupBounds = move(MergedGroupBounds);

    unique_ptr<NaniteClusterProxy> MergedNaniteClusterProxy = make_unique<NaniteClusterProxy>();

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

    // GPU-Driven: Don't create per-mesh buffers, just upload textures
    // Mesh buffers will be created globally later via CreateGlobalMergedMeshBuffers

    // Create textures for materials (阶段1: use first material only)
    if (!MaterialInstances.empty())
    {
        if (MaterialInstances[0]->AlbedoTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[0]->AlbedoTexture.get(),
                MaterialProxyInstances[0]->AlbedoTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[0]->AlbedoTextureProxy = move(TextureProxyInstance);
        }

        if (MaterialInstances[0]->NormalTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[0]->NormalTexture.get(),
                MaterialProxyInstances[0]->NormalTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[0]->NormalTextureProxy = move(TextureProxyInstance);
        }

        if (MaterialInstances[0]->MetallicTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[0]->MetallicTexture.get(),
                MaterialProxyInstances[0]->MetallicTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[0]->MetallicTextureProxy = move(TextureProxyInstance);
        }

        if (MaterialInstances[0]->RoughnessTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[0]->RoughnessTexture.get(),
                MaterialProxyInstances[0]->RoughnessTextureIndex,
                TextureProxyInstance.get(),
                false
            );

            MaterialInstances[0]->RoughnessTextureProxy = move(TextureProxyInstance);
        }
    }

    PipelineInterface::GetInstance().ExecuteAndWaitUploadCommandList();
    
    // Create single StaticMesh actor with merged Nanite data
    unique_ptr<StaticMesh> ActorInstance = make_unique<StaticMesh>(
        &Meshes[0].Local2WorldMatrix,
        move(MergedVertices),
        move(MergedNaniteData),
        move(MergedNaniteClusterProxy)
    );

    PipelineInterface::GetInstance().CreateConstantBuffer(ActorInstance.get());

    // Set material (阶段1: use first material)
    if (!MaterialInstances.empty())
    {
        ActorInstance->SetMaterial(move(MaterialInstances[0]), move(MaterialProxyInstances[0]));
    }

    ActorInstance->Transform.Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    ActorInstance->Transform.Rotation = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);  // Identity quaternion
    ActorInstance->Transform.Scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    ActorInstance->Name = Meshes[0].Name;

    StaticMeshes.push_back(move(ActorInstance));
    
    return static_cast<int>(Meshes.size());
}

StaticMesh* Level::InstantiateCullingVisualCamera()
{
    const string CameraPath = FileTool::GetInstance().GetMeshFullPath("mp5_sil.fbx");

    vector<Mesh> Meshes;
    vector<PBRTextureNamesPatch> TextureNamesPatches;
    MeshLoader::GetInstance().LoadMesh(CameraPath, Meshes, TextureNamesPatches);

    // Merge all submesh vertices and indices (transforms already baked by MeshLoader)
    vector<Vertex> MergedVertices;
    vector<unsigned int> MergedIndices;

    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unsigned int BaseVertex = static_cast<unsigned int>(MergedVertices.size());

        MergedVertices.insert(MergedVertices.end(),
                             Meshes[I].Vertices.begin(),
                             Meshes[I].Vertices.end());

        // Append indices with base vertex offset
        for (size_t IdxOffset = 0; IdxOffset < Meshes[I].Indices.size(); ++IdxOffset)
        {
            MergedIndices.push_back(Meshes[I].Indices[IdxOffset] + BaseVertex);
        }
    }

    // Run Nanite ONCE on merged data
    vector<ClusterData> MergedClusters;
    vector<CLODBound> MergedGroupBounds;

    if (!MergedVertices.empty() && !MergedIndices.empty())
    {
        Mesh MergedMesh;
        MergedMesh.Vertices = MergedVertices;
        MergedMesh.Indices = MergedIndices;
        MergedMesh.Local2WorldMatrix = Meshes[0].Local2WorldMatrix;
        MergedMesh.BoundingSphere = Meshes[0].BoundingSphere;

        MeshLoader::GetInstance().Nanite(MergedMesh, MergedClusters, MergedGroupBounds);
    }

    unique_ptr<NaniteData> MergedNaniteData = make_unique<NaniteData>();
    MergedNaniteData->Clusters = move(MergedClusters);
    MergedNaniteData->GroupBounds = move(MergedGroupBounds);

    unique_ptr<NaniteClusterProxy> MergedNaniteClusterProxy = make_unique<NaniteClusterProxy>();

    unique_ptr<Material> MaterialInstance = make_unique<Material>();
    unique_ptr<MaterialProxy> MaterialProxyInstance = make_unique<MaterialProxy>();

    unique_ptr<CullingVisualCamera> CullingVisualInstance = make_unique<CullingVisualCamera>(
        &Meshes[0].Local2WorldMatrix,
        move(MergedVertices),
        move(MergedNaniteData),
        move(MergedNaniteClusterProxy));

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
    ActorInstance->Transform.Position.y = 500.0f;
    ActorInstance->Transform.Position.z = 0;
    ActorInstance->LookDirection = XMFLOAT3(1.0f, 0, 0);
    ActorInstance->UpDirection = XMFLOAT3(0, 1.0f, 0);
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

    //ActorInstance->IrradianceMap = Cubemap->Convolution(32, 1024);
    ActorInstance->IrradianceMap = Cubemap->Convolution(32, 32);
    ActorInstance->IrradianceMapProxy = std::make_unique<CubemapTextureProxy>();
    ActorInstance->IrradianceMapProxy->DescriptorIndex = PipelineInterface::GetInstance().GetCubemapBindlessAllocator().AllocateRange(1);

    //ActorInstance->PrefilteredMap = Cubemap->PrefilterEnvironment(5, 128, 1024);
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
    ActorInstance->Name = ActorInstance->GetHDRFilePath();
    SkyLights.push_back(move(ActorInstance));

    return SkyLights.back().get();
}

void Level::CreateGlobalMeshBuffers()
{
    // GPU-Driven: Create global merged mesh buffers after all meshes are loaded
    PipelineInterface::GetInstance().ResetUploadCommandList();
    PipelineInterface::GetInstance().CreateGlobalMergedMeshBuffers(this);
    PipelineInterface::GetInstance().ExecuteAndWaitUploadCommandList();
}

void Level::Update(float DeletaTime, unsigned int FrameIndex) const
{
    for (const unique_ptr<Camera>& Actor : Cameras)
    {
        Actor->Update(DeletaTime, FrameIndex);
    }
    
    for (const unique_ptr<StaticMesh>& Actor : StaticMeshes)
    {
        Actor->Update(DeletaTime, FrameIndex);
    }
    
    for (const unique_ptr<SkyLight>& Actor : SkyLights)
    {
        Actor->Update(DeletaTime, FrameIndex);
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

std::vector<Actor*> Level::GetSelectableActors() const
{
    vector<Actor*> Result;

    for (int I = 0; I < StaticMeshes.size(); ++I)
    {
        Result.push_back(StaticMeshes[I].get());
    }

    for (int I = 0; I < SkyLights.size(); ++I)
    {
        Result.push_back(SkyLights[I].get());
    }
    
    return Result;
}

Level::~Level()
{
    StaticMeshes.clear();
    Cameras.clear();
    SkyLights.clear();
}
