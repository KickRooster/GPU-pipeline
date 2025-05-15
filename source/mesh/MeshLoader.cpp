#include "MeshLoader.h"

using namespace std;
using namespace DirectX;

void MeshLoader::ProcessNode(aiNode* Node, const aiScene* Scene, vector<Mesh>& OutMeshes)
{
    for (unsigned int I = 0; I < Node->mNumMeshes; ++I)
    {
        aiMesh* Mesh = Scene->mMeshes[Node->mMeshes[I]];
        ProcessMesh(Mesh, Scene, OutMeshes);
    }

    for (unsigned int I = 0; I < Node->mNumChildren; ++I)
    {
        ProcessNode(Node->mChildren[I], Scene, OutMeshes);
    }
}

void MeshLoader::ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, vector<Mesh>& OutMeshes)
{
    Mesh OutMesh;
    
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

    OutMeshes.push_back(OutMesh);

    // if (Mesh->mMaterialIndex >= 0u)
    // {
    //     aiMaterial* material = Scene->mMaterials[Mesh->mMaterialIndex];
    //
    //     std::vector<Texture> diffuseMaps = LoadMaterialTextures(material, 
    //         aiTextureType_DIFFUSE, "texture_diffuse");
    //     textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    //
    //     std::vector<Texture> specularMaps = LoadMaterialTextures(material, 
    //         aiTextureType_SPECULAR, "texture_specular");
    //     textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // }
}

ErrorCode MeshLoader::LoadMesh(const std::string& Path, vector<Mesh>& OutMeshes)
{
    Assimp::Importer Importer;

    unsigned int PostProcessFlags = 
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

    ProcessNode(Scene->mRootNode, Scene, OutMeshes);

    return ErrorCode::OK;
}

ErrorCode MeshLoader::GenerateMeshletData(const std::vector<Mesh>& Meshes, std::vector<MeshletData>& OutMeshletData) const
{
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        MeshletData MeshletDataInstance;
    
        constexpr size_t MaxVertexCountPerMeshlet = 64;
        constexpr size_t MaxTriangleCountPerMeshlet = 124;
        constexpr float ConeWeight = 0.7f;
        
        const size_t MaxMeshletCount = meshopt_buildMeshletsBound(
            Meshes[I].Indices.size(), MaxVertexCountPerMeshlet, MaxTriangleCountPerMeshlet);
    
        MeshletDataInstance.Meshlets.resize(MaxMeshletCount);
        MeshletDataInstance.MeshletVertices.resize(MaxMeshletCount * MaxVertexCountPerMeshlet);
        MeshletDataInstance.MeshletIndices.resize(MaxMeshletCount * MaxTriangleCountPerMeshlet * 3);
        
        const size_t MeshletCount = meshopt_buildMeshlets(
            MeshletDataInstance.Meshlets.data(),
            MeshletDataInstance.MeshletVertices.data(),
            MeshletDataInstance.MeshletIndices.data(),
            Meshes[I].Indices.data(),
            Meshes[I].Indices.size(),
            &Meshes[I].Vertices[0].Position.x,
            Meshes[I].Vertices.size(),
            sizeof(Vertex),
            MaxVertexCountPerMeshlet,
            MaxTriangleCountPerMeshlet,
            ConeWeight);
    
        MeshletDataInstance.Meshlets.resize(MeshletCount);

        size_t TotalVertexCount = 0;
        size_t TotalTriangleCount = 0;
    
        for (size_t J = 0; J < MeshletCount; ++J)
        {
            TotalVertexCount += MeshletDataInstance.Meshlets[J].vertex_count;
            TotalTriangleCount += MeshletDataInstance.Meshlets[J].triangle_count;
        }
    
        MeshletDataInstance.MeshletVertices.resize(TotalVertexCount);
        MeshletDataInstance.MeshletIndices.resize(TotalTriangleCount * 3);

        OutMeshletData.push_back(MeshletDataInstance);
    }

    return ErrorCode::OK;
}