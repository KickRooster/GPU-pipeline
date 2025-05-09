#include "PipelineInterface.h"
#include "../misc/Math.h"
#include "../actor/CameraActor.h"
#include <d3dcompiler.h>
#include <iostream>
#include <string>
#include <Windows.h>

#ifndef _DEBUG
#define _DEBUG 1
#endif

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

ErrorCode PipelineInterface::Initialize(HWND hWnd)
{
    UINT DxgiFactoryFlags = 0;

#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* DX12Debug = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DX12Debug))))
    {
        DX12Debug->EnableDebugLayer();

        // Enable additional debug layers.
        DxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
    else
    {
        return ErrorCode::DebugInterfaceNotFound;
    }
#endif

    //  Create device, use the default adapter
    if (D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&D3DDevice)) != S_OK)
    {
        return ErrorCode::DeviceCreateFailed;
    }

#ifdef DX12_ENABLE_DEBUG_LAYER
    if (DX12Debug != nullptr)
    {
        ID3D12InfoQueue* InfoQueue = nullptr;
        D3DDevice->QueryInterface(IID_PPV_ARGS(&InfoQueue));
        InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        InfoQueue->Release();
        DX12Debug->Release();
    }
#endif

    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        FrameContexts.push_back(FrameContext());
        if (D3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&FrameContexts[I].CommandAllocator)) != S_OK)
        {
            return ErrorCode::CommandAllocatorCreateFailed;
        }

        if (D3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameContexts[I].CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&FrameContexts[I].CommandList)) != S_OK)
        {
            return ErrorCode::CommandListCreateFailed;
        }

        if (FrameContexts[I].CommandList->Close() != S_OK)
        {
            return ErrorCode::CommandListCloseFailed;
        }
    }

    //  RTV(imgui & level rendering)
    {
        D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
        DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        //  BackBufferCount(imgui) + FrameNumInFlight(render target)
        DescriptorHeapDesc.NumDescriptors = BackBufferCount + FrameNumInFlight;
        DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        //  XXX:    NodeMask?
        DescriptorHeapDesc.NodeMask = 0;
        if (D3DDevice->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&D3DRTVDescHeap)) != S_OK)
        {
            return ErrorCode::DescriptorHeapCreateFailed;
        }

        SIZE_T DescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = D3DRTVDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (int I = 0; I < BackBufferCount; ++I)
        {
            IMGUIRenderTargetDescriptorHandles.push_back(RTVHandle);
            RTVHandle.ptr += DescriptorSize;
        }

        for (int I = 0; I < FrameNumInFlight; ++I)
        {
            FrameContexts[I].RenderTargetCPUDescriptorHandle = RTVHandle;
            RTVHandle.ptr += DescriptorSize;
        }
    }

    //  DSV
    {
        D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc;
        DescriptorHeapDesc.NumDescriptors = FrameNumInFlight;
        DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        DescriptorHeapDesc.NodeMask = 0;
        
        if (D3DDevice->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&D3DDSDescHeap)) != S_OK)
        {
            return ErrorCode::DescriptorHeapCreateFailed;
        }

        SIZE_T DescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle = D3DDSDescHeap->GetCPUDescriptorHandleForHeapStart();

        for (int I = 0; I < FrameNumInFlight; ++I)
        {
            FrameContexts[I].DepthStencilCPUDescriptorHandle = DSVHandle;
            DSVHandle.ptr += DescriptorSize;
        }
    }

    //  SRV & CBV
    {
        D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
        DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        DescriptorHeapDesc.NumDescriptors = SRVHeapSize;
        DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (D3DDevice->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&D3DSRVCBVDescHeap)) != S_OK)
        {
            return ErrorCode::DescriptorHeapCreateFailed;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = D3DSRVCBVDescHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = D3DSRVCBVDescHeap->GetGPUDescriptorHandleForHeapStart();
        unsigned int IncrementSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //  First FrameNumInFlight D3D12_CPU_DESCRIPTOR_HANDLE of D3DSRVDescHeap is reserved for level's render target.
        for (int I = 0; I < FrameNumInFlight; ++I)
        {
            FrameContexts[I].RenderTargetSRVCPUDescriptorHandle = CPUHandle;
            FrameContexts[I].RenderTargetSRVGPUDescriptorHandle = GPUHandle;
            CPUHandle.ptr += IncrementSize;
            GPUHandle.ptr += IncrementSize;
        }
        //  Then D3D12_CPU_DESCRIPTOR_HANDLE of D3DSRVDescHeap is reserved for constant buffer.
        ConstantBufferCPUHandle = CPUHandle;
        CPUHandle.ptr += IncrementSize;

        ConstantBufferGPUHandle = GPUHandle;
        GPUHandle.ptr += IncrementSize;

        D3DSRVDescriptorHeapAllocator.Create(D3DDevice.Get(), D3DSRVCBVDescHeap.Get(), FrameNumInFlight + 1);
    }

    D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
    CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    //  XXX: NodeMask?
    CommandQueueDesc.NodeMask = 1;
    if (D3DDevice->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&D3DCommandQueue)) != S_OK)
    {
        return ErrorCode::Failed;
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

    IDXGIFactory4* DxgiFactory = nullptr;
    IDXGISwapChain1* SwapChain1 = nullptr;
    if (CreateDXGIFactory2(DxgiFactoryFlags, IID_PPV_ARGS(&DxgiFactory)) != S_OK)
    {
        return ErrorCode::DXGIFactoryCreateFailed;
    }

    if (DxgiFactory->CreateSwapChainForHwnd(D3DCommandQueue.Get(), hWnd, &SwapChainDesc, nullptr, nullptr, &SwapChain1) != S_OK)
    {
        return ErrorCode::SwapChainForHwndCreateFailed;
    }

    if (SwapChain1->QueryInterface(IID_PPV_ARGS(&SwapChain)) != S_OK)
    {
        return ErrorCode::QueryIDXGISwapChain3InterfaceFailed;
    }

    SwapChain1->Release();
    DxgiFactory->Release();
    SwapChain->SetMaximumFrameLatency(BackBufferCount);
    SwapChainWaitableObject = SwapChain->GetFrameLatencyWaitableObject();

    for (int I = 0; I < BackBufferCount; ++I)
    {
        ID3D12Resource* BackBuffer = nullptr;
        SwapChain->GetBuffer(I, IID_PPV_ARGS(&BackBuffer));
        D3DDevice->CreateRenderTargetView(BackBuffer, nullptr, IMGUIRenderTargetDescriptorHandles[I]);
        IMGUIRenderTargetResources.push_back(BackBuffer);
    }

    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(D3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    CD3DX12_DESCRIPTOR_RANGE1 Ranges[1];
    CD3DX12_ROOT_PARAMETER1 RootParameters[1];
    Ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    RootParameters[0].InitAsDescriptorTable(1, &Ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    
    D3D12_ROOT_SIGNATURE_FLAGS RootSignatureFlags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc;
    RootSignatureDesc.Init_1_1(_countof(RootParameters), RootParameters, 0, nullptr, RootSignatureFlags);

    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    D3DX12SerializeVersionedRootSignature(&RootSignatureDesc, FeatureData.HighestVersion, &Signature, &Error);
    D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature));

    ComPtr<ID3DBlob> VertexShader;
    ComPtr<ID3DBlob> PixelShader;

#ifdef DX12_ENABLE_DEBUG_LAYER
    // Enable better shader debugging with the graphics debugging tools.
    UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT CompileFlags = 0;
#endif

    std::wstring ShaderPath = L"D:\\GPU-pipeline\\content\\shader\\color.hlsl";
    ComPtr<ID3DBlob> Errors;
    HRESULT Result = D3DCompileFromFile(ShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_0", CompileFlags, 0, &VertexShader, &Errors);
    if (Errors != nullptr)
    {
        OutputDebugStringA((char*)Errors->GetBufferPointer());
    }
    
    Result = D3DCompileFromFile(ShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_0", CompileFlags, 0, &PixelShader, &Errors);
    if (Errors != nullptr)
    {
        OutputDebugStringA((char*)Errors->GetBufferPointer());
    }
    
    D3D12_INPUT_ELEMENT_DESC InputElementDescs[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.InputLayout = { InputElementDescs, _countof(InputElementDescs) };
    PSODesc.pRootSignature = RootSignature.Get();
    PSODesc.VS = CD3DX12_SHADER_BYTECODE(VertexShader.Get());
    PSODesc.PS = CD3DX12_SHADER_BYTECODE(PixelShader.Get());
    PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    
    PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState.DepthEnable = TRUE;
    PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    PSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    PSODesc.DepthStencilState.StencilEnable = FALSE;
    
    PSODesc.SampleMask = UINT_MAX;
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets = 1;
    PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    PSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    PSODesc.SampleDesc.Count = 1;
    D3DDevice->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PipelineState));

    return ErrorCode::OK;
}

void PipelineInterface::CleanUp()
{
    //  WaitForLastSubmittedFrame
    unsigned int FrameContextIndex = FrameIndex % FrameNumInFlight;
    FrameContext* FrameContext = &FrameContexts[FrameContextIndex];

    UINT64 FenceValue = FrameContext->FenceValue;
    if (FenceValue == 0)
    {
        return; // No fence was signaled
    }

    FrameContext->FenceValue = 0;
    if (Fence->GetCompletedValue() >= FenceValue)
    {
        return;
    }

    Fence->SetEventOnCompletion(FenceValue, FenceEvent);
    WaitForSingleObject(FenceEvent, INFINITE);

    //  CleanupRenderTarget
    for (int I = 0; I < BackBufferCount; ++I)
    {
        if (IMGUIRenderTargetResources[I])
        {
            IMGUIRenderTargetResources[I]->Release();
            IMGUIRenderTargetResources[I] = nullptr;
        }
    }
    
    IMGUIRenderTargetResources.clear();
    IMGUIRenderTargetDescriptorHandles.clear();

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

    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        if (FrameContexts[I].CommandAllocator)
        {
            FrameContexts[I].CommandAllocator->Release();
            FrameContexts[I].CommandAllocator = nullptr;
        }

        if (FrameContexts[I].CommandList)
        {
            FrameContexts[I].CommandList->Release();
            FrameContexts[I].CommandList = nullptr;
        }
    }
    FrameContexts.clear();

    if (D3DCommandQueue)
    {
        D3DCommandQueue->Release();
        D3DCommandQueue = nullptr;
    }

    if (D3DRTVDescHeap)
    {
        D3DRTVDescHeap->Release();
        D3DRTVDescHeap = nullptr;
    }

    if (D3DSRVCBVDescHeap)
    {
        D3DSRVCBVDescHeap->Release();
        D3DSRVCBVDescHeap = nullptr;
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

    if (RootSignature)
    {
        RootSignature->Release();
        RootSignature = nullptr;
    }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* Debug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&Debug))))
    {
        Debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        Debug->Release();
    }
