#include "PipelineInterface.h"
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <iostream>
#include <sstream>
#include <string>
#include <Windows.h>

#ifndef _DEBUG
#define _DEBUG 1
#endif

namespace dev
{
#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

// 添加调试辅助函数
void OutputDebugMessage(ID3D12InfoQueue* infoQueue)
{
    UINT64 messageCount = infoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < messageCount; i++)
    {
        SIZE_T messageSize = 0;
        infoQueue->GetMessage(i, nullptr, &messageSize);
        
        D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(messageSize);
        infoQueue->GetMessage(i, message, &messageSize);
            
        std::stringstream ss;
        ss << std::string("D3D12 Debug: ") 
           << message->pDescription 
           << " [Severity: " << static_cast<int>(message->Severity) << "]";
            
        OutputDebugStringA(ss.str().c_str());
        std::cerr << ss.str();
            
        free(message);
    }
    
    infoQueue->ClearStoredMessages();
}
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

        //  RTV(imgui & level rendering)
        {
            D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
            DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            //  BackBufferCount(imgui) + 1(render target)
            DescriptorHeapDesc.NumDescriptors = BackBufferCount + 1;
            DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            DescriptorHeapDesc.NodeMask = 1;
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
            LevelRenderTargetDescriptorHandle = RTVHandle;
            RTVHandle.ptr += DescriptorSize;
        }
    
        //  SRV
        {
            D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
            DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            DescriptorHeapDesc.NumDescriptors = SRVHeapSize;
            DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (D3DDevice->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&D3DSRVDescHeap)) != S_OK)
            {
                return ErrorCode::DescriptorHeapCreateFailed;
            }

            //  First D3D12_CPU_DESCRIPTOR_HANDLE of D3DSRVDescHeap is reserved for level's render target.
            LevelSRVGPUHandle = D3DSRVDescHeap->GetGPUDescriptorHandleForHeapStart();
            D3DSRVDescriptorHeapAllocator.Create(D3DDevice.Get(), D3DSRVDescHeap.Get(), 1);
        }

        D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
        CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        CommandQueueDesc.NodeMask = 1;
        if (D3DDevice->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&D3DCommandQueue)) != S_OK)
        {
            return ErrorCode::Failed;
        }
    
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

        D3D12_RESOURCE_DESC RenderTargetDesc = {};
        RenderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        RenderTargetDesc.Width = 512;
        RenderTargetDesc.Height = 512;
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
            IID_PPV_ARGS(&RenderTarget));
            
        D3DDevice->CreateRenderTargetView(RenderTarget.Get(), nullptr, LevelRenderTargetDescriptorHandle);
        LevelRenderTargetResource = RenderTarget;

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2D.MipLevels = 1;
        D3DDevice->CreateShaderResourceView(RenderTarget.Get(), &SRVDesc, D3DSRVDescHeap->GetCPUDescriptorHandleForHeapStart());
         
        CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
        RootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> Signature;
        ComPtr<ID3DBlob> Error;
        D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error);
        D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature));
        
        ComPtr<ID3DBlob> VertexShader;
        ComPtr<ID3DBlob> PixelShader;

#ifdef DX12_ENABLE_DEBUG_LAYER
        // Enable better shader debugging with the graphics debugging tools.
        UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT CompileFlags = 0;
