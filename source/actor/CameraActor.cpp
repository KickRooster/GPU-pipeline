#include "CameraActor.h"

CameraActor::CameraActor()
{
    ConstantBufferInstance = make_unique<ConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
}

void CameraActor::Update(float DeltaTime)
{
    TransformationMatrix = XMMatrixLookToLH(
        XMLoadFloat3(&Transform.Position),
        XMLoadFloat3(&LookDirection),
            XMLoadFloat3(&UpDirection));

    // view = XMMatrixLookToLH(
    //     XMVectorSet(0, 0, -4.f, 1),
    //     XMVectorSet(0, 0, 1, 0),
    //     XMLoadFloat3(&UpDirection));

    ProjectionMatrix = XMMatrixPerspectiveFovLH(FovY * XM_PI / 180.f, AspectRatio, NearPlane, FarPlane);
    XMMATRIX worldViewProj = TransformationMatrix * ProjectionMatrix;

    XMStoreFloat4x4(&ConstantBufferInstance->WorldViewProj, XMMatrixTranspose(worldViewProj));
    memcpy(
        &ConstantBufferProxyInstance->MappedData[0 * ConstantBufferProxyInstance->ElementByteSize],
        &ConstantBufferInstance.get()->WorldViewProj,
        sizeof(ConstantBuffer));
}

ConstantBuffer* CameraActor::GetConstantBuffer() const
{
    return ConstantBufferInstance.get();    
}

ConstantBufferProxy* CameraActor::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();   
}