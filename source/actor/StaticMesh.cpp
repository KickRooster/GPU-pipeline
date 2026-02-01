#include "StaticMesh.h"
#include "../asset/Mesh.h"
#include "../asset/MeshLoader.h"
#include "../dx12/MeshProxy.h"

using namespace std;
using namespace DirectX;

StaticMesh::StaticMesh(
    const XMFLOAT4X4* Local2WorldMatrix,
    vector<Vertex>&& InVertices,
    unique_ptr<NaniteData> InNaniteDataInstance,
    unique_ptr<NaniteClusterProxy> InNaniteClusterProxyInstance)
    :
    Vertices(move(InVertices)),
    NaniteDataInstance(move(InNaniteDataInstance)),
    NaniteClusterProxyInstance(move(InNaniteClusterProxyInstance))
{
    const XMMATRIX MeshTransform = XMLoadFloat4x4(Local2WorldMatrix);
    
    XMVECTOR Scale, Rotation, Translation;
    if (XMMatrixDecompose(&Scale, &Rotation, &Translation, MeshTransform))
    {
        XMStoreFloat3(&Transform.Scale, Scale);
        XMStoreFloat4(&Transform.Rotation, Rotation);
        XMStoreFloat3(&Transform.Position, Translation);
    }
}

void StaticMesh::Update(float DeltaTime, unsigned int FrameIndex)
{
    const XMMATRIX Scale = XMMatrixScaling(Transform.Scale.x, Transform.Scale.y, Transform.Scale.z);
    const XMVECTOR Quaternion = XMLoadFloat4(&Transform.Rotation);
    const XMMATRIX Rotation = XMMatrixRotationQuaternion(Quaternion);
    
    const XMMATRIX Translation = XMMatrixTranslation(Transform.Position.x, Transform.Position.y, Transform.Position.z);
    TransformationMatrix = Scale * Rotation * Translation;

    // Update GPU Scene data
    XMStoreFloat4x4(&SceneData.LocalToWorld, XMMatrixTranspose(TransformationMatrix));

    const XMMATRIX WorldInverse = XMMatrixInverse(nullptr, TransformationMatrix);
    const XMMATRIX WorldInvTranspose = XMMatrixTranspose(WorldInverse);
    XMStoreFloat4x4(&SceneData.WorldInvTranspose, XMMatrixTranspose(WorldInvTranspose));
}

ConstantBufferProxy* StaticMesh::GetConstantBufferProxy() const
{
    return nullptr;
}

const std::vector<Vertex>& StaticMesh::GetVertices() const
{
    return Vertices;
}

NaniteData* StaticMesh::GetNaniteData() const
{
    return NaniteDataInstance.get();
}

NaniteClusterProxy* StaticMesh::GetNaniteClusterProxy() const
{
    return NaniteClusterProxyInstance.get();
}

void StaticMesh::SetMaterial(std::unique_ptr<Material> InMaterialInstance, std::unique_ptr<MaterialProxy> InMaterialProxyInstance)
{
    MaterialInstance = std::move(InMaterialInstance);
    MaterialProxyInstance = std::move(InMaterialProxyInstance);

    // Update GPU Scene data with PBR texture indices
    SceneData.PBRTextureIndices[0].Value = MaterialProxyInstance->AlbedoTextureIndex;
    SceneData.PBRTextureIndices[1].Value = MaterialProxyInstance->NormalTextureIndex;
    SceneData.PBRTextureIndices[2].Value = MaterialProxyInstance->MetallicTextureIndex;
    SceneData.PBRTextureIndices[3].Value = MaterialProxyInstance->RoughnessTextureIndex;
}

const Material* StaticMesh::GetMaterial() const
{
    return MaterialInstance.get();
}

FPrimitiveSceneData* StaticMesh::GetSceneData()
{
    return &SceneData;
}

void StaticMesh::ClearData()
{
    Vertices.clear();
}