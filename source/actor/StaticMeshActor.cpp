#include "StaticMeshActor.h"
#include "../mesh/Mesh.h"
#include "../mesh/MeshLoader.h"
#include "../dx12/MeshProxy.h"

using namespace std;
using namespace DirectX;

StaticMeshActor::StaticMeshActor(
    unique_ptr<Mesh> InMeshInstance,
    vector<unique_ptr<MeshletData>> InMeshletDataInstances,
    vector<unique_ptr<MeshletDataProxy>> InMeshletDataProxyInstances)
    :
    MeshInstance(move(InMeshInstance)),
    MeshletDataInstances(move(InMeshletDataInstances)),
    MeshletDataProxyInstances(move(InMeshletDataProxyInstances))
{
    ConstantBufferInstance = make_unique<StaticMeshActorConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
    
    for (size_t I = 0; I < MeshletDataInstances.size(); ++I)
    {
        ConstantBufferInstance->MeshletCounts[I] = static_cast<unsigned int>(MeshletDataInstances[I]->Meshlets.size());
    }
    
    if (MeshInstance)
    {
        const XMMATRIX MeshTransform = XMLoadFloat4x4(&MeshInstance->Local2WorldMatrix);
        
        XMVECTOR Scale, Rotation, Translation;
        if (XMMatrixDecompose(&Scale, &Rotation, &Translation, MeshTransform))
        {
            XMStoreFloat3(&Transform.Scale, Scale);
            XMStoreFloat4(&Transform.Rotation, Rotation);
            XMStoreFloat3(&Transform.Position, Translation);
        }
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
        
        const XMMATRIX WorldInverse = XMMatrixInverse(nullptr, TransformationMatrix);
        const XMMATRIX WorldInvTranspose = XMMatrixTranspose(WorldInverse);
        XMStoreFloat4x4(&ConstantBufferInstance->WorldInvTranspose, XMMatrixTranspose(WorldInvTranspose));
        
        if (MeshInstance)
        {
            ConstantBufferInstance->BoundingSphere = MeshInstance->BoundingSphere;
        }
        
        if (ConstantBufferProxyInstance->MappedData != nullptr)
        {
            memcpy(
                ConstantBufferProxyInstance->MappedData,
                ConstantBufferInstance.get(),
                sizeof(StaticMeshActorConstantBuffer));
        }
    }
}

const vector<unique_ptr<MeshletData>>& StaticMeshActor::GetMeshletDataInstances() const
{
    return MeshletDataInstances;
}

const vector<unique_ptr<MeshletDataProxy>>& StaticMeshActor::GetMeshletDataProxyInstances() const
{
    return MeshletDataProxyInstances;
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