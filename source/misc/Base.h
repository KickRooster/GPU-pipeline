#pragma once
#include <DirectXMath.h>

using namespace DirectX;

enum class ErrorCode
{
    Illegal = -1,
    OK,
    //  DX12 error code begin.
    Failed,
    DebugInterfaceNotFound,
    DeviceCreateFailed,
    DescriptorHeapCreateFailed,
    CommandQueueCreateFailed,
    CommandAllocatorCreateFailed,
    CommandListCreateFailed,
    CommandListCloseFailed,
    FenceCreateFailed,
    FenceEventCreateFailed,
    DXGIFactoryCreateFailed,
    SwapChainForHwndCreateFailed,
    QueryIDXGISwapChain3InterfaceFailed,
    //  Assimp begin.
    MeshDataIncomplete,
    ErrorCode_Num
};

struct UIState
{
    bool WPressed;
    bool SPressed;
    bool APressed;
    bool DPressed;
    bool QPressed;
    bool EPressed;

    bool LeftButtonDown;
    bool RightButtonDown;

    float DeltaX;
    float DeltaY;
    float MouseWheel;

    float MoveSpeed;
    float RotateSpeed;
    float DeltaTime;
};

struct Vertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT4 Color;
    XMFLOAT2 UV0;
};

struct Transform
{
    XMFLOAT4 Rotation;
    XMFLOAT3 Position;
    XMFLOAT3 Scale;
};