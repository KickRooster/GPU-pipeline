#pragma once
#include <memory>
#include "Actor.h"
#include "../dx12/ConstantBuffer.h"
#include "../dx12/ConstantBufferProxy.h"
#include "CullingVisualCamera.h"

class Camera : public Actor
{
protected:
    DirectX::XMMATRIX ViewMatrix;
    DirectX::XMMATRIX ProjectionMatrix;
    std::unique_ptr<CameraConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;
    CullingVisualCamera* CullingCamera;

public:
    DirectX::XMFLOAT3 LookDirection;
    DirectX::XMFLOAT3 UpDirection;
    float FovY;
    float ViewportWidth;
    float ViewportHeight;
    float AspectRatio;
    float NearPlane;
    float FarPlane;
    
    Camera();
    void ResponseToUI(const UIState& State, float DeltaTime);
    void SetCullingCamera(CullingVisualCamera* CullingCamera);
    CullingVisualCamera* GetCullingCamera() const;
    void Update(float DeltaTime) override;
    ConstantBufferProxy* GetConstantBufferProxy() const override;
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;
    ~Camera() override = default;
};