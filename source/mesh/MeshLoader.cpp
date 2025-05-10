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