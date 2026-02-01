#pragma once
#include "../misc/Math.h"

struct CameraConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProj = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4   Planes[6];
    DirectX::XMFLOAT3   ViewPosition;
    float               ScreenWidth;
    float               ScreenHeight;
    float               RecipTanHalfFovy;
    float               LODErrorThreshold;
    float               NearPlane;
};

struct SkyLightConstantBuffer
{
    unsigned int IrradianceMapIndex;
    unsigned int PrefilteredMapIndex;
    unsigned int BRDFLUTIndex;
};