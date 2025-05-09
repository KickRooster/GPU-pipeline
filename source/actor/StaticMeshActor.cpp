#include "StaticMeshActor.h"
#include "../mesh/Mesh.h"
#include "../mesh/MeshLoader.h"

StaticMeshActor::StaticMeshActor(const string& Path)
{
    vector<Mesh> Meshes;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes);
    
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unique_ptr<Mesh> MeshInstance = make_unique<Mesh>();
        MeshInstance->Vertices = Meshes[I].Vertices;
        MeshInstance->Indices = Meshes[I].Indices;
        MeshInstances.push_back(std::move(MeshInstance));
        
        unique_ptr<MeshProxy> MeshProxyInstance = make_unique<MeshProxy>();
        MeshProxyInstances.push_back(std::move(MeshProxyInstance));
    }
}

void StaticMeshActor::Update(float DeltaTime)
{
    
}