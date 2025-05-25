#pragma once
#include "../misc/Math.h"
#include <DirectXMath.h>

struct CameraConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProj = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4   Planes[6];
    DirectX::XMFLOAT3   ViewPosition;
    float               Padding;
};

struct StaticMeshActorConstantBuffer
{
    DirectX::XMFLOAT4X4 World = MathTool::GetInstance().Identity4x4();
};

struct MeshInfoConstantBuffer
{
    unsigned int MeshletCount = 0;
    unsigned int Padding[3] = {0, 0, 0}; // 填充使结构对齐到16字节
};

struct ConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProj = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4X4 World = MathTool::GetInstance().Identity4x4();
};