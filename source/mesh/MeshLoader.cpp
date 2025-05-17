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

ErrorCode MeshLoader::LoadCubeMesh(std::vector<Mesh>& OutMeshes)
{
    // 创建一个简单的立方体网格，使用8个顶点和12个三角形（6个面，每个面2个三角形）
    Mesh CubeMesh;
    
    // 定义8个顶点 - 立方体的8个角
    Vertex v0, v1, v2, v3, v4, v5, v6, v7;
    
    // 底面四个顶点
    v0.Position = DirectX::XMFLOAT3(-0.5f, -0.5f, -0.5f);
    v1.Position = DirectX::XMFLOAT3( 0.5f, -0.5f, -0.5f);
    v2.Position = DirectX::XMFLOAT3( 0.5f, -0.5f,  0.5f);
    v3.Position = DirectX::XMFLOAT3(-0.5f, -0.5f,  0.5f);
    
    // 顶面四个顶点
    v4.Position = DirectX::XMFLOAT3(-0.5f,  0.5f, -0.5f);
    v5.Position = DirectX::XMFLOAT3( 0.5f,  0.5f, -0.5f);
    v6.Position = DirectX::XMFLOAT3( 0.5f,  0.5f,  0.5f);
    v7.Position = DirectX::XMFLOAT3(-0.5f,  0.5f,  0.5f);
    
    // 设置每个顶点的法线
    // 底面顶点 (-Y)
    v0.Normal = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
    v1.Normal = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
    v2.Normal = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
    v3.Normal = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
    
    // 顶面顶点 (+Y)
    v4.Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
    v5.Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
    v6.Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
    v7.Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
    
    // 设置UV坐标和颜色
    DirectX::XMFLOAT4 color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    DirectX::XMFLOAT2 uv0 = DirectX::XMFLOAT2(0.0f, 0.0f);
    
    for (Vertex* v : {&v0, &v1, &v2, &v3, &v4, &v5, &v6, &v7})
    {
        v->Color = color;
        v->UV0 = uv0;
    }
    
    // 添加所有顶点到网格
    CubeMesh.Vertices = {v0, v1, v2, v3, v4, v5, v6, v7};
    
    // 添加三角形索引 (6个面，每个面2个三角形 = 12个三角形)
    // 底面 (-Y)
    CubeMesh.Indices.push_back(0); CubeMesh.Indices.push_back(1); CubeMesh.Indices.push_back(2);
    CubeMesh.Indices.push_back(0); CubeMesh.Indices.push_back(2); CubeMesh.Indices.push_back(3);
    
    // 顶面 (+Y)
    CubeMesh.Indices.push_back(4); CubeMesh.Indices.push_back(6); CubeMesh.Indices.push_back(5);
    CubeMesh.Indices.push_back(4); CubeMesh.Indices.push_back(7); CubeMesh.Indices.push_back(6);
    
    // 前面 (+Z)
    CubeMesh.Indices.push_back(3); CubeMesh.Indices.push_back(2); CubeMesh.Indices.push_back(6);
    CubeMesh.Indices.push_back(3); CubeMesh.Indices.push_back(6); CubeMesh.Indices.push_back(7);
    
    // 后面 (-Z)
    CubeMesh.Indices.push_back(0); CubeMesh.Indices.push_back(5); CubeMesh.Indices.push_back(1);
    CubeMesh.Indices.push_back(0); CubeMesh.Indices.push_back(4); CubeMesh.Indices.push_back(5);
    
    // 左面 (-X)
    CubeMesh.Indices.push_back(0); CubeMesh.Indices.push_back(3); CubeMesh.Indices.push_back(7);
    CubeMesh.Indices.push_back(0); CubeMesh.Indices.push_back(7); CubeMesh.Indices.push_back(4);
    
    // 右面 (+X)
    CubeMesh.Indices.push_back(1); CubeMesh.Indices.push_back(5); CubeMesh.Indices.push_back(6);
    CubeMesh.Indices.push_back(1); CubeMesh.Indices.push_back(6); CubeMesh.Indices.push_back(2);
    
    // 将网格添加到输出列表
    OutMeshes.push_back(CubeMesh);
    
    return ErrorCode::OK;
}

ErrorCode MeshLoader::GenerateMeshletData(const std::vector<Mesh>& Meshes, std::vector<MeshletDataForMeshOptimizer>& OutMeshletData) const
{
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        MeshletDataForMeshOptimizer MeshletDataInstance;
    
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

        OutMeshletData.push_back(MeshletDataInstance);
    }

    return ErrorCode::OK;
}