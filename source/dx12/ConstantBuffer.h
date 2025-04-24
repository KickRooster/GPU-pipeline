#pragma once
#include "../base/Math.h"
#include <DirectXMath.h>

using namespace DirectX;

struct ConstantBuffer
{
    XMFLOAT4X4 WorldViewProj = MathTool::GetInstance().Identity4x4();
};