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

    vector<MeshletDataForMeshOptimizer> MeshletDatas;
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
        for (unsigned int J = 0; J < MeshletDatas[I].MeshletIndices.size(); ++J)
        {
            MeshletDataInstance->MeshletIndices.push_back(static_cast<unsigned int>(MeshletDatas[I].MeshletIndices[J]));
        }
        MeshletDataInstance->MeshletBounds = MeshletDatas[I].MeshletBounds;
        MeshletDataInstances.push_back(std::move(MeshletDataInstance));
        
        unique_ptr<MeshletDataProxy> MeshletDataProxyInstance = make_unique<MeshletDataProxy>();
        MeshletDataProxyInstances.push_back(std::move(MeshletDataProxyInstance));
    }
}

void StaticMeshActor::Update(float DeltaTime)
{
    const XMMATRIX Scale = XMMatrixScaling(Transform.Scale.x, Transform.Scale.y, Transform.Scale.z);
    
    const XMVECTOR Quaternion = XMLoadFloat4(&Transform.Rotation);
    const XMMATRIX Rotation = XMMatrixRotationQuaternion(Quaternion);
    
    const XMMATRIX Translation = XMMatrixTranslation(Transform.Position.x, Transform.Position.y, Transform.Position.z);
    
    TransformationMatrix = Scale * Rotation * Translation;
    
    if (ConstantBufferInstance)
    {
        XMStoreFloat4x4(&ConstantBufferInstance->World, XMMatrixTranspose(TransformationMatrix));
        
        XMMATRIX WorldInverse = XMMatrixInverse(nullptr, TransformationMatrix);
        XMMATRIX WorldInvTranspose = XMMatrixTranspose(WorldInverse);
        XMStoreFloat4x4(&ConstantBufferInstance->WorldInvTranspose, XMMatrixTranspose(WorldInvTranspose));
        
        if (ConstantBufferProxyInstance->MappedData != nullptr)
        {
            memcpy(
                ConstantBufferProxyInstance->MappedData,
                ConstantBufferInstance.get(),
                sizeof(StaticMeshActorConstantBuffer));
        }
    }
}

unsigned int StaticMeshActor::GetSubMeshCount() const
{
    assert(MeshInstances.size() == MeshProxyInstances.size());
    
    return static_cast<unsigned int>(MeshInstances.size());    
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

XMMATRIX StaticMeshActor::GetWorldMatrix() const
{
    return TransformationMatrix;
}