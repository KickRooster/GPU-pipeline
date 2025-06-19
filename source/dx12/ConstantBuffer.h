#pragma once
#include "../misc/Math.h"
#include <DirectXMath.h>

struct CameraConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProj = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4   Planes[6];
    DirectX::XMFLOAT3   ViewPosition;
    float               RecipTanHalfFovy;  // 1.0f / tanf(fovy * 0.5f)
    uint32_t            LODCount;
};

struct StaticMeshActorConstantBuffer
{
    DirectX::XMFLOAT4X4 World = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4X4 WorldInvTranspose = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4   BoundingSphere;
    unsigned int        MeshletCounts[4] = {0, 0, 0, 0};
};