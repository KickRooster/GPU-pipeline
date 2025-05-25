#pragma once
#include <memory>
#include "Actor.h"
#include "../dx12/ConstantBuffer.h"
#include "../dx12/ConstantBufferProxy.h"
#include "CullingVisualCameraActor.h"

class CameraActor : public Actor
{
protected:
    DirectX::XMMATRIX ViewMatrix;
    DirectX::XMMATRIX ProjectionMatrix;
    std::unique_ptr<CameraConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;
    CullingVisualCameraActor* CullingCamera;

public:
    DirectX::XMFLOAT3 LookDirection;
    DirectX::XMFLOAT3 UpDirection;
    float FovY;
    float AspectRatio;
    float NearPlane;
    float FarPlane;
    

    CameraActor();
    void ResponseToUI(const UIState* State);
    void SetCullingCamera(CullingVisualCameraActor* CullingCamera);
    CullingVisualCameraActor* GetCullingCamera() const;
    void Update(float DeltaTime) override;
    CameraConstantBuffer* GetConstantBuffer() const;
    ConstantBufferProxy* GetConstantBufferProxy() const;
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;
    ~CameraActor() override = default;
};