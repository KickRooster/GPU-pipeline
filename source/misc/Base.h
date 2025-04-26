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