#include "Camera.h"
#include "../asset/MeshLoader.h"

using namespace std;
using namespace DirectX;

Camera::Camera()
    :CullingCamera(nullptr)
{
    ConstantBufferInstance = make_unique<CameraConstantBuffer>();
    ConstantBufferInstance->LODErrorThreshold = 1.0f;
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
}

void Camera::ResponseToUI(const UIState& State, float DeltaTime)
{
    //  Translation
    if (State.RightButtonDown)
    {
        XMVECTOR Look = XMLoadFloat3(&LookDirection);
        XMVECTOR Up = XMLoadFloat3(&UpDirection);
    
        Look = XMVector3Normalize(Look);
        Up = XMVector3Normalize(Up);
    
        XMStoreFloat3(&LookDirection, Look);
        XMStoreFloat3(&UpDirection, Up);

        XMVECTOR Position = XMLoadFloat3(&Transform.Position);
        XMVECTOR Right = XMVector3Cross(Up, Look);

        if (State.WDown)
        {
            Position = XMVectorAdd(Position, XMVectorScale(Look, State.MoveSpeed * DeltaTime));
        }
        if (State.SDown)
        {
            Position = XMVectorSubtract(Position, XMVectorScale(Look, State.MoveSpeed * DeltaTime));
        }
    
        if (State.ADown)
        {
            Position = XMVectorSubtract(Position, XMVectorScale(Right, State.MoveSpeed * DeltaTime));
        }
        if (State.DDown)
        {
            Position = XMVectorAdd(Position, XMVectorScale(Right, State.MoveSpeed * DeltaTime));
        }
    
        if (State.QDown)
        {
            Position = XMVectorSubtract(Position, XMVectorScale(Up, State.MoveSpeed * DeltaTime));
        }
        if (State.EDown)
        {
            Position = XMVectorAdd(Position, XMVectorScale(Up, State.MoveSpeed * DeltaTime));
        }
    
        XMStoreFloat3(&Transform.Position, Position);
    }

    //  Rotation
    if (State.RightButtonDown)
    {
        if (State.DeltaX != 0.0f)
        {
            XMMATRIX RotY = XMMatrixRotationY(State.DeltaX * State.RotateSpeed * DeltaTime);
        
            XMVECTOR Look = XMLoadFloat3(&LookDirection);
            Look = XMVector3TransformNormal(Look, RotY);
            XMStoreFloat3(&LookDirection, Look);
        
            XMVECTOR Up = XMLoadFloat3(&UpDirection);
            Up = XMVector3TransformNormal(Up, RotY);
            XMStoreFloat3(&UpDirection, Up);
        }

        if (State.DeltaY != 0.0f)
        {
            XMVECTOR Look = XMLoadFloat3(&LookDirection);
            XMVECTOR Up = XMLoadFloat3(&UpDirection);
            XMVECTOR Right = XMVector3Cross(Up, Look);
        
            XMMATRIX RotX = XMMatrixRotationAxis(Right, State.DeltaY * State.RotateSpeed * DeltaTime);
        
            Look = XMVector3TransformNormal(Look, RotX);
            XMStoreFloat3(&LookDirection, Look);
        
            Up = XMVector3TransformNormal(Up, RotX);
            XMStoreFloat3(&UpDirection, Up);
        }
    }
}

void Camera::SetCullingCamera(CullingVisualCamera* CullingCamera)
{
    this->CullingCamera = CullingCamera;    
}

CullingVisualCamera* Camera::GetCullingCamera() const
{
    return CullingCamera;
}

void Camera::Update(float DeltaTime, unsigned int FrameIndex)
{
    ViewMatrix = XMMatrixLookToLH(
        XMLoadFloat3(&Transform.Position),
        XMLoadFloat3(&LookDirection),
        XMLoadFloat3(&UpDirection));

    ProjectionMatrix = MathTool::GetInstance().XMMatrixPerspectiveFovLHReverseZ(FovY * XM_PI / 180.f, AspectRatio, NearPlane, FarPlane);
    const XMMATRIX ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;

    XMStoreFloat4x4(&ConstantBufferInstance->ViewProj, XMMatrixTranspose(ViewProjectionMatrix));

    //  Use Culling Camera for culling if Culling Visual Camera Actor is set.
    //  if so, CullingCamera's update function must be calling before this function. 
    const XMMATRIX ViewProjectionTransposeMatrix = CullingCamera ?
        XMMatrixTranspose(CullingCamera->ViewProjectionMatrix) : XMMatrixTranspose(ViewProjectionMatrix);
    XMVECTOR Planes[6];
    
    // Left: row4 + row1
    Planes[0] = XMVectorAdd(ViewProjectionTransposeMatrix.r[3], ViewProjectionTransposeMatrix.r[0]);
    
    // Right: row4 - row1  
    Planes[1] = XMVectorSubtract(ViewProjectionTransposeMatrix.r[3], ViewProjectionTransposeMatrix.r[0]);
    
    // Top: row4 - row2
    Planes[2] = XMVectorSubtract(ViewProjectionTransposeMatrix.r[3], ViewProjectionTransposeMatrix.r[1]);
    
    // Bottom: row4 + row2
    Planes[3] = XMVectorAdd(ViewProjectionTransposeMatrix.r[3], ViewProjectionTransposeMatrix.r[1]);
    
    // Far: row4 - row3
    Planes[4] = XMVectorSubtract(ViewProjectionTransposeMatrix.r[3], ViewProjectionTransposeMatrix.r[2]);
    
    // Near: row4 + row3
    Planes[5] = XMVectorAdd(ViewProjectionTransposeMatrix.r[3], ViewProjectionTransposeMatrix.r[2]);
    
    // Normalize
    for (int I = 0; I < 6; ++I)
    {
        Planes[I] = XMPlaneNormalize(Planes[I]);
        XMStoreFloat4(&ConstantBufferInstance->Planes[I], Planes[I]);
    }

    //  And be same for ViewPosition.
    if (CullingCamera)
    {
        ConstantBufferInstance->ViewPosition = CullingCamera->Transform.Position;
    }
    else
    {
        ConstantBufferInstance->ViewPosition = Transform.Position;
    }
    
    ConstantBufferInstance->ScreenWidth = ViewportWidth;
    ConstantBufferInstance->ScreenHeight = ViewportHeight;
    ConstantBufferInstance->RecipTanHalfFovy = 1.0f / tanf((FovY * XM_PI / 180.f) * 0.5f);
    ConstantBufferInstance->NearPlane = NearPlane;
    
    if (ConstantBufferProxyInstance->MappedData[FrameIndex] != nullptr)
    {
        memcpy(
            ConstantBufferProxyInstance->MappedData[FrameIndex],
            ConstantBufferInstance.get(),
            MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(CameraConstantBuffer)));
    }
}

ConstantBufferProxy* Camera::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();   
}

XMMATRIX Camera::GetViewMatrix() const
{
    return ViewMatrix;
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    return ProjectionMatrix;
}