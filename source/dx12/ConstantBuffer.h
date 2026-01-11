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

struct StaticMeshConstantBuffer
{
    DirectX::XMFLOAT4X4 World = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4X4 WorldInvTranspose = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4   BoundingSphere;

    struct
    {
        unsigned int Value;
        unsigned int Padding[3];  // 16-byte alignment per element
    } PBRTextureIndices[4];

    unsigned int NaniteClusterCount = 0;
    unsigned int Padding[3] = {0, 0, 0};  // 16-byte alignment
};

struct SkyLightConstantBuffer
{
    unsigned int IrradianceMapIndex;
    unsigned int PrefilteredMapIndex;
    unsigned int BRDFLUTIndex;
};