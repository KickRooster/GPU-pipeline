#pragma once

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
    //  xxx begin.
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

    float MoveSpeed;
    float RotateSpeed;
};