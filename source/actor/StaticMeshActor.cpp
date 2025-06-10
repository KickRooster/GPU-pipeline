#include "StaticMeshActor.h"
#include "../mesh/Mesh.h"
#include "../mesh/MeshLoader.h"
#include "../dx12/MeshProxy.h"

using namespace std;
using namespace DirectX;

StaticMeshActor::StaticMeshActor(std::unique_ptr<Mesh> InMeshInstance, std::unique_ptr<MeshProxy> InMeshProxyInstance, std::unique_ptr<MeshletData> InMeshletDataInstance, std::unique_ptr<MeshletDataProxy> InMeshletDataProxyInstance)
    :
    MeshInstance(move(InMeshInstance)),
    MeshProxyInstance(move(InMeshProxyInstance)),
    MeshletDataInstance(move(InMeshletDataInstance)),
    MeshletDataProxyInstance(move(InMeshletDataProxyInstance))
{
    ConstantBufferInstance = make_unique<StaticMeshActorConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
    
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

Mesh* StaticMeshActor::GetMeshInstance() const
{
    return MeshInstance.get();    
}

MeshProxy* StaticMeshActor::GetMeshProxyInstance() const
{
    return MeshProxyInstance.get();    
}

MeshletData* StaticMeshActor::GetMeshletDataInstance() const
{
    return MeshletDataInstance.get();    
}

MeshletDataProxy* StaticMeshActor::GetMeshletDataProxyInstance() const
{
    return MeshletDataProxyInstance.get();    
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