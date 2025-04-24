#include "CameraActor.h"

CameraActor::CameraActor()
{
    ConstantBufferInstance = make_unique<ConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
}

void CameraActor::Update(float DeltaTime)
{
    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 5.0f;

    XMFLOAT4X4 mWorld = MathTool::GetInstance().Identity4x4();
    XMFLOAT4X4 mView = MathTool::GetInstance().Identity4x4();
    XMFLOAT4X4 mProj = MathTool::GetInstance().Identity4x4();
    
    // Convert Spherical to Cartesian coordinates.
    float x = mRadius*sinf(mPhi)*cosf(mTheta);
    float z = mRadius*sinf(mPhi)*sinf(mTheta);
    float y = mRadius*cosf(mPhi);

    // Build the view matrix.
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*3.1415926535f, 1, 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX worldViewProj = world*view*proj;

    // Update the constant buffer with the latest worldViewProj matrix.
    XMStoreFloat4x4(&ConstantBufferInstance->WorldViewProj, XMMatrixTranspose(worldViewProj));
    memcpy(
        &ConstantBufferProxyInstance->MappedData[0 * ConstantBufferProxyInstance->ElementByteSize],
        &ConstantBufferInstance.get()->WorldViewProj,
        sizeof(ConstantBuffer));
    
    // TransformationMatrix = XMMatrixLookToLH(
    //     XMLoadFloat3(&Transform.Position),
    //     XMLoadFloat3(&LookDirection),
    //     XMLoadFloat3(&UpDirection));
    //
    // ProjectionMatrix = XMMatrixPerspectiveFovLH(FovY, AspectRatio, NearPlane, FarPlane);
    // ViewProjectionMatrix = ProjectionMatrix * TransformationMatrix;
    //
    // //  Update the constant buffer data.
    // XMStoreFloat4x4(&ConstantBufferInstance.get()->WorldViewProj, ViewProjectionMatrix);
    // memcpy(
    //     &ConstantBufferProxyInstance->MappedData[0 * ConstantBufferProxyInstance->ElementByteSize],
    //     &ConstantBufferInstance.get()->WorldViewProj,
    //     sizeof(ConstantBuffer));
}

XMMATRIX CameraActor::GetViewProjectionMatrix()
{
    return ViewProjectionMatrix;
}

ConstantBuffer* CameraActor::GetConstantBuffer() const
{
    return ConstantBufferInstance.get();    
}

ConstantBufferProxy* CameraActor::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();   
}