#include "MeshLoader.h"
#include "Texture.h"
#include "../misc/Math.h"
#include "DirectXMath.h"

using namespace std;
using namespace DirectX;

void MeshLoader::ExtractPBRTextures(const aiMaterial* Material, PBRTextureNamesPatch& OutTextureNamesPatch)
{
    aiString Path;
    
    if (Material->GetTexture(aiTextureType_DIFFUSE, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.AlbedoPath = Path.C_Str();
    }
    else if (Material->GetTexture(aiTextureType_BASE_COLOR, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.AlbedoPath = Path.C_Str();
    }
    
    if (Material->GetTexture(aiTextureType_NORMALS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.NormalPath = Path.C_Str();
    }
    else if (Material->GetTexture(aiTextureType_HEIGHT, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.NormalPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga"))
    {
        OutTextureNamesPatch.NormalPath = "Textures\\Cerberus_N.tga";
    }
    
    if (Material->GetTexture(aiTextureType_METALNESS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.MetallicPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga"))
    {
        OutTextureNamesPatch.MetallicPath = "Textures\\Cerberus_M.tga";
    }
    
    if (Material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.RoughnessPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga"))
    {
        OutTextureNamesPatch.RoughnessPath = "Textures\\Cerberus_R.tga";
    }
    
    aiTextureType OtherTypes[] = {
        aiTextureType_SPECULAR,
        aiTextureType_AMBIENT,
        aiTextureType_EMISSIVE,
        aiTextureType_SHININESS,
        aiTextureType_OPACITY,
        aiTextureType_DISPLACEMENT,
        aiTextureType_LIGHTMAP,
        aiTextureType_REFLECTION,
        aiTextureType_BASE_COLOR,
        aiTextureType_NORMAL_CAMERA,
        aiTextureType_EMISSION_COLOR,
        aiTextureType_AMBIENT_OCCLUSION,
        aiTextureType_SHEEN,
        aiTextureType_CLEARCOAT,
        aiTextureType_TRANSMISSION,
        aiTextureType_MAYA_BASE,
        aiTextureType_MAYA_SPECULAR,
        aiTextureType_MAYA_SPECULAR_COLOR,
        aiTextureType_MAYA_SPECULAR_ROUGHNESS,
        aiTextureType_ANISOTROPY,
        aiTextureType_GLTF_METALLIC_ROUGHNESS,
        aiTextureType_UNKNOWN
    };
    
    for (aiTextureType Type : OtherTypes)
    {
        if (Material->GetTexture(Type, 0, &Path) == AI_SUCCESS)
        {
            OutTextureNamesPatch.OtherTexturePaths.push_back(Path.C_Str());
        }
    }
}

void MeshLoader::ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, const XMMATRIX& NodeTransform, vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches)
{
    Mesh OutMesh;
    PBRTextureNamesPatch OutTextureNamesPatch;

    XMStoreFloat4x4(&OutMesh.Local2WorldMatrix, NodeTransform);
    
    for (unsigned int I = 0; I < AssimpMesh->mNumVertices; ++I)
    {
        Vertex Vertex;
        
        Vertex.Position.x = AssimpMesh->mVertices[I].x;
        Vertex.Position.y = AssimpMesh->mVertices[I].y;
        Vertex.Position.z = AssimpMesh->mVertices[I].z;

        if (AssimpMesh->HasNormals())
        {
            Vertex.Normal.x = AssimpMesh->mNormals[I].x;
            Vertex.Normal.y = AssimpMesh->mNormals[I].y;
            Vertex.Normal.z = AssimpMesh->mNormals[I].z;
        }
        else
        {
            Vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
        }

        if (AssimpMesh->HasTangentsAndBitangents())
        {
            Vertex.Tangent.x = AssimpMesh->mTangents[I].x;
            Vertex.Tangent.y = AssimpMesh->mTangents[I].y;
            Vertex.Tangent.z = AssimpMesh->mTangents[I].z;
        }
        else
        {
            Vertex.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
        }

        if (AssimpMesh->HasVertexColors(0))
        {
            Vertex.Color.x = AssimpMesh->mColors[0]->r;
            Vertex.Color.y = AssimpMesh->mColors[0]->g;
            Vertex.Color.z = AssimpMesh->mColors[0]->b;
            Vertex.Color.w = AssimpMesh->mColors[0]->a;
        }
        else
        {
            Vertex.Color.x = 1.0f;
            Vertex.Color.y = 1.0f;
            Vertex.Color.z = 1.0f;
            Vertex.Color.w = 1.0f;
        }

        if (AssimpMesh->mTextureCoords[0])
        {
            Vertex.UV0.x = AssimpMesh->mTextureCoords[0][I].x;
            Vertex.UV0.y = AssimpMesh->mTextureCoords[0][I].y;
        }
        else
        {
            Vertex.UV0 = XMFLOAT2(0.0f, 0.0f);
        }

        OutMesh.Vertices.push_back(Vertex);
    }

    for (unsigned int I = 0; I < AssimpMesh->mNumFaces; ++I)
    {
        aiFace Face = AssimpMesh->mFaces[I];
        
        for (unsigned int J = 0; J < Face.mNumIndices; ++J)
        {
            OutMesh.Indices.push_back(Face.mIndices[J]);
        }
    }
    
    vector<float> Positions;
    Positions.reserve(OutMesh.Vertices.size() * 3);
    for (const auto& Vertex : OutMesh.Vertices)
    {
        Positions.push_back(Vertex.Position.x);
        Positions.push_back(Vertex.Position.y);
        Positions.push_back(Vertex.Position.z);
    }
    
    meshopt_Bounds SphereBounds = meshopt_computeSphereBounds(
        Positions.data(),
        OutMesh.Vertices.size(),
        sizeof(float) * 3,
        nullptr,
        0);
    
    OutMesh.BoundingSphere.x = SphereBounds.center[0];
    OutMesh.BoundingSphere.y = SphereBounds.center[1]; 
    OutMesh.BoundingSphere.z = SphereBounds.center[2];
    OutMesh.BoundingSphere.w = SphereBounds.radius;

    OutMesh.Name = AssimpMesh->mName.C_Str();
    aiMaterial* Material = Scene->mMaterials[AssimpMesh->mMaterialIndex];
    ExtractPBRTextures(Material, OutTextureNamesPatch);
    OutTextureNamesPatches.push_back(OutTextureNamesPatch);
    OutMeshes.push_back(OutMesh);
}

void MeshLoader::ProcessNode(aiNode* Node, const aiScene* Scene, const XMMATRIX& ParentTransform, vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches)
{
    const XMMATRIX NodeTransform = MathTool::GetInstance().AssimpMatrixToXMMatrix(Node->mTransformation);
    const XMMATRIX WorldTransform = ParentTransform * NodeTransform;
    
    for (unsigned int I = 0; I < Node->mNumMeshes; I++)
    {
        aiMesh* AssimpMesh = Scene->mMeshes[Node->mMeshes[I]];
        ProcessMesh(AssimpMesh, Scene, WorldTransform, OutMeshes, OutTextureNamesPatches);
    }
    
    for (unsigned int I = 0; I < Node->mNumChildren; I++)
    {
        ProcessNode(Node->mChildren[I], Scene, WorldTransform, OutMeshes, OutTextureNamesPatches);
    }
}

void MeshLoader::GenerateMeshletData(const vector<Vertex>& Vertices, const vector<unsigned int>& Indices, MeshletData& OutMeshletData) const
{
    constexpr size_t MaxVertexCountPerMeshlet = 64;
    constexpr size_t MaxTriangleCountPerMeshlet = 124;
    constexpr float ConeWeight = 0.25f;
    
    MeshletDataForMeshOptimizer NativeMeshletData;
    
    const size_t MaxMeshletCount = meshopt_buildMeshletsBound(
        Indices.size(), MaxVertexCountPerMeshlet, MaxTriangleCountPerMeshlet);

    NativeMeshletData.Meshlets.resize(MaxMeshletCount);
    NativeMeshletData.MeshletVertices.resize(MaxMeshletCount * MaxVertexCountPerMeshlet);
    NativeMeshletData.MeshletIndices.resize(MaxMeshletCount * MaxTriangleCountPerMeshlet * 3);
    
    const size_t MeshletCount = meshopt_buildMeshlets(
        NativeMeshletData.Meshlets.data(),
        NativeMeshletData.MeshletVertices.data(),
        NativeMeshletData.MeshletIndices.data(),
        Indices.data(),
        Indices.size(),
        &Vertices[0].Position.x,
        Vertices.size(),
        sizeof(Vertex),
        MaxVertexCountPerMeshlet,
        MaxTriangleCountPerMeshlet,
        ConeWeight);

    const meshopt_Meshlet& LastMeshlet = NativeMeshletData.Meshlets[MeshletCount - 1];
    NativeMeshletData.MeshletVertices.resize(LastMeshlet.vertex_offset + LastMeshlet.vertex_count);
    NativeMeshletData.MeshletIndices.resize(LastMeshlet.triangle_offset + ((LastMeshlet.triangle_count * 3 + 3) & ~3));
    NativeMeshletData.Meshlets.resize(MeshletCount);

    for (size_t J = 0; J < NativeMeshletData.Meshlets.size(); ++J)
    {
        const meshopt_Meshlet& CurrentMeshlet = NativeMeshletData.Meshlets[J];
        meshopt_optimizeMeshlet(
            &NativeMeshletData.MeshletVertices[CurrentMeshlet.vertex_offset],
            &NativeMeshletData.MeshletIndices[CurrentMeshlet.triangle_offset],
            CurrentMeshlet.triangle_count,
            CurrentMeshlet.vertex_count);
    }

    for (size_t J = 0; J < NativeMeshletData.Meshlets.size(); ++J)
    {
        const meshopt_Meshlet& CurrentMeshlet = NativeMeshletData.Meshlets[J];
        meshopt_Bounds Bounds = meshopt_computeMeshletBounds(
                &NativeMeshletData.MeshletVertices[CurrentMeshlet.vertex_offset],
                &NativeMeshletData.MeshletIndices[CurrentMeshlet.triangle_offset],
                CurrentMeshlet.triangle_count,
                &Vertices[0].Position.x,
                Vertices.size(),
                sizeof(Vertex));
        NativeMeshletData.MeshletBounds.push_back(Bounds);
    }
    
    OutMeshletData.Meshlets = move(NativeMeshletData.Meshlets);
    OutMeshletData.MeshletVertices = move(NativeMeshletData.MeshletVertices);
    
    OutMeshletData.MeshletIndices.resize(NativeMeshletData.MeshletIndices.size());
    for (size_t I = 0; I < NativeMeshletData.MeshletIndices.size(); ++I)
    {
        OutMeshletData.MeshletIndices[I] = static_cast<unsigned int>(NativeMeshletData.MeshletIndices[I]);
    }
    
    OutMeshletData.MeshletBounds = move(NativeMeshletData.MeshletBounds);
}

ErrorCode MeshLoader::LoadMesh(const string& Path, vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches)
{
    Assimp::Importer Importer;

    constexpr unsigned int PostProcessFlags = 
        aiProcess_Triangulate |
        aiProcess_MakeLeftHanded |
        aiProcess_FlipWindingOrder |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs;
    
    const aiScene* Scene = Importer.ReadFile(Path, PostProcessFlags);

    if (!Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode)
    {
        return ErrorCode::MeshDataIncomplete;
    }

    ProcessNode(Scene->mRootNode, Scene, XMMatrixIdentity(), OutMeshes, OutTextureNamesPatches);

    return ErrorCode::OK;
}

void MeshLoader::GenerateWholeMeshLODData(const Mesh& Mesh, const MeshLODSettings& Settings, vector<MeshLODData>& OutLODDatas) const
{
    constexpr float ScreenPercentages[] = {1.0f, 0.5f, 0.25f, 0.125f};
    constexpr float TargetRetainPercentages[] = {1.0f, 0.75f, 0.5f, 0.25f};
    
    const size_t OriginalTriCount = Mesh.Indices.size() / 3;
    
    for (int I = 0; I < Settings.NumLODs; ++I)
    {
        MeshLODData LodLevel;
        
        if (I == 0)
        {
            LodLevel.Vertices = Mesh.Vertices;
            LodLevel.Indices = Mesh.Indices;
        }
        else
        {
            const float TargetRetainPercentage = TargetRetainPercentages[I];
            size_t TargetTriCount = static_cast<size_t>(OriginalTriCount * TargetRetainPercentage);
            
            size_t MinTriCount = max(static_cast<size_t>(4), OriginalTriCount / 100); // 至少保留1%的三角形
            TargetTriCount = max(TargetTriCount, MinTriCount);
            
            const size_t TargetIndexCount = TargetTriCount * 3;
            
            // 使用业界标准的error threshold
            const float TargetError = 1e-2f;
            
            const vector<unsigned int>* SourceIndices = &Mesh.Indices;
            
            if (SourceIndices->size() <= TargetIndexCount)
            {
                LodLevel.Vertices = Mesh.Vertices;
                LodLevel.Indices = *SourceIndices;
            }
            else
            {
                vector<unsigned int> SimplifiedIndices(SourceIndices->size());
                
                const size_t SimplifiedSize = meshopt_simplifySloppy(
                    SimplifiedIndices.data(),
                    SourceIndices->data(),
                    SourceIndices->size(),
                    &Mesh.Vertices[0].Position.x,
                    Mesh.Vertices.size(),
                    sizeof(Vertex),
                    TargetIndexCount,
                    TargetError);
                
                if (SimplifiedSize == 0)
                {
                    const vector<unsigned int>* FallbackIndices = (I == 1) ? &Mesh.Indices : &OutLODDatas[I-1].Indices;
                    const vector<Vertex>* FallbackVertices = (I == 1) ? &Mesh.Vertices : &OutLODDatas[I-1].Vertices;
                    
                    LodLevel.Vertices = *FallbackVertices;
                    LodLevel.Indices = *FallbackIndices;
                }
                else
                {
                    SimplifiedIndices.resize(SimplifiedSize);
                    
                    meshopt_optimizeVertexCache(
                        SimplifiedIndices.data(),
                        SimplifiedIndices.data(),
                        SimplifiedIndices.size(),
                        Mesh.Vertices.size());
                        
                    meshopt_optimizeOverdraw(
                        SimplifiedIndices.data(),
                        SimplifiedIndices.data(),
                        SimplifiedIndices.size(),
                        &Mesh.Vertices[0].Position.x,
                        Mesh.Vertices.size(),
                        sizeof(Vertex),
                        1.05f);
                    
                    LodLevel.Vertices.resize(Mesh.Vertices.size());
                    vector<unsigned int> RemappedIndices = SimplifiedIndices;
                    
                    const size_t OptimizedVertexCount = meshopt_optimizeVertexFetch(
                        LodLevel.Vertices.data(),
                        RemappedIndices.data(),
                        RemappedIndices.size(),
                        &Mesh.Vertices[0],
                        Mesh.Vertices.size(),
                        sizeof(Vertex));
                    
                    LodLevel.Vertices.resize(OptimizedVertexCount);
                    LodLevel.Indices = RemappedIndices;
                }
            }
        }
        
        LodLevel.ScreenSizePercentage = ScreenPercentages[I];
        LodLevel.ReductionPercentage = 1.0f - TargetRetainPercentages[I];
        LodLevel.MaxDeviation = (I > 0) ? 1e-2f : 0.0f;
        LodLevel.WeldingThreshold = 0.0f;
        LodLevel.bLockBoundaries = Settings.bEnableBoundaryProtection;
        LodLevel.bLockUVBoundaries = Settings.bEnableBoundaryProtection;
        LodLevel.MinTriangleCount = (I > 0) ? static_cast<int>(max(static_cast<size_t>(4), OriginalTriCount / 100)) : static_cast<int>(OriginalTriCount);

        OutLODDatas.push_back(LodLevel);
    }
}

void MeshLoader::GenerateWholeMeshletData(const vector<MeshLODData>& LODDatas, vector<unique_ptr<MeshletData>>& OutMeshletDatas) const
{
    for (size_t LodIndex = 0; LodIndex < LODDatas.size(); ++LodIndex)
    {
        OutMeshletDatas.emplace_back(make_unique<MeshletData>());
        GenerateMeshletData(LODDatas[LodIndex].Vertices, LODDatas[LodIndex].Indices, *OutMeshletDatas[LodIndex]);
    }
}