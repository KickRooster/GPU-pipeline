#pragma once
#include <memory>
#include <vector>
#include "Actor.h"
#include "../asset/Mesh.h"
#include "../asset/Material.h"
#include "../dx12/MeshProxy.h"
#include "../dx12/ConstantBuffer.h"
#include "../dx12/ConstantBufferProxy.h"
#include "../dx12/MaterialProxy.h"

class StaticMesh : public Actor
{
protected:
    DirectX::XMMATRIX TransformationMatrix;    // SRT order: Scale * Rotation * Translation (UE style)
    DirectX::XMFLOAT4 BoundingSphere;
    
    std::unique_ptr<Material> MaterialInstance;
    std::unique_ptr<MaterialProxy> MaterialProxyInstance;
    
    std::vector<std::unique_ptr<MeshletData>> MeshletDataInstances;
    std::vector<std::unique_ptr<MeshletDataProxy>> MeshletDataProxyInstances;
    
    std::unique_ptr<StaticMeshConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;

public:
    StaticMesh(
        const DirectX::XMFLOAT4X4* Local2WorldMatrix,
        const DirectX::XMFLOAT4 BoundingSphere,
        std::vector<std::unique_ptr<MeshletData>> InMeshletDataInstances,
        std::vector<std::unique_ptr<MeshletDataProxy>> InMeshletDataProxyInstances);

    void Update(float DeltaTime) override;
    ConstantBufferProxy* GetConstantBufferProxy() const override;
    const std::vector<std::unique_ptr<MeshletData>>& GetMeshletDataInstances() const;
    const std::vector<std::unique_ptr<MeshletDataProxy>>& GetMeshletDataProxyInstances() const;
    void SetMaterial(std::unique_ptr<Material> InMaterialInstance, std::unique_ptr<MaterialProxy> InMaterialProxyInstance);
    const Material* GetMaterial() const;
    DirectX::XMMATRIX GetWorldMatrix() const;
    ~StaticMesh() override = default;
};