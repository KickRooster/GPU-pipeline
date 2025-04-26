#pragma once
#include "Actor.h"
#include "../dx12/ConstantBuffer.h"
#include "../dx12/ConstantBufferProxy.h"

class CameraActor : public Actor
{
protected:
    XMMATRIX ProjectionMatrix;
    std::unique_ptr<ConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;

public:
    XMFLOAT3 LookDirection;
    XMFLOAT3 UpDirection;
    float FovY;
    float AspectRatio;
    float NearPlane;
    float FarPlane;

    CameraActor();
    void Update(float DeltaTime) override;
    ConstantBuffer* GetConstantBuffer() const;
    ConstantBufferProxy* GetConstantBufferProxy() const;
    ~CameraActor() override = default;
};
