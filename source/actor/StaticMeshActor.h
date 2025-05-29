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
    DirectX::XMMATRIX TransformationMatrixTRS;
    
    std::vector<std::unique_ptr<Mesh>> MeshInstances;
    std::vector<std::unique_ptr<MeshProxy>> MeshProxyInstances;

    std::vector<std::unique_ptr<MeshletData>> MeshletDataInstances;
    std::vector<std::unique_ptr<MeshletDataProxy>> MeshletDataProxyInstances;

    std::unique_ptr<StaticMeshActorConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;

public:
    StaticMeshActor(const std::string& Path);
    void Update(float DeltaTime) override;
    unsigned int GetSubMeshCount() const;
    Mesh* GetMeshInstance(unsigned int Index) const;
    MeshProxy* GetMeshProxyInstance(unsigned int Index) const;
    MeshletData* GetMeshletDataInstance(unsigned int Index) const;
    MeshletDataProxy* GetMeshletDataProxyInstance(unsigned int Index) const;
    StaticMeshActorConstantBuffer* GetConstantBuffer() const;
    ConstantBufferProxy* GetConstantBufferProxy() const;
    DirectX::XMMATRIX GetWorldMatrix() const;
    ~StaticMeshActor() override = default;
};