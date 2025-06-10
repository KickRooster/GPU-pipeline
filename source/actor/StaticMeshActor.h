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

    std::unique_ptr<MeshletData> MeshletDataInstance;
    std::unique_ptr<MeshletDataProxy> MeshletDataProxyInstance;

    std::unique_ptr<StaticMeshActorConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;

public:
    StaticMeshActor(
        std::unique_ptr<Mesh> InMeshInstance,
        std::unique_ptr<MeshProxy> InMeshProxyInstance,
        std::unique_ptr<MeshletData> InMeshletDataInstance,
        std::unique_ptr<MeshletDataProxy> InMeshletDataProxyInstance);
    void Update(float DeltaTime) override;
    Mesh* GetMeshInstance() const;
    MeshProxy* GetMeshProxyInstance() const;
    MeshletData* GetMeshletDataInstance() const;
    MeshletDataProxy* GetMeshletDataProxyInstance() const;
    StaticMeshActorConstantBuffer* GetConstantBuffer() const;
    ConstantBufferProxy* GetConstantBufferProxy() const;
    DirectX::XMMATRIX GetWorldMatrix() const;
    ~StaticMeshActor() override = default;
};