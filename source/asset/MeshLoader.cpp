#include "MeshLoader.h"
#include "Texture.h"
#include "../misc/Math.h"
#include "DirectXMath.h"
#include <algorithm>
#include <stdio.h>
#include <assimp/postprocess.h>

#define CLUSTERLOD_IMPLEMENTATION
#include "../../thirdpatry/meshoptimizer/src/meshoptimizer.h"
#include "../../thirdpatry/meshoptimizer/demo/clusterlod.h"

using namespace std;
using namespace DirectX;

// computes approximate (perspective) projection error of a cluster in screen space (0..1; multiply by screen height to get pixels)
// camera_proj is projection[1][1], or cot(fovy/2); camera_znear is *positive* near plane distance
// for DAG cut to be valid, boundsError must be monotonic: it must return a larger error for parent cluster
// for simplicity, we ignore perspective distortion and use rotationally invariant projection size estimation
static float boundsError(const clodBounds& bounds, float camera_x, float camera_y, float camera_z, float camera_proj, float camera_znear)
{
    float dx = bounds.center[0] - camera_x, dy = bounds.center[1] - camera_y, dz = bounds.center[2] - camera_z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz) - bounds.radius;
    return bounds.error / (d > camera_znear ? d : camera_znear) * (camera_proj * 0.5f);
}

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
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga") != std::string::npos)
    {
        OutTextureNamesPatch.NormalPath = "Textures\\Cerberus_N.tga";
    }
    
    if (Material->GetTexture(aiTextureType_METALNESS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.MetallicPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga") != std::string::npos)
    {
        OutTextureNamesPatch.MetallicPath = "Textures\\Cerberus_M.tga";
    }
    
    if (Material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.RoughnessPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga") != std::string::npos)
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

ErrorCode MeshLoader::Nanite(const Mesh& Mesh, std::vector<ClusterData>& OutClusters, std::vector<CLODBound>& OutGroupBounds)
{
    // Configure Nanite LOD hierarchy generation
    clodConfig Config = clodDefaultConfig(126);

    // Mesh Shader hardware limit: 64 vertices per cluster
    Config.max_vertices = 64;

    // Reduce 50% triangles per level (UE5 Nanite standard)
    Config.simplify_ratio = 0.5f;

    // Skip level if simplification < 20% effective
    Config.simplify_threshold = 0.80f;

    // Parent error additive factor for smooth LOD transitions
    Config.simplify_error_merge_additive = 0.5f;

    // Normal attribute weights (equal importance to position)
    const float AttributeWeights[3] = {0.5f, 0.5f, 0.5f};

    // Setup input mesh data
    clodMesh CMesh = {};
    CMesh.indices = Mesh.Indices.data();
    CMesh.index_count = Mesh.Indices.size();
    CMesh.vertex_count = Mesh.Vertices.size();
    CMesh.vertex_positions = &Mesh.Vertices[0].Position.x;
    CMesh.vertex_positions_stride = sizeof(Vertex);
    CMesh.vertex_attributes = &Mesh.Vertices[0].Normal.x;
    CMesh.vertex_attributes_stride = sizeof(Vertex);
    CMesh.attribute_weights = AttributeWeights;
    CMesh.attribute_count = 3;

    // Protect UV seams from simplification (bits 6-7 are UV coords)
    CMesh.attribute_protect_mask = (1 << 6) | (1 << 7);
    CMesh.vertex_lock = nullptr;

    OutClusters.clear();
    OutGroupBounds.clear();

    // Build Nanite LOD hierarchy
    clodBuild(Config, CMesh, [&](clodGroup Group, const clodCluster* Clusters,
                                 size_t ClusterCount) -> int {

        for (size_t I = 0; I < ClusterCount; ++I)
        {
            const clodCluster& Cluster = Clusters[I];
            ClusterData Data;

            // Deduplicate vertices: global indices -> unique vertices + local indices (0-255)
            Data.UniqueVertices.resize(Cluster.vertex_count);
            Data.LocalIndices.resize(Cluster.index_count);

            size_t UniqueCount = clodLocalIndices(
                Data.UniqueVertices.data(),
                Data.LocalIndices.data(),
                Cluster.indices,
                Cluster.index_count
            );

            Data.UniqueVertices.resize(UniqueCount);

            // Refined: index to finer group (-1 = leaf node)
            Data.Refined = Cluster.refined;

            // Bounds: center/radius for frustum culling, error for LOD selection
            Data.Bound.Center[0] = Cluster.bounds.center[0];
            Data.Bound.Center[1] = Cluster.bounds.center[1];
            Data.Bound.Center[2] = Cluster.bounds.center[2];
            Data.Bound.Radius = Cluster.bounds.radius;
            Data.Bound.Error = Cluster.bounds.error;

            // GroupId: index to OutGroupBounds for runtime cluster selection
            Data.GroupId = int(OutGroupBounds.size());

            OutClusters.push_back(std::move(Data));
        }

        // Group simplified bounds (monotonic error: parent > child)
        CLODBound GroupBound;
        GroupBound.Center[0] = Group.simplified.center[0];
        GroupBound.Center[1] = Group.simplified.center[1];
        GroupBound.Center[2] = Group.simplified.center[2];
        GroupBound.Radius = Group.simplified.radius;
        GroupBound.Error = Group.simplified.error;
        OutGroupBounds.push_back(GroupBound);

        // Return group index for parent cluster's refined field
        return int(OutGroupBounds.size() - 1);
    });

    return ErrorCode::OK;
}