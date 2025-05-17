#include "CameraActor.h"

using namespace std;
using namespace DirectX;

CameraActor::CameraActor()
{
    ConstantBufferInstance = make_unique<CameraConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
}

void CameraActor::ResponseToUI(const UIState* State)
{
    {
        XMVECTOR Look = XMLoadFloat3(&LookDirection);
        XMVECTOR Up = XMLoadFloat3(&UpDirection);
    
        Look = XMVector3Normalize(Look);
        Up = XMVector3Normalize(Up);
    
        XMStoreFloat3(&LookDirection, Look);
        XMStoreFloat3(&UpDirection, Up);

        XMVECTOR Position = XMLoadFloat3(&Transform.Position);
        XMVECTOR Right = XMVector3Cross(Up, Look);

        if (State->WDown)
        {
            Position = XMVectorAdd(Position, XMVectorScale(Look, State->MoveSpeed * State->DeltaTime));
        }
        if (State->SDown)
        {
            Position = XMVectorSubtract(Position, XMVectorScale(Look, State->MoveSpeed * State->DeltaTime));
        }
    
        if (State->ADown)
        {
            Position = XMVectorSubtract(Position, XMVectorScale(Right, State->MoveSpeed * State->DeltaTime));
        }
        if (State->DDown)
        {
            Position = XMVectorAdd(Position, XMVectorScale(Right, State->MoveSpeed * State->DeltaTime));
        }
    
        if (State->QDown)
        {
            Position = XMVectorSubtract(Position, XMVectorScale(Up, State->MoveSpeed * State->DeltaTime));
        }
        if (State->EDown)
        {
            Position = XMVectorAdd(Position, XMVectorScale(Up, State->MoveSpeed * State->DeltaTime));
        }
    
        XMStoreFloat3(&Transform.Position, Position);
    }
    
    if (State->RightButtonDown)
    {
        if (State->DeltaX != 0.0f)
        {
            XMMATRIX RotY = XMMatrixRotationY(State->DeltaX * State->RotateSpeed * State->DeltaTime);
        
            XMVECTOR Look = XMLoadFloat3(&LookDirection);
            Look = XMVector3TransformNormal(Look, RotY);
            XMStoreFloat3(&LookDirection, Look);
        
            XMVECTOR Up = XMLoadFloat3(&UpDirection);
            Up = XMVector3TransformNormal(Up, RotY);
            XMStoreFloat3(&UpDirection, Up);
        }

        if (State->DeltaY != 0.0f)
        {
            XMVECTOR Look = XMLoadFloat3(&LookDirection);
            XMVECTOR Up = XMLoadFloat3(&UpDirection);
            XMVECTOR Right = XMVector3Cross(Up, Look);
        
            XMMATRIX RotX = XMMatrixRotationAxis(Right, State->DeltaY * State->RotateSpeed * State->DeltaTime);
        
            Look = XMVector3TransformNormal(Look, RotX);
            XMStoreFloat3(&LookDirection, Look);
        
            Up = XMVector3TransformNormal(Up, RotX);
            XMStoreFloat3(&UpDirection, Up);
        }
    }
}

void CameraActor::Update(float DeltaTime)
{
    ViewMatrix = XMMatrixLookToLH(
        XMLoadFloat3(&Transform.Position),
        XMLoadFloat3(&LookDirection),
        XMLoadFloat3(&UpDirection));

    ProjectionMatrix = XMMatrixPerspectiveFovLH(FovY * XM_PI / 180.f, AspectRatio, NearPlane, FarPlane);
    XMMATRIX ViewProj = ViewMatrix * ProjectionMatrix;

    XMStoreFloat4x4(&ConstantBufferInstance->ViewProj, XMMatrixTranspose(ViewProj));
    
    if (ConstantBufferProxyInstance->MappedData != nullptr)
    {
        memcpy(
            ConstantBufferProxyInstance->MappedData,
            ConstantBufferInstance.get(),
            sizeof(CameraConstantBuffer));
    }
}

CameraConstantBuffer* CameraActor::GetConstantBuffer() const
{
    return ConstantBufferInstance.get();    
}

ConstantBufferProxy* CameraActor::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();   
}