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
        std::unique_ptr<MeshletData> InMeshletDataInstance,
        std::unique_ptr<MeshletDataProxy> InMeshletDataProxyInstance);
    void Update(float DeltaTime) override;
    virtual ~CullingVisualCameraActor() override = default;
};