#endif
}

void PipelineInterface::PackImGuiInitInfo(ImGui_ImplDX12_InitInfo& OutInitInfo)
{
    OutInitInfo.Device = D3DDevice.Get();
    OutInitInfo.CommandQueue = D3DCommandQueue.Get();
    OutInitInfo.NumFramesInFlight = FrameNumInFlight;
    OutInitInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    OutInitInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
    OutInitInfo.SrvDescriptorHeap = D3DSRVCBVDescHeap.Get();

    // Use UserData to pass this pointer
    OutInitInfo.UserData = this;

    OutInitInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) {
        PipelineInterface* Instance = static_cast<PipelineInterface*>(info->UserData);
        return Instance->D3DSRVDescriptorHeapAllocator.Alloc(out_cpu_handle, out_gpu_handle);
        };

    OutInitInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
        PipelineInterface* Instance = static_cast<PipelineInterface*>(info->UserData);
        return Instance->D3DSRVDescriptorHeapAllocator.Free(cpu_handle, gpu_handle);
        };
}
    
unsigned int PipelineInterface::WaitForNextFrameResources()
{
    ++FrameIndex;
    HANDLE WaitableObjects[] = { SwapChainWaitableObject, nullptr };
    DWORD NumWaitableObjects = 1;

    unsigned int FrameContextIndex = FrameIndex % FrameNumInFlight;
    UINT64 FenceValue = FrameContexts[FrameContextIndex].FenceValue;
    if (FenceValue != 0) // means no fence was signaled
    {
        FrameContexts[FrameContextIndex].FenceValue = 0;
        Fence->SetEventOnCompletion(FenceValue, FenceEvent);
        WaitableObjects[1] = FenceEvent;
        NumWaitableObjects = 2;
    }

    WaitForMultipleObjects(NumWaitableObjects, WaitableObjects, TRUE, INFINITE);

    return FrameContextIndex;
}

