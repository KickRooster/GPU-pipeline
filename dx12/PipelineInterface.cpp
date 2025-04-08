#include "PipelineInterface.h"

namespace dev
{
#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

    ErrorCode PipelineInterface::Initialize(HWND hWnd)
    {
#ifdef DX12_ENABLE_DEBUG_LAYER
        ID3D12Debug* pDX12Debug = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDX12Debug))))
        {
            pDX12Debug->EnableDebugLayer();
        }
        else
        {
            return ErrorCode::DebugInterfaceNotFound;
        }
#endif

        // Create device
        if (D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&D3DDevice)) != S_OK)
        {
            return ErrorCode::DeviceCreateFailed;
        }

#ifdef DX12_ENABLE_DEBUG_LAYER
        if (pDX12Debug != nullptr)
        {
            ID3D12InfoQueue* pInfoQueue = nullptr;
            D3DDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
            pInfoQueue->Release();
            pDX12Debug->Release();
        }
#endif

        {
            D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
            DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            DescriptorHeapDesc.NumDescriptors = BackBufferCount;
            DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            DescriptorHeapDesc.NodeMask = 1;
            if (D3DDevice->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&D3DRtvDescHeap)) != S_OK)
            {
                return ErrorCode::DescriptorHeapCreateFailed;
            }
        }

        {   
            D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
            DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            DescriptorHeapDesc.NumDescriptors = SrvHeapSize;
            DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (D3DDevice->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&D3DSrvDescHeap)) != S_OK)
            {
                return ErrorCode::DescriptorHeapCreateFailed;
            }

            D3DSrvDescHeapAlloc.Create(D3DDevice, D3DSrvDescHeap);
        }
            
        SIZE_T rtvDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = D3DRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < BackBufferCount; i++)
        {
            MainRenderTargetDescriptors[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
        
        D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
        CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        CommandQueueDesc.NodeMask = 1;
        if (D3DDevice->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&D3DCommandQueue)) != S_OK)
        {
            return ErrorCode::Failed;
        }

        for (UINT i = 0; i < FrameNumInFlight; i++)
        {
            if (D3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&FrameContexts[i].CommandAllocator)) != S_OK)
            {
                return ErrorCode::CommandAllocatorCreateFailed;
            }
        }
        
        if (D3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameContexts[0].CommandAllocator, nullptr, IID_PPV_ARGS(&D3DCommandList)) != S_OK)
        {
            return ErrorCode::CommandListCreateFailed;
        }

        if (D3DCommandList->Close() != S_OK)
        {
            return ErrorCode::CommandListCloseFailed;
        }
        
        if (D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)) != S_OK)
        {
            return ErrorCode::FenceCreateFailed;
        }
        
        FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (FenceEvent == nullptr)
        {
            return ErrorCode::FenceEventCreateFailed;
        }
        
        // Setup swap chain
        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc;
        {
            ZeroMemory(&SwapChainDesc, sizeof(SwapChainDesc));
            SwapChainDesc.BufferCount = BackBufferCount;
            SwapChainDesc.Width = 0;
            SwapChainDesc.Height = 0;
            SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            SwapChainDesc.SampleDesc.Count = 1;
            SwapChainDesc.SampleDesc.Quality = 0;
            SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
            SwapChainDesc.Stereo = FALSE;
        }
            
        IDXGIFactory4* dxgiFactory = nullptr;
        IDXGISwapChain1* swapChain1 = nullptr;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
        {
            return ErrorCode::DXGIFactoryCreateFailed;
        }
            
        if (dxgiFactory->CreateSwapChainForHwnd(D3DCommandQueue, hWnd, &SwapChainDesc, nullptr, nullptr, &swapChain1) != S_OK)
        {
            return ErrorCode::SwapChainForHwndCreateFailed;
        }
            
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&SwapChain)) != S_OK)
        {
            return ErrorCode::QueryIDXGISwapChain3InterfaceFailed;
        }
            
        swapChain1->Release();
        dxgiFactory->Release();
        SwapChain->SetMaximumFrameLatency(BackBufferCount);
        SwapChainWaitableObject = SwapChain->GetFrameLatencyWaitableObject();

        for (UINT i = 0; i < BackBufferCount; i++)
        {
            ID3D12Resource* pBackBuffer = nullptr;
            SwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            D3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, MainRenderTargetDescriptors[i]);
            MainRenderTargetResources[i] = pBackBuffer;
        }
        
        return ErrorCode::OK;
    }

    void PipelineInterface::CleanUp()
    {
        //  WaitForLastSubmittedFrame
        FrameContext* frameCtx = &FrameContexts[FrameIndex % FrameNumInFlight];

        UINT64 fenceValue = frameCtx->FenceValue;
        if (fenceValue == 0)
        {
            return; // No fence was signaled
        }
        
        frameCtx->FenceValue = 0;
        if (Fence->GetCompletedValue() >= fenceValue)
        {
            return;
        }
        
        Fence->SetEventOnCompletion(fenceValue, FenceEvent);
        WaitForSingleObject(FenceEvent, INFINITE);

        //  CleanupRenderTarget
        for (UINT i = 0; i < BackBufferCount; i++)
        {
            if (MainRenderTargetResources[i])
            {
                MainRenderTargetResources[i]->Release();
                MainRenderTargetResources[i] = nullptr;
            }
        }
        
        //  Do clean
        if (SwapChain)
        {
            SwapChain->SetFullscreenState(false, nullptr);
            SwapChain->Release();
            SwapChain = nullptr;
        }
        
        if (SwapChainWaitableObject != nullptr)
        {
            CloseHandle(SwapChainWaitableObject);
        }
        
        for (UINT i = 0; i < FrameNumInFlight; i++)
        {
            if (FrameContexts[i].CommandAllocator)
            {
                FrameContexts[i].CommandAllocator->Release();
                FrameContexts[i].CommandAllocator = nullptr;
            }
        }
        
        if (D3DCommandQueue)
        {
            D3DCommandQueue->Release();
            D3DCommandQueue = nullptr;
        }
        
        if (D3DCommandList)
        {
            D3DCommandList->Release();
            D3DCommandList = nullptr;
        }
        
        if (D3DRtvDescHeap)
        {
            D3DRtvDescHeap->Release();
            D3DRtvDescHeap = nullptr;
        }
        
        if (D3DSrvDescHeap)
        {
            D3DSrvDescHeap->Release();
            D3DSrvDescHeap = nullptr;
        }
        
        if (Fence)
        {
            Fence->Release();
            Fence = nullptr;
        }
        
        if (FenceEvent)
        {
            CloseHandle(FenceEvent);
            FenceEvent = nullptr;
        }
        
        if (D3DDevice)
        {
            D3DDevice->Release();
            D3DDevice = nullptr;
        }

#ifdef DX12_ENABLE_DEBUG_LAYER
        IDXGIDebug1* pDebug = nullptr;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
        {
            pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
            pDebug->Release();
        }
#endif
    }

    void PipelineInterface::PackImGuiInitInfo(ImGui_ImplDX12_InitInfo& OutInitInfo)
    {
        OutInitInfo.Device = D3DDevice;
        OutInitInfo.CommandQueue = D3DCommandQueue;
        OutInitInfo.NumFramesInFlight = FrameNumInFlight;
        OutInitInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        OutInitInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
        // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
        // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
        OutInitInfo.SrvDescriptorHeap = D3DSrvDescHeap;
        
        // 使用UserData字段传递this指针
        OutInitInfo.UserData = this;
        
        // 使用没有捕获的lambda，通过UserData访问实例
        OutInitInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { 
            PipelineInterface* Instance = static_cast<PipelineInterface*>(info->UserData);
            return Instance->D3DSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); 
        };
        
        OutInitInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { 
            PipelineInterface* Instance = static_cast<PipelineInterface*>(info->UserData);
            return Instance->D3DSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); 
        };
    }
    
    FrameContext* PipelineInterface::WaitForNextFrameResources()
    {
        UINT nextFrameIndex = FrameIndex + 1;
        FrameIndex = nextFrameIndex;

        HANDLE waitableObjects[] = { SwapChainWaitableObject, nullptr };
        DWORD numWaitableObjects = 1;

        FrameContext* frameCtx = &FrameContexts[nextFrameIndex % FrameNumInFlight];
        UINT64 fenceValue = frameCtx->FenceValue;
        if (fenceValue != 0) // means no fence was signaled
        {
            frameCtx->FenceValue = 0;
            Fence->SetEventOnCompletion(fenceValue, FenceEvent);
            waitableObjects[1] = FenceEvent;
            numWaitableObjects = 2;
         }

        WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

        return frameCtx;
    }

    void PipelineInterface::WaitForLastSubmittedFrame()
    {
        FrameContext* frameCtx = &FrameContexts[FrameIndex % FrameNumInFlight];

        UINT64 fenceValue = frameCtx->FenceValue;
        if (fenceValue == 0)
            return; // No fence was signaled

        frameCtx->FenceValue = 0;
        if (Fence->GetCompletedValue() >= fenceValue)
            return;

        Fence->SetEventOnCompletion(fenceValue, FenceEvent);
        WaitForSingleObject(FenceEvent, INFINITE);
    }

    HRESULT PipelineInterface::Present(unsigned SyncInterval, unsigned Flags) const
    {
        return SwapChain->Present(SyncInterval, Flags);
    }

    unsigned PipelineInterface::GetCurrentBackBufferIndex() const
    {
        return SwapChain->GetCurrentBackBufferIndex();
    }

    void PipelineInterface::InsertRenderTargetBarrier(FrameContext* frameCtx, unsigned BackbufferIndex, D3D12_RESOURCE_BARRIER& OutBarrier) const
    {
        OutBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        OutBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        OutBarrier.Transition.pResource = MainRenderTargetResources[BackbufferIndex];
        OutBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        OutBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        OutBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        D3DCommandList->Reset(frameCtx->CommandAllocator, nullptr);
        D3DCommandList->ResourceBarrier(1, &OutBarrier);
    }

    void PipelineInterface::ClearRenderTargetView(unsigned int BackBufferIndex, const float ColorRGBA[4], unsigned int NumRects, const D3D12_RECT* pRects) const
    {
        D3DCommandList->ClearRenderTargetView(MainRenderTargetDescriptors[BackBufferIndex], ColorRGBA, NumRects, pRects);
    }

    void PipelineInterface::OMSetRenderTargets(unsigned NumRenderTargetDescriptors, unsigned int BackBufferIndex, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) const
    {
        D3DCommandList->OMSetRenderTargets(NumRenderTargetDescriptors, MainRenderTargetDescriptors + BackBufferIndex, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
    }

    void PipelineInterface::SetDescriptorHeaps(unsigned NumDescriptorHeaps) const
    {
        D3DCommandList->SetDescriptorHeaps(NumDescriptorHeaps, &D3DSrvDescHeap);
    }

    void PipelineInterface::ExecuteCommandLists()
    {
        D3DCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&D3DCommandList);
    }

    void PipelineInterface::CreateRenderTarget()
    {
        for (UINT i = 0; i < BackBufferCount; i++)
        {
            ID3D12Resource* pBackBuffer = nullptr;
            SwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            D3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, MainRenderTargetDescriptors[i]);
            MainRenderTargetResources[i] = pBackBuffer;
        }
    }

    void PipelineInterface::CleanupRenderTarget()
    {
        WaitForLastSubmittedFrame();

        for (UINT i = 0; i < BackBufferCount; i++)
        {
            if (MainRenderTargetResources[i])
            {
                MainRenderTargetResources[i]->Release();
                MainRenderTargetResources[i] = nullptr;
            }
        }
    }

    ID3D12GraphicsCommandList* PipelineInterface::GetCommandList()
    {
        return D3DCommandList;
    }

    ID3D12CommandQueue* PipelineInterface::GetCommandQueue()
    {
        return D3DCommandQueue;
    }

    IDXGISwapChain3* PipelineInterface::GetSwapChain()
    {
        return SwapChain;   
    }

    ID3D12Fence* PipelineInterface::GetFence()
    {
        return Fence;
    }
}