#include "CullingVisualCameraActor.h"

using namespace std;
using namespace DirectX;

CullingVisualCameraActor::CullingVisualCameraActor(
    unique_ptr<Mesh> InMeshInstance,
    vector<unique_ptr<MeshletData>> InMeshletDataInstances,
    vector<unique_ptr<MeshletDataProxy>> InMeshletDataProxyInstances)
:StaticMeshActor(
    &InMeshInstance->Local2WorldMatrix,
    InMeshInstance->BoundingSphere,
    move(InMeshletDataInstances),
    move(InMeshletDataProxyInstances))
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