void PipelineInterface::WaitForLastSubmittedFrame()
{
    FrameContext* FrameContext = &FrameContexts[FrameIndex % FrameNumInFlight];

    unsigned long long FenceValue = FrameContext->FenceValue;
    //  No fence was signaled
    if (FenceValue == 0)
    {
        return;
    }

    FrameContext->FenceValue = 0;
    if (Fence->GetCompletedValue() < FenceValue)
    {
        Fence->SetEventOnCompletion(FenceValue, FenceEvent);
        WaitForSingleObject(FenceEvent, INFINITE);
    }
}

HRESULT PipelineInterface::Present(unsigned SyncInterval, unsigned Flags) const
{
    return SwapChain->Present(SyncInterval, Flags);
}

unsigned int PipelineInterface::GetCurrentBackBufferIndex() const
{
    return SwapChain->GetCurrentBackBufferIndex();
}
    
void PipelineInterface::InsertIMGUIRenderTargetBarrier(unsigned int FrameContextIndex, unsigned BackbufferIndex, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, D3D12_RESOURCE_BARRIER_TYPE BarrierType, D3D12_RESOURCE_BARRIER_FLAGS BarrierFlag) const
{
    D3D12_RESOURCE_BARRIER Barrier;
    Barrier.Type = BarrierType;
    Barrier.Flags = BarrierFlag;
    Barrier.Transition.pResource = IMGUIRenderTargetResources[BackbufferIndex].Get();
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    Barrier.Transition.StateBefore = StateBefore;
    Barrier.Transition.StateAfter = StateAfter;
    FrameContexts[FrameContextIndex].CommandList->ResourceBarrier(1, &Barrier);
}
    
