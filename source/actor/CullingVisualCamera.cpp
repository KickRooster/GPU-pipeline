#include "CullingVisualCamera.h"

using namespace std;
using namespace DirectX;

CullingVisualCamera::CullingVisualCamera(
    unique_ptr<Mesh> InMeshInstance,
    std::unique_ptr<NaniteData> InNaniteDataInstance,
    std::unique_ptr<NaniteClusterProxy> InNaniteClusterProxyInstance)
:StaticMesh(
    &InMeshInstance->Local2WorldMatrix,
    InMeshInstance->BoundingSphere,
    move(InNaniteDataInstance),
    move(InNaniteClusterProxyInstance))
{
}

void CullingVisualCamera::Update(float DeltaTime)
{
    StaticMesh::Update(DeltaTime);

    //  Cause CullingVisualCamera is a proxy for debugging, and are transformed manually by editor at 3rd player view,
    //  we update its' ViewMatrix using the inverse of TransformationMatrix.
    ViewMatrix = XMMatrixInverse(nullptr, TransformationMatrix);
    
    ProjectionMatrix = MathTool::GetInstance().XMMatrixPerspectiveFovLHReverseZ(FovY * XM_PI / 180.f, AspectRatio, NearPlane, FarPlane);
    ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
}

ConstantBufferProxy* CullingVisualCamera::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();   
}