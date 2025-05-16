#include "StaticMeshActor.h"
#include "../mesh/Mesh.h"
#include "../mesh/MeshLoader.h"
#include "../dx12/MeshProxy.h"

using namespace std;
using namespace DirectX;

StaticMeshActor::StaticMeshActor(const string& Path)
{
    ConstantBufferInstance = make_unique<StaticMeshActorConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
    
    vector<Mesh> Meshes;
    MeshLoader::GetInstance().LoadMesh(Path, Meshes);

    vector<MeshletData> MeshletDatas;
    MeshLoader::GetInstance().GenerateMeshletData(Meshes, MeshletDatas);
    
    for (unsigned int I = 0; I < Meshes.size(); ++I)
    {
        unique_ptr<Mesh> MeshInstance = make_unique<Mesh>();
        MeshInstance->Vertices = Meshes[I].Vertices;
        MeshInstance->Indices = Meshes[I].Indices;
        MeshInstances.push_back(std::move(MeshInstance));
        
        unique_ptr<MeshProxy> MeshProxyInstance = make_unique<MeshProxy>();
        MeshProxyInstances.push_back(std::move(MeshProxyInstance));
    }

    for (unsigned int I = 0; I < MeshletDatas.size(); ++I)
    {
        unique_ptr<MeshletData> MeshletDataInstance = make_unique<MeshletData>();
        MeshletDataInstance->Meshlets = MeshletDatas[I].Meshlets;
        MeshletDataInstance->MeshletVertices = MeshletDatas[I].MeshletVertices;
        MeshletDataInstance->MeshletIndices = MeshletDatas[I].MeshletIndices;
        MeshletDataInstances.push_back(std::move(MeshletDataInstance));
        
        unique_ptr<MeshletDataProxy> MeshletDataProxyInstance = make_unique<MeshletDataProxy>();
        MeshletDataProxyInstances.push_back(std::move(MeshletDataProxyInstance));
    }
}

void StaticMeshActor::Update(float DeltaTime)
{
    XMMATRIX Scale = XMMatrixScaling(Transform.Scale.x, Transform.Scale.y, Transform.Scale.z);
    
    XMMATRIX Rotation = XMMatrixRotationRollPitchYaw(
        Transform.Rotation.x * XM_PI / 180.0f,  // Pitch (X轴)
        Transform.Rotation.y * XM_PI / 180.0f,  // Yaw (Y轴)
        Transform.Rotation.z * XM_PI / 180.0f   // Roll (Z轴)
    );
    
    XMMATRIX Translation = XMMatrixTranslation(Transform.Position.x, Transform.Position.y, Transform.Position.z);
    
    TransformationMatrix = Translation * Rotation * Scale;
    
    if (ConstantBufferInstance)
    {
        XMStoreFloat4x4(&ConstantBufferInstance->World, XMMatrixTranspose(TransformationMatrix));
        
        if (ConstantBufferProxyInstance->MappedData != nullptr)
        {
            memcpy(
                ConstantBufferProxyInstance->MappedData,
                ConstantBufferInstance.get(),
                sizeof(StaticMeshActorConstantBuffer));
        }
    }
}

unsigned StaticMeshActor::GetSubMeshCount() const
{
    assert(MeshInstances.size() == MeshProxyInstances.size());
    
    return MeshInstances.size();    
}

Mesh* StaticMeshActor::GetMeshInstance(unsigned int Index) const
{
    if (MeshInstances.size() == 0)
    {
        return nullptr;
    }
    
    return MeshInstances[Index].get();    
}

MeshProxy* StaticMeshActor::GetMeshProxyInstance(unsigned int Index) const
{
    if (MeshProxyInstances.size() == 0)
    {
        return nullptr;
    }
    
    return MeshProxyInstances[Index].get();    
}

MeshletData* StaticMeshActor::GetMeshletDataInstance(unsigned int Index) const
{
    if (MeshletDataInstances.size() == 0)
    {
        return nullptr;
    }
    
    return MeshletDataInstances[Index].get();    
}

MeshletDataProxy* StaticMeshActor::GetMeshletDataProxyInstance(unsigned int Index) const
{
    if (MeshletDataProxyInstances.size() == 0)
    {
        return nullptr;
    }
    
    return MeshletDataProxyInstances[Index].get();    
}

StaticMeshActorConstantBuffer* StaticMeshActor::GetConstantBuffer() const
{
    return ConstantBufferInstance.get();
}

ConstantBufferProxy* StaticMeshActor::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();   
}