void PipelineInterface::ClearIMGUIRenderTargetView(unsigned int FrameContextIndex, unsigned int BackBufferIndex, const float ColorRGBA[4], unsigned int NumRects, const D3D12_RECT* pRects) const
{
    FrameContexts[FrameContextIndex].CommandList->ClearRenderTargetView(IMGUIRenderTargetDescriptorHandles[BackBufferIndex], ColorRGBA, NumRects, pRects);
}

void PipelineInterface::OMSetIMGUIRenderTargets(unsigned int FrameContextIndex, unsigned NumRenderTargetDescriptors, unsigned int BackBufferIndex, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) const
{
    FrameContexts[FrameContextIndex].CommandList->OMSetRenderTargets(NumRenderTargetDescriptors, &IMGUIRenderTargetDescriptorHandles[BackBufferIndex], RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}

void PipelineInterface::ExecuteCommandLists(unsigned int FrameContextIndex) const
{
    ID3D12GraphicsCommandList* RawPointer = FrameContexts[FrameContextIndex].CommandList.Get();
    D3DCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&RawPointer);
}

void PipelineInterface::CreateIMGUIRenderTarget()
{
    for (int I = 0; I < BackBufferCount; ++I)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        SwapChain->GetBuffer(I, IID_PPV_ARGS(&pBackBuffer));
        D3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, IMGUIRenderTargetDescriptorHandles[I]);
        IMGUIRenderTargetResources[I] = pBackBuffer;
    }
}

void PipelineInterface::CleanupIMGUIRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (int I = 0; I < BackBufferCount; ++I)
    {
        if (IMGUIRenderTargetResources[I])
        {
            IMGUIRenderTargetResources[I]->Release();
            IMGUIRenderTargetResources[I] = nullptr;
        }
    }
}

void PipelineInterface::ResetCommandAllocator(unsigned FrameContextIndex) const
{
    FrameContexts[FrameContextIndex].CommandAllocator->Reset();
}
    
HRESULT PipelineInterface::ResetCommandList(unsigned int FrameContextIndex) const
{
    return FrameContexts[FrameContextIndex].CommandList->Reset(
        FrameContexts[FrameContextIndex].CommandAllocator.Get(),
        nullptr);
}

