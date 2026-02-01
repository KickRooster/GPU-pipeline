#include "CullingVisualCamera.h"

using namespace std;
using namespace DirectX;

CullingVisualCamera::CullingVisualCamera(
    const XMFLOAT4X4* Local2WorldMatrix,
    vector<Vertex>&& InVertices,
    unique_ptr<NaniteData> InNaniteDataInstance,
    unique_ptr<NaniteClusterProxy> InNaniteClusterProxyInstance)
:StaticMesh(
    Local2WorldMatrix,
    move(InVertices),
    move(InNaniteDataInstance),
    move(InNaniteClusterProxyInstance))
{
}

void CullingVisualCamera::Update(float DeltaTime, unsigned int FrameIndex)
{
    StaticMesh::Update(DeltaTime, FrameIndex);

    //  Cause CullingVisualCamera is a proxy for debugging, and are transformed manually by editor at 3rd player view,
    //  we update its' ViewMatrix using the inverse of TransformationMatrix.
    ViewMatrix = XMMatrixInverse(nullptr, TransformationMatrix);
    
    ProjectionMatrix = MathTool::GetInstance().XMMatrixPerspectiveFovLHReverseZ(FovY * XM_PI / 180.f, AspectRatio, NearPlane, FarPlane);
    ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
}

ConstantBufferProxy* CullingVisualCamera::GetConstantBufferProxy() const
{
    return nullptr; 
}