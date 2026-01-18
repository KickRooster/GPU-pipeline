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

    CullingVisualCamera(std::unique_ptr<Mesh> InMeshInstance,
        std::unique_ptr<NaniteData> InNaniteDataInstance,
        std::unique_ptr<NaniteClusterProxy> InNaniteClusterProxyInstance);

    void Update(float DeltaTime, unsigned int FrameIndex) override;
    ConstantBufferProxy* GetConstantBufferProxy() const override;
    virtual ~CullingVisualCamera() override = default;
};