void PipelineInterface::Signal(unsigned long FenceValue) const
{
    D3DCommandQueue->Signal(Fence.Get(), FenceValue);
}
    
ID3D12GraphicsCommandList* PipelineInterface::GetCommandList(unsigned FrameContextIndex) const
{
    return FrameContexts[FrameContextIndex].CommandList.Get();
}


IDXGISwapChain3* PipelineInterface::GetSwapChain()
{
    return SwapChain.Get();
}

void PipelineInterface::UpdateFrameContextFenceValue(unsigned FrameContextIndex, unsigned long FenceValue)
{
    FrameContexts[FrameContextIndex].FenceValue = FenceValue;
}

D3D12_GPU_DESCRIPTOR_HANDLE PipelineInterface::GetRenderTargetSRVGPUHandle(unsigned int FrameContextIndex) const
{
    return FrameContexts[FrameContextIndex].RenderTargetSRVGPUDescriptorHandle;
}

//  Deprecated, for reading only.
// void PipelineInterface::CreateVertexBuffer(const Shape* ShapeInstance, ShapeProxy* ShapeProxyInstance)
// {
//     const unsigned int VertexBufferSize = sizeof(Vertex) * ShapeInstance->GetVertices().size();
//     
//     // 1. Create vertex buffer on default heap
//     CD3DX12_HEAP_PROPERTIES DefaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
//     CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);
//     D3DDevice->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&ShapeProxyInstance->VertexBuffer));
//     
//     // 2. Create vertex buffer on upload heap
//     CD3DX12_HEAP_PROPERTIES UploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
//     D3DDevice->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ShapeProxyInstance->VertexBufferUpload));
//     
//     // 3. Copy data to vertex buffer for uploading
//     unsigned int* VertexDataBegin;
//     CD3DX12_RANGE ReadRange(0, 0);        // We do not intend to read from this resource on the CPU.
//     ShapeProxyInstance->VertexBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&VertexDataBegin));
//     memcpy(VertexDataBegin, ShapeInstance->GetVertices().data(), VertexBufferSize);
//     ShapeProxyInstance->VertexBufferUpload->Unmap(0, nullptr);
//     
//     FrameContext& FrameContext = FrameContexts[0];
//     
//     // 4. Reset command list for initializing resource, any command list is okay
//     FrameContext.CommandAllocator->Reset();
//     FrameContext.CommandList->Reset(FrameContext.CommandAllocator.Get(), nullptr);
//     
//     // 5. Record copy command
//     FrameContext.CommandList->CopyBufferRegion(
//         ShapeProxyInstance->VertexBuffer.Get(), 0,
//         ShapeProxyInstance->VertexBufferUpload.Get(), 0,
//         VertexBufferSize);
//     
//     // 6. Insert a barrier
//     CD3DX12_RESOURCE_BARRIER BufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
//         ShapeProxyInstance->VertexBuffer.Get(),
//         D3D12_RESOURCE_STATE_COPY_DEST,
//         D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
//     FrameContext.CommandList->ResourceBarrier(1, &BufferBarrier);
//     
//     // 7. Close command list
//     FrameContext.CommandList->Close();
//     
//     // 8. Execute command list
//     ID3D12GraphicsCommandList* CommandListPointer = FrameContext.CommandList.Get();
//     D3DCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&CommandListPointer);
//     
//     // 9. Wait for it completion on GPU
//     UINT64 FenceValue = 1;
//     D3DCommandQueue->Signal(Fence.Get(), FenceValue);
//     if (Fence->GetCompletedValue() < FenceValue)
//     {
//         Fence->SetEventOnCompletion(FenceValue, FenceEvent);
//         WaitForSingleObject(FenceEvent, INFINITE);
//     }
//     
//     // 10. Initialize vertex buffer view, we will use it later when using it
//     ShapeProxyInstance->VertexBufferView.BufferLocation = ShapeProxyInstance->VertexBuffer->GetGPUVirtualAddress();
//     ShapeProxyInstance->VertexBufferView.StrideInBytes = sizeof(Vertex);
//     ShapeProxyInstance->VertexBufferView.SizeInBytes = VertexBufferSize;
// }

