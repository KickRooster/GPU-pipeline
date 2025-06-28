#pragma once
#include "../misc/Math.h"
#include <DirectXMath.h>

struct CameraConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProj = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4   Planes[6];
    DirectX::XMFLOAT3   ViewPosition;
    float               RecipTanHalfFovy;  // 1.0f / tanf(fovy * 0.5f)
    unsigned int        LODCount;
};

struct StaticMeshActorConstantBuffer
{
    DirectX::XMFLOAT4X4 World = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4X4 WorldInvTranspose = MathTool::GetInstance().Identity4x4();
    DirectX::XMFLOAT4   BoundingSphere;
    
    // HLSL中每个数组元素占16字节，直接模拟布局
    struct {
        unsigned int Value;
        unsigned int Padding[3];
    } MeshletCounts[4];
    
    struct {
        unsigned int Value;
        unsigned int Padding[3];
    } PBRTextureIndices[4];
};