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

    CullingVisualCameraActor(const std::string& Path);
    void Update(float DeltaTime) override;
    virtual ~CullingVisualCameraActor() override = default;
};