void PipelineInterface::CreateMeshProxyBuffer(const Mesh* MeshInstance, MeshProxy* MeshProxyInstance)
{
    const unsigned int VertexBufferSize = sizeof(Vertex) * MeshInstance->Vertices.size();
    const unsigned int IndexBufferSize = sizeof(unsigned int) * MeshInstance->Indices.size();
    
    //  Create vertex buffer on default heap
    CD3DX12_HEAP_PROPERTIES DefaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC VertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);
    D3DDevice->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &VertexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&MeshProxyInstance->VertexBuffer));

    //  Create index buffer on default heap
    CD3DX12_RESOURCE_DESC IndexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferSize);
    D3DDevice->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &IndexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&MeshProxyInstance->IndexBuffer));

    //  Create vertex buffer on upload heap
    CD3DX12_HEAP_PROPERTIES UploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    D3DDevice->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &VertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&MeshProxyInstance->VertexBufferUpload));

    //  Create index buffer on upload heap
    D3DDevice->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &IndexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&MeshProxyInstance->IndexBufferUpload));
    
    //  Copy data to vertex buffer for uploading
    unsigned int* VertexDataBegin;
    CD3DX12_RANGE VertexReadRange(0, 0);        // We do not intend to read from this resource on the CPU.
    MeshProxyInstance->VertexBufferUpload->Map(0, &VertexReadRange, reinterpret_cast<void**>(&VertexDataBegin));
    memcpy(VertexDataBegin, MeshInstance->Vertices.data(), VertexBufferSize);
    MeshProxyInstance->VertexBufferUpload->Unmap(0, nullptr);

    //  Copy data to index buffer for uploading
    unsigned int* IndexDataBegin;
    CD3DX12_RANGE IndexReadRange = CD3DX12_RANGE(0, 0);        // We do not intend to read from this resource on the CPU.
    MeshProxyInstance->IndexBufferUpload->Map(0, &IndexReadRange, reinterpret_cast<void**>(&IndexDataBegin));
    memcpy(IndexDataBegin, MeshInstance->Indices.data(), IndexBufferSize);
    MeshProxyInstance->IndexBufferUpload->Unmap(0, nullptr);
    
    FrameContext& FrameContext = FrameContexts[0];
    
    //  Reset command list for initializing resource, any command list is okay
    FrameContext.CommandAllocator->Reset();
    FrameContext.CommandList->Reset(FrameContext.CommandAllocator.Get(), nullptr);
    
    //  Record copy command
    FrameContext.CommandList->CopyBufferRegion(
        MeshProxyInstance->VertexBuffer.Get(), 0,
        MeshProxyInstance->VertexBufferUpload.Get(), 0,
        VertexBufferSize);
    
    //  Insert a barrier
    CD3DX12_RESOURCE_BARRIER BufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        MeshProxyInstance->VertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    FrameContext.CommandList->ResourceBarrier(1, &BufferBarrier);

    //  Record copy command
    FrameContext.CommandList->CopyBufferRegion(
        MeshProxyInstance->IndexBuffer.Get(), 0,
        MeshProxyInstance->IndexBufferUpload.Get(), 0,
        IndexBufferSize);
    
    //  Insert a barrier
    BufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        MeshProxyInstance->IndexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER);
    FrameContext.CommandList->ResourceBarrier(1, &BufferBarrier);
    
    //  Close command list
    FrameContext.CommandList->Close();
    
    //  Execute command list
    ID3D12GraphicsCommandList* CommandListPointer = FrameContext.CommandList.Get();
    D3DCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&CommandListPointer);
    
    //  Wait for it completion on GPU
    UINT64 FenceValue = 1;
    D3DCommandQueue->Signal(Fence.Get(), FenceValue);
    if (Fence->GetCompletedValue() < FenceValue)
    {
        Fence->SetEventOnCompletion(FenceValue, FenceEvent);
        WaitForSingleObject(FenceEvent, INFINITE);
    }
    
    //  Initialize vertex buffer view, we will use it later when using it
    MeshProxyInstance->VertexBufferView.BufferLocation = MeshProxyInstance->VertexBuffer->GetGPUVirtualAddress();
    MeshProxyInstance->VertexBufferView.StrideInBytes = sizeof(Vertex);
    MeshProxyInstance->VertexBufferView.SizeInBytes = VertexBufferSize;

    //  Initialize index buffer view, we will use it later when using it
    MeshProxyInstance->IndexBufferView.BufferLocation = MeshProxyInstance->IndexBuffer->GetGPUVirtualAddress();
    MeshProxyInstance->IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    MeshProxyInstance->IndexBufferView.SizeInBytes = IndexBufferSize;
}

