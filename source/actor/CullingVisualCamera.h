#pragma once
#include "StaticMesh.h"

class CullingVisualCamera : public StaticMesh
{
public:
    DirectX::XMMATRIX ViewMatrix;
    DirectX::XMMATRIX ProjectionMatrix;
    DirectX::XMMATRIX ViewProjectionMatrix;
    float FovY;
    float AspectRatio;
    float NearPlane;
    float FarPlane;

    CullingVisualCamera(
        std::unique_ptr<Mesh> InMeshInstance,
        std::vector<std::unique_ptr<MeshletData>> InMeshletDataInstances,
        std::vector<std::unique_ptr<MeshletDataProxy>> InMeshletDataProxyInstances);
    
    void Update(float DeltaTime) override;
    ConstantBufferProxy* GetConstantBufferProxy() const override;
    virtual ~CullingVisualCamera() override = default;
};