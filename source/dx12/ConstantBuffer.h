#pragma once
#include "../misc/Math.h"
#include <DirectXMath.h>

struct CameraConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProj = MathTool::GetInstance().Identity4x4();
};

struct StaticMeshActorConstantBuffer
{
    DirectX::XMFLOAT4X4 World = MathTool::GetInstance().Identity4x4();
};

struct ConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProj = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4X4 World = MathTool::GetInstance().Identity4x4();
};