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
    std::vector<std::unique_ptr<Mesh>> MeshInstances;
    std::vector<std::unique_ptr<MeshProxy>> MeshProxyInstances;
    DirectX::XMMATRIX TransformationMatrix;
    
    std::unique_ptr<StaticMeshActorConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;
    
public:
    StaticMeshActor(const std::string& Path);
    void Update(float DeltaTime) override;
    unsigned int GetSubMeshCount() const;
    Mesh* GetMeshInstance(unsigned int Index) const;
    MeshProxy* GetMeshProxyInstance(unsigned int Index) const;
    StaticMeshActorConstantBuffer* GetConstantBuffer() const;
    ConstantBufferProxy* GetConstantBufferProxy() const;
    ~StaticMeshActor() override = default;
};