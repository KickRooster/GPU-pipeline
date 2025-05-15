#pragma once
#include <DirectXMath.h>

enum class ErrorCode
{
    Illegal = -1,
    OK,
    //  DX12 error code begin.
    Failed,
    DebugInterfaceNotFound,
    SerializeVersionedRootSignatureFailed,
    RootSignatureCreationFailed,
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
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT4 Color;
    DirectX::XMFLOAT2 UV0;
};

struct Transform
{
    DirectX::XMFLOAT4 Rotation;
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Scale;
};