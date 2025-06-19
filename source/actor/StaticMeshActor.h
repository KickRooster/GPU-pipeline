#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Actor.h"
#include "../mesh/Mesh.h"
#include "../dx12/MeshProxy.h"
#include "../dx12/ConstantBuffer.h"
#include "../dx12/ConstantBufferProxy.h"

class StaticMeshActor : public Actor
{
protected:
    DirectX::XMMATRIX TransformationMatrix;    // SRT order: Scale * Rotation * Translation (UE style)
    
    std::unique_ptr<Mesh> MeshInstance;
    std::unique_ptr<MeshProxy> MeshProxyInstance;

    std::vector<std::unique_ptr<MeshletData>> MeshletDataInstances;
    std::vector<std::unique_ptr<MeshletDataProxy>> MeshletDataProxyInstances;
    
    std::unique_ptr<StaticMeshActorConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;

public:
    StaticMeshActor(
        std::unique_ptr<Mesh> InMeshInstance,
        std::unique_ptr<MeshProxy> InMeshProxyInstance,
        std::vector<std::unique_ptr<MeshletData>> InMeshletDataInstances,
        std::vector<std::unique_ptr<MeshletDataProxy>> InMeshletDataProxyInstances);
    
    void Update(float DeltaTime) override;
    Mesh* GetMeshInstance() const;
    MeshProxy* GetMeshProxyInstance() const;
    const std::vector<std::unique_ptr<MeshletData>>& GetMeshletDataInstances() const;
    const std::vector<std::unique_ptr<MeshletDataProxy>>& GetMeshletDataProxyInstances() const;
    
    StaticMeshActorConstantBuffer* GetConstantBuffer() const;
    ConstantBufferProxy* GetConstantBufferProxy() const;
    DirectX::XMMATRIX GetWorldMatrix() const;
    ~StaticMeshActor() override = default;
};