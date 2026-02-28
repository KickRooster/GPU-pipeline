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

// Global material ID counter to avoid conflicts across multiple meshes
static unsigned int GlobalMaterialIDCounter = 0;

int Level::InstantiateStaticMeshes(const string& Path)
{
    vector<Mesh> Meshes;
    vector<PBRTextureNamesPatch> TextureNamesPatches;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes, TextureNamesPatches);

    // Merge all submesh vertices and indices (transforms already baked by MeshLoader)
    vector<Vertex> MergedVertices;
    vector<unsigned int> MergedIndices;
    vector<unsigned int> TriangleMaterialIDs;  // Track which submesh each triangle came from

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

        // Assign global material ID for each triangle in this submesh
        size_t TriangleCount = Meshes[I].Indices.size() / 3;
        for (size_t TriIdx = 0; TriIdx < TriangleCount; ++TriIdx)
        {
            TriangleMaterialIDs.push_back(GlobalMaterialIDCounter + I);  // Global material ID
        }
    }

    // Update global material ID counter
    GlobalMaterialIDCounter += static_cast<unsigned int>(Meshes.size());

    // Run Nanite ONCE on merged data with material tracking
    vector<ClusterData> MergedClusters;
    vector<CLODBound> MergedGroupBounds;
    Mesh MergedMesh;

    if (!MergedVertices.empty() && !MergedIndices.empty())
    {
        MergedMesh.Vertices = MergedVertices;
        MergedMesh.Indices = MergedIndices;
        MergedMesh.Local2WorldMatrix = Meshes[0].Local2WorldMatrix;

        // Compute bounding sphere for merged mesh (centroid + max distance)
        XMVECTOR CentroidSum = XMVectorZero();
        for (const auto& V : MergedVertices)
            CentroidSum = XMVectorAdd(CentroidSum, XMLoadFloat3(&V.Position));
        XMVECTOR Centroid = XMVectorScale(CentroidSum, 1.0f / static_cast<float>(MergedVertices.size()));
        float MaxDistSq = 0.0f;
        for (const auto& V : MergedVertices)
        {
            XMVECTOR Diff = XMVectorSubtract(XMLoadFloat3(&V.Position), Centroid);
            float DistSq = XMVectorGetX(XMVector3LengthSq(Diff));
            if (DistSq > MaxDistSq) MaxDistSq = DistSq;
        }
        XMFLOAT3 CentroidF;
        XMStoreFloat3(&CentroidF, Centroid);
        MergedMesh.BoundingSphere = XMFLOAT4(CentroidF.x, CentroidF.y, CentroidF.z, sqrtf(MaxDistSq));

        MeshLoader::GetInstance().Nanite(MergedMesh, TriangleMaterialIDs, MergedClusters, MergedGroupBounds);
    }

    // Create single NaniteData and NaniteClusterProxy for the merged mesh
    unique_ptr<NaniteData> MergedNaniteData = make_unique<NaniteData>();
    MergedNaniteData->Clusters = move(MergedClusters);
    MergedNaniteData->GroupBounds = move(MergedGroupBounds);

    // Verify Nanite hierarchy against Nanite implementation standards
    //MeshLoader::VerifyNaniteHierarchy(*MergedNaniteData, MergedMesh, Path);

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

    // Create textures for ALL materials (multi-material support)
    for (unsigned int I = 0; I < MaterialInstances.size(); ++I)
    {
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
    
    // Create single StaticMesh actor with merged Nanite data
    unique_ptr<StaticMesh> ActorInstance = make_unique<StaticMesh>(
        &Meshes[0].Local2WorldMatrix,
        move(MergedVertices),
        move(MergedNaniteData),
        move(MergedNaniteClusterProxy)
    );

    PipelineInterface::GetInstance().CreateConstantBuffer(ActorInstance.get());

    if (!MaterialProxyInstances.empty())
    {
        unique_ptr<Material> FirstMat = make_unique<Material>();
        unique_ptr<MaterialProxy> FirstProxy = make_unique<MaterialProxy>(*MaterialProxyInstances[0]);
        ActorInstance->SetMaterial(move(FirstMat), move(FirstProxy));
    }

    // Store all material proxies in global array (indexed by global material ID)
    for (unsigned int I = 0; I < MaterialProxyInstances.size(); ++I)
    {
        AllMaterialProxies.push_back(*MaterialProxyInstances[I]);
    }

    // Store all materials to keep GPU texture resources alive
    for (unsigned int I = 0; I < MaterialInstances.size(); ++I)
    {
        AllMaterials.push_back(move(MaterialInstances[I]));
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
    vector<unsigned int> TriangleMaterialIDs;

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

        // Assign global material ID for each triangle
        size_t TriangleCount = Meshes[I].Indices.size() / 3;
        for (size_t TriIdx = 0; TriIdx < TriangleCount; ++TriIdx)
        {
            TriangleMaterialIDs.push_back(GlobalMaterialIDCounter + I);  // Global material ID
        }
    }

    // Update global material ID counter
    GlobalMaterialIDCounter += static_cast<unsigned int>(Meshes.size());

    // Run Nanite ONCE on merged data with material tracking
    vector<ClusterData> MergedClusters;
    vector<CLODBound> MergedGroupBounds;

    if (!MergedVertices.empty() && !MergedIndices.empty())
    {
        Mesh MergedMesh;
        MergedMesh.Vertices = MergedVertices;
        MergedMesh.Indices = MergedIndices;
        MergedMesh.Local2WorldMatrix = Meshes[0].Local2WorldMatrix;

        // Compute bounding sphere for merged mesh (centroid + max distance)
        XMVECTOR CentroidSum = XMVectorZero();
        for (const auto& V : MergedVertices)
            CentroidSum = XMVectorAdd(CentroidSum, XMLoadFloat3(&V.Position));
        XMVECTOR Centroid = XMVectorScale(CentroidSum, 1.0f / static_cast<float>(MergedVertices.size()));
        float MaxDistSq = 0.0f;
        for (const auto& V : MergedVertices)
        {
            XMVECTOR Diff = XMVectorSubtract(XMLoadFloat3(&V.Position), Centroid);
            float DistSq = XMVectorGetX(XMVector3LengthSq(Diff));
            if (DistSq > MaxDistSq) MaxDistSq = DistSq;
        }
        XMFLOAT3 CentroidF;
        XMStoreFloat3(&CentroidF, Centroid);
        MergedMesh.BoundingSphere = XMFLOAT4(CentroidF.x, CentroidF.y, CentroidF.z, sqrtf(MaxDistSq));

        MeshLoader::GetInstance().Nanite(MergedMesh, TriangleMaterialIDs, MergedClusters, MergedGroupBounds);
    }

    // Verify multi-material support
    //MeshLoader::VerifyNaniteHierarchy(...);

    unique_ptr<NaniteData> MergedNaniteData = make_unique<NaniteData>();
    MergedNaniteData->Clusters = move(MergedClusters);
    MergedNaniteData->GroupBounds = move(MergedGroupBounds);

    unique_ptr<NaniteClusterProxy> MergedNaniteClusterProxy = make_unique<NaniteClusterProxy>();

    vector<unique_ptr<Material>> MaterialInstances;
    vector<unique_ptr<MaterialProxy>> MaterialProxyInstances;

    for (unsigned int I = 0; I < TextureNamesPatches.size(); ++I)
    {
        unique_ptr<Material> MatInstance = make_unique<Material>();
        unique_ptr<MaterialProxy> MatProxyInstance = make_unique<MaterialProxy>();

        if (!TextureNamesPatches[I].AlbedoPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].AlbedoPath, true, TextureInstance) == ErrorCode::OK)
            {
                MatInstance->AlbedoTexture = make_unique<Texture>(std::move(TextureInstance));
                MatProxyInstance->AlbedoTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].NormalPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].NormalPath, false, TextureInstance) == ErrorCode::OK)
            {
                MatInstance->NormalTexture = make_unique<Texture>(std::move(TextureInstance));
                MatProxyInstance->NormalTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].MetallicPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].MetallicPath, false, TextureInstance) == ErrorCode::OK)
            {
                MatInstance->MetallicTexture = make_unique<Texture>(std::move(TextureInstance));
                MatProxyInstance->MetallicTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        if (!TextureNamesPatches[I].RoughnessPath.empty())
        {
            Texture TextureInstance;
            if (TextureLoader::GetInstance().LoadTexture(TextureNamesPatches[I].RoughnessPath, false, TextureInstance) == ErrorCode::OK)
            {
                MatInstance->RoughnessTexture = make_unique<Texture>(std::move(TextureInstance));
                MatProxyInstance->RoughnessTextureIndex = PipelineInterface::GetInstance().GetTextureBindlessAllocator().AllocateRange(1);
            }
        }

        MaterialInstances.push_back(move(MatInstance));
        MaterialProxyInstances.push_back(move(MatProxyInstance));
    }

    PipelineInterface::GetInstance().ResetUploadCommandList();

    // Upload all materials' textures
    for (unsigned int I = 0; I < MaterialInstances.size(); ++I)
    {
        if (MaterialInstances[I]->AlbedoTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->AlbedoTexture.get(),
                MaterialProxyInstances[I]->AlbedoTextureIndex,
                TextureProxyInstance.get(), false);
            MaterialInstances[I]->AlbedoTextureProxy = move(TextureProxyInstance);
        }
        if (MaterialInstances[I]->NormalTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->NormalTexture.get(),
                MaterialProxyInstances[I]->NormalTextureIndex,
                TextureProxyInstance.get(), false);
            MaterialInstances[I]->NormalTextureProxy = move(TextureProxyInstance);
        }
        if (MaterialInstances[I]->MetallicTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->MetallicTexture.get(),
                MaterialProxyInstances[I]->MetallicTextureIndex,
                TextureProxyInstance.get(), false);
            MaterialInstances[I]->MetallicTextureProxy = move(TextureProxyInstance);
        }
        if (MaterialInstances[I]->RoughnessTexture)
        {
            unique_ptr<TextureProxy> TextureProxyInstance = make_unique<TextureProxy>();
            PipelineInterface::GetInstance().CreateTexture(
                MaterialInstances[I]->RoughnessTexture.get(),
                MaterialProxyInstances[I]->RoughnessTextureIndex,
                TextureProxyInstance.get(), false);
            MaterialInstances[I]->RoughnessTextureProxy = move(TextureProxyInstance);
        }
    }

    PipelineInterface::GetInstance().ExecuteAndWaitUploadCommandList();

    unique_ptr<CullingVisualCamera> CullingVisualInstance = make_unique<CullingVisualCamera>(
        &Meshes[0].Local2WorldMatrix,
        move(MergedVertices),
        move(MergedNaniteData),
        move(MergedNaniteClusterProxy));

    // Store all material proxies in global array
    for (unsigned int I = 0; I < MaterialProxyInstances.size(); ++I)
    {
        AllMaterialProxies.push_back(*MaterialProxyInstances[I]);
    }

    // Store all materials to keep GPU texture resources alive
    for (unsigned int I = 0; I < MaterialInstances.size(); ++I)
    {
        AllMaterials.push_back(move(MaterialInstances[I]));
    }

    if (!MaterialProxyInstances.empty())
    {
        unique_ptr<Material> FirstMat = make_unique<Material>();
        unique_ptr<MaterialProxy> FirstProxy = make_unique<MaterialProxy>(*MaterialProxyInstances[0]);
        CullingVisualInstance->SetMaterial(move(FirstMat), move(FirstProxy));
    }

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
    ActorInstance->Transform.Position.x = 12.7424021f;
    ActorInstance->Transform.Position.y = 69.4671021f;
    ActorInstance->Transform.Position.z = 232.522491f;
    ActorInstance->LookDirection = XMFLOAT3(0.0334060229f, 0.0364667103f, -0.998776376);
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
    PipelineInterface::GetInstance().ReleaseGlobalUploadBuffers();
}

