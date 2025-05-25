#include "CullingVisualCameraActor.h"

using namespace std;
using namespace DirectX;

CullingVisualCameraActor::CullingVisualCameraActor(const std::string& Path)
    :StaticMeshActor(Path)
{
}

void CullingVisualCameraActor::Update(float DeltaTime)
{
    StaticMeshActor::Update(DeltaTime);

    ViewMatrix = XMMatrixLookToLH(
        XMLoadFloat3(&Transform.Position),
        XMLoadFloat3(&LookDirection),
        XMLoadFloat3(&UpDirection));

    ProjectionMatrix = XMMatrixPerspectiveFovLH(FovY * XM_PI / 180.f, AspectRatio, NearPlane, FarPlane);
    ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
}