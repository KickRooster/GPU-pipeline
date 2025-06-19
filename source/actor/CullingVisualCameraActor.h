#pragma once
#include "StaticMeshActor.h"

class CullingVisualCameraActor : public StaticMeshActor
{
public:
    DirectX::XMMATRIX ViewMatrix;
    DirectX::XMMATRIX ProjectionMatrix;
    DirectX::XMMATRIX ViewProjectionMatrix;
    float FovY;
    float AspectRatio;
    float NearPlane;
    float FarPlane;

    CullingVisualCameraActor(
        std::unique_ptr<Mesh> InMeshInstance,
        std::unique_ptr<MeshProxy> InMeshProxyInstance,
        std::vector<std::unique_ptr<MeshletData>> InMeshletDataInstances,
        std::vector<std::unique_ptr<MeshletDataProxy>> InMeshletDataProxyInstances);
    
    void Update(float DeltaTime) override;
    virtual ~CullingVisualCameraActor() override = default;
};