void PipelineInterface::CreateConstantBuffer(const CameraActor* CameraActorInstance)
{
    const unsigned int ByteSize = MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(CameraActorInstance->GetConstantBuffer()));
    ConstantBufferProxy* BufferProxy = CameraActorInstance->GetConstantBufferProxy();
    BufferProxy->ElementByteSize = ByteSize;

    CD3DX12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ByteSize);
    D3DDevice->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&BufferProxy->UploadBuffer));

    BufferProxy->UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&BufferProxy->MappedData));
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC ConstantBufferViewDesc;
    ConstantBufferViewDesc.BufferLocation = BufferProxy->UploadBuffer->GetGPUVirtualAddress();
    ConstantBufferViewDesc.SizeInBytes = ByteSize;

    D3DDevice->CreateConstantBufferView(
        &ConstantBufferViewDesc,
        ConstantBufferCPUHandle);

    // CD3DX12_RANGE ReadRange(0, 0);        // We do not intend to read from this resource on the CPU.
    // ConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&ConstantBufferDataBegin));
    // memcpy(ConstantBufferDataBegin, &Buffer, ConstantBufferSize);
}

void PipelineInterface::UpdateViewport(unsigned int FrameContextIndex, ImVec2 NewViewportSize)
{
    bool SizeChanged = ViewportSize.x != NewViewportSize.x || ViewportSize.y != NewViewportSize.y;
    
    if (SizeChanged || bResizedLastFrame)
    {
        D3D12_RESOURCE_DESC RenderTargetDesc = {};
        RenderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        RenderTargetDesc.Width = static_cast<UINT64>(NewViewportSize.x);
        RenderTargetDesc.Height = static_cast<UINT64>(NewViewportSize.y);
        RenderTargetDesc.DepthOrArraySize = 1;
        RenderTargetDesc.MipLevels = 1;
        RenderTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        RenderTargetDesc.SampleDesc.Count = 1;
        RenderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_HEAP_PROPERTIES HeapProps = {};
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_CLEAR_VALUE ClearValue = {};
        ClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        ClearValue.Color[0] = 0.0f;
        ClearValue.Color[1] = 0.0f;
        ClearValue.Color[2] = 0.0f;
        ClearValue.Color[3] = 1.0f;

        D3DDevice->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &RenderTargetDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &ClearValue,
            IID_PPV_ARGS(&FrameContexts[FrameContextIndex].RenderTarget));

        D3DDevice->CreateRenderTargetView(FrameContexts[FrameContextIndex].RenderTarget.Get(), nullptr, FrameContexts[FrameContextIndex].RenderTargetCPUDescriptorHandle);

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2D.MipLevels = 1;
        D3DDevice->CreateShaderResourceView(FrameContexts[FrameContextIndex].RenderTarget.Get(), &SRVDesc, FrameContexts[FrameContextIndex].RenderTargetSRVCPUDescriptorHandle);
        
        D3D12_RESOURCE_DESC DepthStencilDesc;
        DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        DepthStencilDesc.Alignment = 0;
        DepthStencilDesc.Width = static_cast<UINT64>(NewViewportSize.x);;
        DepthStencilDesc.Height = static_cast<UINT64>(NewViewportSize.y);;
        DepthStencilDesc.DepthOrArraySize = 1;
        DepthStencilDesc.MipLevels = 1;

        // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
        // the depth buffer.  Therefore, because we need to create two views to the same resource:
        //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
        //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
        // we need to create the depth buffer resource with a typeless format.  
        DepthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        DepthStencilDesc.SampleDesc.Count = 1;
        DepthStencilDesc.SampleDesc.Quality = 0;
        DepthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        DepthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE DepthStencilClearValue;
        DepthStencilClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        DepthStencilClearValue.DepthStencil.Depth = 1.0f;
        DepthStencilClearValue.DepthStencil.Stencil = 0;

        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3DDevice->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &DepthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &DepthStencilClearValue,
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].DepthStencilBuffer.GetAddressOf()));

        D3D12_DEPTH_STENCIL_VIEW_DESC DeptStencilViewDesc;
        DeptStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;
        DeptStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        DeptStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        DeptStencilViewDesc.Texture2D.MipSlice = 0;
        D3DDevice->CreateDepthStencilView(FrameContexts[FrameContextIndex].DepthStencilBuffer.Get(), &DeptStencilViewDesc, FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle);

        CD3DX12_RESOURCE_BARRIER BufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].DepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
        
        FrameContexts[FrameContextIndex].CommandList->ResourceBarrier(1, &BufferBarrier);

        ViewportSize.x = NewViewportSize.x;
        ViewportSize.y = NewViewportSize.y;
    }

    if (SizeChanged && FrameNumInFlight > 1)
    {
        bResizedLastFrame = true;
    }
    else
    {
        bResizedLastFrame = false;
    }
}

