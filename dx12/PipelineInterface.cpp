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

// 删除全局变量
// static ExampleDescriptorHeapAllocator* g_CurrentDescriptorHeapAllocator = nullptr;
    
    ErrorCode PipelineInterface::Initialize(HWND hWnd)
    {
        // Setup swap chain
        DXGI_SWAP_CHAIN_DESC1 sd;
        {
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = APP_NUM_BACK_BUFFERS;
            sd.Width = 0;
            sd.Height = 0;
            sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            sd.Scaling = DXGI_SCALING_STRETCH;
            sd.Stereo = FALSE;
        }

#ifdef DX12_ENABLE_DEBUG_LAYER
        ID3D12Debug* pdx12Debug = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
            pdx12Debug->EnableDebugLayer();
#endif

        // Create device
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        if (D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
            return ErrorCode_Failed;

#ifdef DX12_ENABLE_DEBUG_LAYER
        if (pdx12Debug != nullptr)
        {
            ID3D12InfoQueue* pInfoQueue = nullptr;
            g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
            pInfoQueue->Release();
            pdx12Debug->Release();
        }
#endif

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = APP_NUM_BACK_BUFFERS;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            desc.NodeMask = 1;
            if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
                return ErrorCode_Failed;

            SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < APP_NUM_BACK_BUFFERS; i++)
            {
                g_mainRenderTargetDescriptor[i] = rtvHandle;
                rtvHandle.ptr += rtvDescriptorSize;
            }
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = APP_SRV_HEAP_SIZE;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
                return ErrorCode_Failed;
            
            g_pd3dSrvDescHeapAlloc.Create(g_pd3dDevice, g_pd3dSrvDescHeap);
        }

        {
            D3D12_COMMAND_QUEUE_DESC desc = {};
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.NodeMask = 1;
            if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
                return ErrorCode_Failed;
        }

        for (UINT i = 0; i < APP_NUM_FRAMES_IN_FLIGHT; i++)
            if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
                return ErrorCode_Failed;

        if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
                g_pd3dCommandList->Close() != S_OK)
                    return ErrorCode_Failed;

        if (g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
            return ErrorCode_Failed;

        g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (g_fenceEvent == nullptr)
            return ErrorCode_Failed;

        {
            IDXGIFactory4* dxgiFactory = nullptr;
            IDXGISwapChain1* swapChain1 = nullptr;
            if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
                return ErrorCode_Failed;
            if (dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, nullptr, nullptr, &swapChain1) != S_OK)
                return ErrorCode_Failed;
            if (swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
                return ErrorCode_Failed;
            swapChain1->Release();
            dxgiFactory->Release();
            g_pSwapChain->SetMaximumFrameLatency(APP_NUM_BACK_BUFFERS);
            g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
        }

        for (UINT i = 0; i < APP_NUM_BACK_BUFFERS; i++)
        {
            ID3D12Resource* pBackBuffer = nullptr;
            g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, g_mainRenderTargetDescriptor[i]);
            g_mainRenderTargetResource[i] = pBackBuffer;
        }
        
        return ErrorCode_OK;
    }

    void PipelineInterface::CleanUp()
    {
        //  WaitForLastSubmittedFrame
        FrameContext* frameCtx = &g_frameContext[g_frameIndex % APP_NUM_FRAMES_IN_FLIGHT];

        UINT64 fenceValue = frameCtx->FenceValue;
        if (fenceValue == 0)
            return; // No fence was signaled

        frameCtx->FenceValue = 0;
        if (g_fence->GetCompletedValue() >= fenceValue)
            return;

        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);

        //  CleanupRenderTarget
        for (UINT i = 0; i < APP_NUM_BACK_BUFFERS; i++)
            if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release(); g_mainRenderTargetResource[i] = nullptr; }

        //  Do clean
        if (g_pSwapChain) { g_pSwapChain->SetFullscreenState(false, nullptr); g_pSwapChain->Release(); g_pSwapChain = nullptr; }
        if (g_hSwapChainWaitableObject != nullptr) { CloseHandle(g_hSwapChainWaitableObject); }
        for (UINT i = 0; i < APP_NUM_FRAMES_IN_FLIGHT; i++)
            if (g_frameContext[i].CommandAllocator) { g_frameContext[i].CommandAllocator->Release(); g_frameContext[i].CommandAllocator = nullptr; }
        if (g_pd3dCommandQueue) { g_pd3dCommandQueue->Release(); g_pd3dCommandQueue = nullptr; }
        if (g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = nullptr; }
        if (g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = nullptr; }
        if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = nullptr; }
        if (g_fence) { g_fence->Release(); g_fence = nullptr; }
        if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }

#ifdef DX12_ENABLE_DEBUG_LAYER
        IDXGIDebug1* pDebug = nullptr;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
        {
            pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
            pDebug->Release();
        }
#endif
    }

    void PipelineInterface::PackImGuiParameter(ImGui_ImplDX12_InitInfo& OutParam)
    {
        OutParam.Device = g_pd3dDevice;
        OutParam.CommandQueue = g_pd3dCommandQueue;
        OutParam.NumFramesInFlight = APP_NUM_FRAMES_IN_FLIGHT;
        OutParam.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        OutParam.DSVFormat = DXGI_FORMAT_UNKNOWN;
        // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
        // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
        OutParam.SrvDescriptorHeap = g_pd3dSrvDescHeap;
        
        // 使用UserData字段传递this指针
        OutParam.UserData = this;
        
        // 使用没有捕获的lambda，通过UserData访问实例
        OutParam.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { 
            PipelineInterface* Instance = static_cast<PipelineInterface*>(info->UserData);
            return Instance->g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); 
        };
        
        OutParam.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { 
            PipelineInterface* Instance = static_cast<PipelineInterface*>(info->UserData);
            return Instance->g_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); 
        };
    }
    
    FrameContext* PipelineInterface::WaitForNextFrameResources()
    {
        UINT nextFrameIndex = g_frameIndex + 1;
        g_frameIndex = nextFrameIndex;

        HANDLE waitableObjects[] = { g_hSwapChainWaitableObject, nullptr };
        DWORD numWaitableObjects = 1;

        FrameContext* frameCtx = &g_frameContext[nextFrameIndex % APP_NUM_FRAMES_IN_FLIGHT];
        UINT64 fenceValue = frameCtx->FenceValue;
        if (fenceValue != 0) // means no fence was signaled
            {
            frameCtx->FenceValue = 0;
            g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
            waitableObjects[1] = g_fenceEvent;
            numWaitableObjects = 2;
            }

        WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

        return frameCtx;
    }

    void PipelineInterface::WaitForLastSubmittedFrame()
    {
        FrameContext* frameCtx = &g_frameContext[g_frameIndex % APP_NUM_FRAMES_IN_FLIGHT];

        UINT64 fenceValue = frameCtx->FenceValue;
        if (fenceValue == 0)
            return; // No fence was signaled

        frameCtx->FenceValue = 0;
        if (g_fence->GetCompletedValue() >= fenceValue)
            return;

        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    HRESULT PipelineInterface::Present(unsigned SyncInterval, unsigned Flags) const
    {
        return g_pSwapChain->Present(SyncInterval, Flags);
    }

    unsigned PipelineInterface::GetCurrentBackBufferIndex() const
    {
        return g_pSwapChain->GetCurrentBackBufferIndex();
    }

    void PipelineInterface::InsertRenderTargetBarrier(FrameContext* frameCtx, unsigned BackbufferIndex, D3D12_RESOURCE_BARRIER& OutBarrier) const
    {
        OutBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        OutBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        OutBarrier.Transition.pResource = g_mainRenderTargetResource[BackbufferIndex];
        OutBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        OutBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        OutBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_pd3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
        g_pd3dCommandList->ResourceBarrier(1, &OutBarrier);
    }

    void PipelineInterface::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const float ColorRGBA[4], unsigned NumRects, const D3D12_RECT* pRects) const
    {
        g_pd3dCommandList->ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);
    }

    void PipelineInterface::OMSetRenderTargets(unsigned NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) const
    {
        g_pd3dCommandList->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
    }

    void PipelineInterface::SetDescriptorHeaps(unsigned NumDescriptorHeaps) const
    {
        g_pd3dCommandList->SetDescriptorHeaps(NumDescriptorHeaps, &g_pd3dSrvDescHeap);
    }

    void PipelineInterface::ExecuteCommandLists(unsigned NumCommandLists)
    {
        g_pd3dCommandQueue->ExecuteCommandLists(NumCommandLists, (ID3D12CommandList* const*)&g_pd3dCommandList);
    }

    void PipelineInterface::CreateRenderTarget()
    {
        for (UINT i = 0; i < APP_NUM_BACK_BUFFERS; i++)
        {
            ID3D12Resource* pBackBuffer = nullptr;
            g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, g_mainRenderTargetDescriptor[i]);
            g_mainRenderTargetResource[i] = pBackBuffer;
        }
    }

    void PipelineInterface::CleanupRenderTarget()
    {
        WaitForLastSubmittedFrame();

        for (UINT i = 0; i < APP_NUM_BACK_BUFFERS; i++)
            if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release(); g_mainRenderTargetResource[i] = nullptr; }
    }
}