#endif

        std::wstring ShaderPath = L"D:\\GPU-pipeline\\shaders\\shaders.hlsl";
        D3DCompileFromFile(ShaderPath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", CompileFlags, 0, &VertexShader, nullptr);
        D3DCompileFromFile(ShaderPath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", CompileFlags, 0, &PixelShader, nullptr);

        D3D12_INPUT_ELEMENT_DESC InputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
        PSODesc.InputLayout = {InputElementDescs, _countof(InputElementDescs) };
        PSODesc.pRootSignature = RootSignature.Get();
        PSODesc.VS = CD3DX12_SHADER_BYTECODE(VertexShader.Get());
        PSODesc.PS = CD3DX12_SHADER_BYTECODE(PixelShader.Get());
        PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        PSODesc.DepthStencilState.DepthEnable = FALSE;
        PSODesc.DepthStencilState.StencilEnable = FALSE;
        PSODesc.SampleMask = UINT_MAX;
        PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PSODesc.NumRenderTargets = 1;
        PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PSODesc.SampleDesc.Count = 1;
        D3DDevice->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PipelineState));

        struct Vertex
        {
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT4 color;
        };
        
        Vertex TriangleVertices[] =
        {
            { { 0.0f, 0.25f * 1, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * 1, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * 1, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const unsigned int VertexBufferSize = sizeof(TriangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        CD3DX12_HEAP_PROPERTIES HeapPropertied(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);
        D3DDevice->CreateCommittedResource(&HeapPropertied, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&VertexBuffer));

        // Copy the triangle data to the vertex buffer.
        UINT8* VertexDataBegin;
        CD3DX12_RANGE ReadRange(0, 0);        // We do not intend to read from this resource on the CPU.
        VertexBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&VertexDataBegin));
        memcpy(VertexDataBegin, TriangleVertices, sizeof(TriangleVertices));
        VertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
        VertexBufferView.StrideInBytes = sizeof(Vertex);
        VertexBufferView.SizeInBytes = VertexBufferSize;
    
        return ErrorCode::OK;
    }

    void PipelineInterface::CleanUp()
    {
        //  WaitForLastSubmittedFrame
        FrameContext* FrameContext = &FrameContexts[FrameIndex % FrameNumInFlight];

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

        if (LevelRenderTargetResource)
        {
            LevelRenderTargetResource->Release();
            LevelRenderTargetResource = nullptr;
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
        
        if (D3DSRVDescHeap)
        {
            D3DSRVDescHeap->Release();
            D3DSRVDescHeap = nullptr;
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
        OutInitInfo.SrvDescriptorHeap = D3DSRVDescHeap.Get();
        
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

        unsigned long FenceValue = FrameContext->FenceValue;
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

    void PipelineInterface::SetSRVDescriptorHeaps(unsigned int FrameContextIndex, unsigned NumDescriptorHeaps) const
    {
        ID3D12DescriptorHeap* RawHeap = D3DSRVDescHeap.Get();
        FrameContexts[FrameContextIndex].CommandList->SetDescriptorHeaps(NumDescriptorHeaps, &RawHeap);
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

    D3D12_GPU_DESCRIPTOR_HANDLE PipelineInterface::GetLevelRenderTargetGPUHandle() const
    {
        return LevelSRVGPUHandle;   
    }
    
    void PipelineInterface::RenderLevel(unsigned int FrameContextIndex)
    {
        D3D12_RESOURCE_BARRIER Barrier = {};
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        Barrier.Transition.pResource = RenderTarget.Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        
        FrameContexts[FrameContextIndex].CommandList->ResourceBarrier(1, &Barrier);
        FrameContexts[FrameContextIndex].CommandList->OMSetRenderTargets(1, &LevelRenderTargetDescriptorHandle, false, nullptr);

        const float ClearColor[] = { 0, 0, 0, 1.0f };
        FrameContexts[FrameContextIndex].CommandList->ClearRenderTargetView(LevelRenderTargetDescriptorHandle, ClearColor, 0, nullptr);

        CD3DX12_VIEWPORT ViewPort = CD3DX12_VIEWPORT(0.f, 0.f, 512, 512);
        CD3DX12_RECT ScissorRect = CD3DX12_RECT(0, 0, 512, 512);
        FrameContexts[FrameContextIndex].CommandList->RSSetViewports(1, &ViewPort);
        FrameContexts[FrameContextIndex].CommandList->RSSetScissorRects(1, &ScissorRect);

        FrameContexts[FrameContextIndex].CommandList->SetGraphicsRootSignature(RootSignature.Get());
        FrameContexts[FrameContextIndex].CommandList->SetPipelineState(PipelineState.Get());
        
        FrameContexts[FrameContextIndex].CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        FrameContexts[FrameContextIndex].CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
        FrameContexts[FrameContextIndex].CommandList->DrawInstanced(3, 1, 0, 0);
        
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        FrameContexts[FrameContextIndex].CommandList->ResourceBarrier(1, &Barrier);
    }
}
