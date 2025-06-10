#include "CullingVisualCameraActor.h"

using namespace std;
using namespace DirectX;

CullingVisualCameraActor::CullingVisualCameraActor(std::unique_ptr<Mesh> InMeshInstance, std::unique_ptr<MeshProxy> InMeshProxyInstance, std::unique_ptr<MeshletData> InMeshletDataInstance, std::unique_ptr<MeshletDataProxy> InMeshletDataProxyInstance)
:StaticMeshActor(
    move(InMeshInstance),
    move(InMeshProxyInstance),
    move(InMeshletDataInstance),
    move(InMeshletDataProxyInstance))
{
}

void CullingVisualCameraActor::Update(float DeltaTime)
{
    StaticMeshActor::Update(DeltaTime);

    //  Cause CullingVisualCameraActor is a proxy for debugging, and are transformed manually by editor at 3rd player view,
    //  we update its' ViewMatrix using the inverse of TransformationMatrix.
    ViewMatrix = XMMatrixInverse(nullptr, TransformationMatrix);
    
    ProjectionMatrix = XMMatrixPerspectiveFovLH(FovY * XM_PI / 180.f, AspectRatio, NearPlane, FarPlane);
    ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
}