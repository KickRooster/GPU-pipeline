#include "MeshLoader.h"

using namespace std;
using namespace DirectX;

void MeshLoader::ProcessNode(aiNode* Node, const aiScene* Scene, vector<Mesh>& OutMeshes, const XMMATRIX& ParentTransform)
{
    const XMMATRIX NodeTransform = MathTool::GetInstance().AssimpMatrixToXMMatrix(Node->mTransformation);
    const XMMATRIX WorldTransform = ParentTransform * NodeTransform;
    
    for (unsigned int I = 0; I < Node->mNumMeshes; I++)
    {
        aiMesh* AssimpMesh = Scene->mMeshes[Node->mMeshes[I]];
        ProcessMesh(AssimpMesh, Scene, OutMeshes, WorldTransform);
    }
    
    for (unsigned int I = 0; I < Node->mNumChildren; I++)
    {
        ProcessNode(Node->mChildren[I], Scene, OutMeshes, WorldTransform);
    }
}

void MeshLoader::ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, vector<Mesh>& OutMeshes, const XMMATRIX& NodeTransform)
{
    Mesh OutMesh;

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

    OutMesh.Name = AssimpMesh->mName.C_Str();

    OutMeshes.push_back(OutMesh);
}

ErrorCode MeshLoader::LoadMesh(const std::string& Path, vector<Mesh>& OutMeshes)
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

    ProcessNode(Scene->mRootNode, Scene, OutMeshes, XMMatrixIdentity());

    return ErrorCode::OK;
}

ErrorCode MeshLoader::GenerateMeshletData(const std::vector<Mesh>& Meshes, std::vector<MeshletDataForMeshOptimizer>& OutMeshletData) const
{
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        MeshletDataForMeshOptimizer MeshletDataInstance;
    
        constexpr size_t MaxVertexCountPerMeshlet = 64;
        constexpr size_t MaxTriangleCountPerMeshlet = 124;
        constexpr float ConeWeight = 0.25f;
        
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

        const meshopt_Meshlet& LastMeshlet = MeshletDataInstance.Meshlets[MeshletCount - 1];
        MeshletDataInstance.MeshletVertices.resize(LastMeshlet.vertex_offset + LastMeshlet.vertex_count);
        MeshletDataInstance.MeshletIndices.resize(LastMeshlet.triangle_offset + ((LastMeshlet.triangle_count * 3 + 3) & ~3));
        MeshletDataInstance.Meshlets.resize(MeshletCount);

        //  For optimal performance, it is recommended to further optimize each meshlet in isolation for better triangle and vertex locality.
        for (size_t J = 0; J < MeshletDataInstance.Meshlets.size(); ++J)
        {
            const meshopt_Meshlet& CurrentMeshlet = MeshletDataInstance.Meshlets[J];
            meshopt_optimizeMeshlet(
                &MeshletDataInstance.MeshletVertices[CurrentMeshlet.vertex_offset],
                &MeshletDataInstance.MeshletIndices[CurrentMeshlet.triangle_offset],
                CurrentMeshlet.triangle_count,
                CurrentMeshlet.vertex_count);
        }

        //  Generate extra data for each meshlet that can be saved and used at runtime to perform cluster culling.
        for (size_t J = 0; J < MeshletDataInstance.Meshlets.size(); ++J)
        {
            const meshopt_Meshlet& CurrentMeshlet = MeshletDataInstance.Meshlets[J];
            meshopt_Bounds Bounds = meshopt_computeMeshletBounds(
                    &MeshletDataInstance.MeshletVertices[CurrentMeshlet.vertex_offset],
                    &MeshletDataInstance.MeshletIndices[CurrentMeshlet.triangle_offset],
                    CurrentMeshlet.triangle_count,
                    &Meshes[I].Vertices[0].Position.x,
                    Meshes[I].Vertices.size(),
                    sizeof(Vertex));
            MeshletDataInstance.MeshletBounds.push_back(Bounds);
        }
        
        OutMeshletData.push_back(MeshletDataInstance);
    }

    return ErrorCode::OK;
}