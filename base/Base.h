#pragma once

namespace dev
{
    enum ErrorCode
    {
        ErrorCode_Illegal = -1,
        ErrorCode_OK,
        //  DX12 error code begin.
        ErrorCode_Failed,
        ErrorCode_DebugInterfaceNotFound,
        ErrorCode_DeviceCreateFailed,
        ErrorCode_DescriptorHeapCreateFailed,
        ErrorCode_CommandQueueCreateFailed,
        ErrorCode_CommandAllocatorCreateFailed,
        ErrorCode_CommandListCreateFailed,
        ErrorCode_CommandListCloseFailed,
        ErrorCode_FenceCreateFailed,
        ErrorCode_FenceEventCreateFailed,
        ErrorCode_DXGIFactoryCreateFailed,
        ErrorCode_SwapChainForHwndCreateFailed,
        ErrorCode_QueryIDXGISwapChain3InterfaceFailed,
        //  xxx begin.
        ErrorCode_Num
    };
}