void Level::Update(float DeltaTime, unsigned int FrameIndex) const
{
    for (const unique_ptr<Camera>& Actor : Cameras)
    {
        Actor->Update(DeltaTime, FrameIndex);
    }
    
    for (const unique_ptr<StaticMesh>& Actor : StaticMeshes)
    {
        Actor->Update(DeltaTime, FrameIndex);
    }
    
    for (const unique_ptr<SkyLight>& Actor : SkyLights)
    {
        Actor->Update(DeltaTime, FrameIndex);
    }
}

const vector<unique_ptr<StaticMesh>>& Level::GetStaticMeshes() const
{
    return StaticMeshes;
}

vector<Camera*> Level::GetCameras() const
{
    vector<Camera*> Result;

    for (size_t I = 0; I < Cameras.size(); ++I)
    {
        Result.push_back(Cameras[I].get());
    }

    return Result;
}

vector<SkyLight*> Level::GetSkyLights() const
{
    vector<SkyLight*> Result;

    for (size_t I = 0; I < SkyLights.size(); ++I)
    {
        Result.push_back(SkyLights[I].get());
    }

    return Result;
}

std::vector<Actor*> Level::GetSelectableActors() const
{
    vector<Actor*> Result;

    for (size_t I = 0; I < StaticMeshes.size(); ++I)
    {
        Result.push_back(StaticMeshes[I].get());
    }

    for (size_t I = 0; I < SkyLights.size(); ++I)
    {
        Result.push_back(SkyLights[I].get());
    }
    
    return Result;
}

const std::vector<MaterialProxy>& Level::GetAllMaterialProxies() const
{
    return AllMaterialProxies;
}

void Level::Clear()
{
    StaticMeshes.clear();
    Cameras.clear();
    SkyLights.clear();
    AllMaterials.clear();
    AllMaterialProxies.clear();
}

Level::~Level()
{
    Clear();
}
