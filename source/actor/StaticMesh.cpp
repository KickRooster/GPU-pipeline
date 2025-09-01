#include "StaticMesh.h"
#include "../asset/Mesh.h"
#include "../asset/MeshLoader.h"
#include "../dx12/MeshProxy.h"

using namespace std;
using namespace DirectX;

StaticMesh::StaticMesh(
    const XMFLOAT4X4* Local2WorldMatrix,
    const XMFLOAT4 InBoundingSphere,
    vector<unique_ptr<MeshletData>> InMeshletDataInstances,
    vector<unique_ptr<MeshletDataProxy>> InMeshletDataProxyInstances)
    :
    BoundingSphere(InBoundingSphere),
    MeshletDataInstances(move(InMeshletDataInstances)),
    MeshletDataProxyInstances(move(InMeshletDataProxyInstances))
{
    ConstantBufferInstance = make_unique<StaticMeshConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
    
    for (size_t I = 0; I < MeshletDataInstances.size(); ++I)
    {
        ConstantBufferInstance->MeshletCounts[I].Value = static_cast<unsigned int>(MeshletDataInstances[I]->Meshlets.size());
    }
    
    const XMMATRIX MeshTransform = XMLoadFloat4x4(Local2WorldMatrix);
    
    XMVECTOR Scale, Rotation, Translation;
    if (XMMatrixDecompose(&Scale, &Rotation, &Translation, MeshTransform))
    {
        XMStoreFloat3(&Transform.Scale, Scale);
        XMStoreFloat4(&Transform.Rotation, Rotation);
        XMStoreFloat3(&Transform.Position, Translation);
    }
}

void StaticMesh::Update(float DeltaTime)
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

        ConstantBufferInstance->BoundingSphere = BoundingSphere;
        
        if (ConstantBufferProxyInstance->MappedData != nullptr)
        {
            memcpy(
                ConstantBufferProxyInstance->MappedData,
                ConstantBufferInstance.get(),
                MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(StaticMeshConstantBuffer)));
        }
    }
}

ConstantBufferProxy* StaticMesh::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();   
}

const vector<unique_ptr<MeshletData>>& StaticMesh::GetMeshletDataInstances() const
{
    return MeshletDataInstances;
}

const vector<unique_ptr<MeshletDataProxy>>& StaticMesh::GetMeshletDataProxyInstances() const
{
    return MeshletDataProxyInstances;
}

void StaticMesh::SetMaterial(std::unique_ptr<Material> InMaterialInstance, std::unique_ptr<MaterialProxy> InMaterialProxyInstance)
{
    MaterialInstance = std::move(InMaterialInstance);
    MaterialProxyInstance = std::move(InMaterialProxyInstance);
    
    ConstantBufferInstance->PBRTextureIndices[0].Value = MaterialProxyInstance->AlbedoTextureIndex;
    ConstantBufferInstance->PBRTextureIndices[1].Value = MaterialProxyInstance->NormalTextureIndex;
    ConstantBufferInstance->PBRTextureIndices[2].Value = MaterialProxyInstance->MetallicTextureIndex;
    ConstantBufferInstance->PBRTextureIndices[3].Value = MaterialProxyInstance->RoughnessTextureIndex;
}

const Material* StaticMesh::GetMaterial() const
{
    return MaterialInstance.get();
}

XMMATRIX StaticMesh::GetWorldMatrix() const
{
    return TransformationMatrix;
}