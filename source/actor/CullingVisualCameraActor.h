#pragma once
#include "StaticMeshActor.h"

class CullingVisualCameraActor : public StaticMeshActor
{
public:
    DirectX::XMMATRIX ViewMatrix;
    DirectX::XMMATRIX ProjectionMatrix;
    DirectX::XMMATRIX ViewProjectionMatrix;
    DirectX::XMFLOAT3 LookDirection;
    DirectX::XMFLOAT3 UpDirection;
    float FovY;
    float AspectRatio;
    float NearPlane;
    float FarPlane;

    CullingVisualCameraActor(const std::string& Path);
    void Update(float DeltaTime) override;
    virtual ~CullingVisualCameraActor() override = default;
};