void PipelineInterface::RenderLevel(unsigned int FrameContextIndex, const Level* LevelInstance)
{
    D3D12_RESOURCE_BARRIER Barrier = {};
    Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    Barrier.Transition.pResource = FrameContexts[FrameContextIndex].RenderTarget.Get();
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    FrameContexts[FrameContextIndex].CommandList->ResourceBarrier(1, &Barrier);
    FrameContexts[FrameContextIndex].CommandList->OMSetRenderTargets(1, &FrameContexts[FrameContextIndex].RenderTargetCPUDescriptorHandle, false, &FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle);

    ID3D12DescriptorHeap* RawHeap = D3DSRVCBVDescHeap.Get();
    FrameContexts[FrameContextIndex].CommandList->SetDescriptorHeaps(1, &RawHeap);
    
    const float ClearColor[] = { 0, 0, 0, 1.0f };
    FrameContexts[FrameContextIndex].CommandList->ClearRenderTargetView(FrameContexts[FrameContextIndex].RenderTargetCPUDescriptorHandle, ClearColor, 0, nullptr);

    FrameContexts[FrameContextIndex].CommandList->ClearDepthStencilView(FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    
    CD3DX12_VIEWPORT ViewPort = CD3DX12_VIEWPORT(0.f, 0.f, ViewportSize.x, ViewportSize.y);
    CD3DX12_RECT ScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(ViewportSize.x), static_cast<LONG>(ViewportSize.y));
    FrameContexts[FrameContextIndex].CommandList->RSSetViewports(1, &ViewPort);
    FrameContexts[FrameContextIndex].CommandList->RSSetScissorRects(1, &ScissorRect);

    FrameContexts[FrameContextIndex].CommandList->SetGraphicsRootSignature(RootSignature.Get());
    FrameContexts[FrameContextIndex].CommandList->SetPipelineState(PipelineState.Get());

    for (int I = 0; I < LevelInstance->GetActors().size(); ++I)
    {
        for (unsigned int SubMeshIndex = 0; SubMeshIndex < LevelInstance->GetActors()[I]->GetSubMeshCount(); ++SubMeshIndex)
        {
            D3D12_VERTEX_BUFFER_VIEW VertexBufferView = LevelInstance->GetActors()[I]->GetMeshProxyInstance(SubMeshIndex)->VertexBufferView;//GetVertexBufferView();
            FrameContexts[FrameContextIndex].CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
            D3D12_INDEX_BUFFER_VIEW IndexBufferView = LevelInstance->GetActors()[I]->GetMeshProxyInstance(SubMeshIndex)->IndexBufferView;//GetIndexBufferView();
            FrameContexts[FrameContextIndex].CommandList->IASetIndexBuffer(&IndexBufferView);
            FrameContexts[FrameContextIndex].CommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            FrameContexts[FrameContextIndex].CommandList->SetGraphicsRootDescriptorTable(0, ConstantBufferGPUHandle);
    
            FrameContexts[FrameContextIndex].CommandList->DrawIndexedInstanced(
            LevelInstance->GetActors()[I]->GetMeshInstance(SubMeshIndex)->Indices.size(),
                1, 0, 0, 0);
        }
    }
    
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    FrameContexts[FrameContextIndex].CommandList->ResourceBarrier(1, &Barrier);
}