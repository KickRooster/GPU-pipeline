#include "PipelineInterface.h"
#include "../misc/Math.h"
#include "../actor/CameraActor.h"
#include "../actor/StaticMeshActor.h"
#include "../mesh/Mesh.h"
#include "../mesh/MeshLoader.h"
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <iostream>
#include <string>

#ifndef DX12_ENABLE_DEBUG_LAYER
#define DX12_ENABLE_DEBUG_LAYER 1
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// 明确链接到DXC库
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;
using namespace std;

ErrorCode PipelineInterface::CreateRootSignature()
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(D3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    // 扩展根签名：支持4级LOD的所有资源传递
    // 2个CBV + 4级LOD×5个SRV = 22个参数
    CD3DX12_ROOT_PARAMETER1 RootParameters[22] = {};
    
    // Parameter 0: Camera Constants (b0)
    RootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    
    // Parameter 1: Actor Constants (b1)
    RootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    
    // LOD 0 资源 (参数 2-6, 寄存器 t0-t4)
    RootParameters[2].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 Vertices
    RootParameters[3].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 Meshlets
    RootParameters[4].InitAsShaderResourceView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 UniqueVertexIndices
    RootParameters[5].InitAsShaderResourceView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 MeshletTriangles
    RootParameters[6].InitAsShaderResourceView(4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 MeshletBounds
    
    // LOD 1 资源 (参数 7-11, 寄存器 t5-t9)
    RootParameters[7].InitAsShaderResourceView(5, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);    // LOD1 Vertices
    RootParameters[8].InitAsShaderResourceView(6, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);    // LOD1 Meshlets
    RootParameters[9].InitAsShaderResourceView(7, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);    // LOD1 UniqueVertexIndices
    RootParameters[10].InitAsShaderResourceView(8, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);  // LOD1 MeshletTriangles
    RootParameters[11].InitAsShaderResourceView(9, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);  // LOD1 MeshletBounds
    
    // LOD 2 资源 (参数 12-16, 寄存器 t10-t14)
    RootParameters[12].InitAsShaderResourceView(10, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 Vertices
    RootParameters[13].InitAsShaderResourceView(11, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 Meshlets
    RootParameters[14].InitAsShaderResourceView(12, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 UniqueVertexIndices
    RootParameters[15].InitAsShaderResourceView(13, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 MeshletTriangles
    RootParameters[16].InitAsShaderResourceView(14, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 MeshletBounds
    
    // LOD 3 资源 (参数 17-21, 寄存器 t15-t19)
    RootParameters[17].InitAsShaderResourceView(15, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 Vertices
    RootParameters[18].InitAsShaderResourceView(16, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 Meshlets
    RootParameters[19].InitAsShaderResourceView(17, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 UniqueVertexIndices
    RootParameters[20].InitAsShaderResourceView(18, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 MeshletTriangles
    RootParameters[21].InitAsShaderResourceView(19, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 MeshletBounds
    
    D3D12_ROOT_SIGNATURE_FLAGS RootSignatureFlags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc;
    RootSignatureDesc.Init_1_1(_countof(RootParameters), RootParameters, 0, nullptr, RootSignatureFlags);

    // 序列化并创建根签名
    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    
    HRESULT hResult = D3DX12SerializeVersionedRootSignature(&RootSignatureDesc, FeatureData.HighestVersion, &Signature, &Error);
    if (FAILED(hResult))
    {
        return ErrorCode::SerializeVersionedRootSignatureFailed;
    }
    
    hResult = D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), 
                                      IID_PPV_ARGS(&RootSignature));
    
    if (FAILED(hResult))
    {
        return ErrorCode::RootSignatureCreationFailed;
    }
    
    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CompileShaderFXC(const wstring& ShaderPath, const string& EntryPoint, const string& TargetProfile, ComPtr<ID3DBlob>& OutShaderBlob) const
{
#ifdef DX12_ENABLE_DEBUG_LAYER
    UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT CompileFlags = 0;
#endif

    ComPtr<ID3DBlob> ErrorBlob;
    
    HRESULT hResult = D3DCompileFromFile(
        ShaderPath.c_str(), 
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        EntryPoint.c_str(),
        TargetProfile.c_str(),
        CompileFlags,
        0,
        &OutShaderBlob,
        &ErrorBlob
    );
    
    if (FAILED(hResult))
    {
        if (ErrorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
        }
        return ErrorCode::ShaderCompileFailed;
    }
    
    if (!OutShaderBlob || !OutShaderBlob->GetBufferPointer() || OutShaderBlob->GetBufferSize() == 0)
    {
        return ErrorCode::GetShaderByteCodeFailed;
    }
    
    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CompileShaderDXC(const wstring& ShaderPath, const wstring& EntryPoint, const wstring& TargetProfile, ComPtr<IDxcBlob>& OutShaderBlob) const
{
    ComPtr<IDxcUtils> DxcUtils;
    ComPtr<IDxcCompiler3> DxcCompiler;
    
    HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&DxcUtils));
    if (FAILED(hResult))
    {
        return ErrorCode::DxcUtilsCreateFailed;
    }
    
    hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&DxcCompiler));
    if (FAILED(hResult))
    {
        return ErrorCode::DxcCompilerCreateFailed;
    }
    
    ComPtr<IDxcIncludeHandler> IncludeHandler;
    hResult = DxcUtils->CreateDefaultIncludeHandler(&IncludeHandler);
    if (FAILED(hResult))
    {
        return ErrorCode::DefaultIncludeHandlerCreateFailed;
    }

    ComPtr<IDxcBlobEncoding> ShaderSource;
    hResult = DxcUtils->LoadFile(ShaderPath.c_str(), nullptr, &ShaderSource);
    if (FAILED(hResult) || !ShaderSource)
    {
        return ErrorCode::ShaderLoadFailed;
    }
    
    vector<LPCWSTR> Arguments;
    Arguments.push_back(L"-E");
    Arguments.push_back(EntryPoint.c_str());
    Arguments.push_back(L"-T");
    Arguments.push_back(TargetProfile.c_str());
    
#ifdef DX12_ENABLE_DEBUG_LAYER
    Arguments.push_back(L"-Zi"); // With debug info
    Arguments.push_back(L"-Od"); // Disable optimization
#endif
    
    DxcBuffer SourceBuffer = {};
    SourceBuffer.Ptr = ShaderSource->GetBufferPointer();
    SourceBuffer.Size = ShaderSource->GetBufferSize();
    SourceBuffer.Encoding = DXC_CP_ACP;
    
    ComPtr<IDxcResult> CompileResult;
    hResult = DxcCompiler->Compile(
        &SourceBuffer,
        Arguments.data(),
        static_cast<UINT32>(Arguments.size()),
        IncludeHandler.Get(),
        IID_PPV_ARGS(&CompileResult)
    );
    
    HRESULT CompileStatus;
    if (SUCCEEDED(hResult) && CompileResult && SUCCEEDED(CompileResult->GetStatus(&CompileStatus)) && SUCCEEDED(CompileStatus))
    {
        hResult = CompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&OutShaderBlob), nullptr);
        
        if (SUCCEEDED(hResult) && OutShaderBlob && OutShaderBlob->GetBufferPointer() && OutShaderBlob->GetBufferSize() > 0)
        {
            return ErrorCode::OK;
        }
        else
        {
            return ErrorCode::GetShaderByteCodeFailed;
        }
    }
    else
    {
        ComPtr<IDxcBlobEncoding> ErrorBlob;
        if (CompileResult && SUCCEEDED(CompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&ErrorBlob), nullptr))
            && ErrorBlob && ErrorBlob->GetBufferSize() > 0)
        {
            OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
        }
        
        return ErrorCode::ShaderCompileFailed;
    }
}

ErrorCode PipelineInterface::CreateMeshShaderPipelinestate()
{
    ComPtr<IDxcBlob> AmplificationShader;
    ComPtr<IDxcBlob> MeshShader;
    ComPtr<IDxcBlob> PixelShader;

    wstring AmplificationShaderPath = L"D:\\GPU-pipeline\\content\\shader\\meshlet_as.hlsl";
    wstring MeshShaderPath = L"D:\\GPU-pipeline\\content\\shader\\meshlet_ms.hlsl";
    wstring PixelShaderPath = L"D:\\GPU-pipeline\\content\\shader\\meshlet_ps.hlsl";
    
    ErrorCode Result = CompileShaderDXC(AmplificationShaderPath, L"main", L"as_6_5", AmplificationShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CompileShaderDXC(MeshShaderPath, L"main", L"ms_6_5", MeshShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CompileShaderDXC(PixelShaderPath, L"main", L"ps_6_0", PixelShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }
    
    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.pRootSignature = RootSignature.Get();
    
    PSODesc.AS = CD3DX12_SHADER_BYTECODE(AmplificationShader->GetBufferPointer(), AmplificationShader->GetBufferSize());
    PSODesc.MS = CD3DX12_SHADER_BYTECODE(MeshShader->GetBufferPointer(), MeshShader->GetBufferSize());
    PSODesc.PS = CD3DX12_SHADER_BYTECODE(PixelShader->GetBufferPointer(), PixelShader->GetBufferSize());
    
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

    CD3DX12_PIPELINE_MESH_STATE_STREAM PipelineStream(PSODesc);

    D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc;
    StreamDesc.pPipelineStateSubobjectStream = &PipelineStream;
    StreamDesc.SizeInBytes = sizeof(PipelineStream);

    HRESULT hResult = D3DDevice->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(&MeshShaderPipelineState));
    if (FAILED(hResult))
    {
        char buffer[256];
        sprintf_s(buffer, "Failed to create mesh shader pipeline state. HRESULT: 0x%08X\n", hResult);
        OutputDebugStringA(buffer);
        return ErrorCode::MeshShaderPipelineStateCreateFailed;
    }

    return ErrorCode::OK;
}

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
    }
    
    if (D3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&UploadCommandAllocator)) != S_OK)
    {
        return ErrorCode::CommandAllocatorCreateFailed;
    }

    if (D3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, UploadCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&UploadCommandList)) != S_OK)
    {
        return ErrorCode::CommandListCreateFailed;
    }

    if (UploadCommandList->Close() != S_OK)
    {
        return ErrorCode::CommandListCloseFailed;
    }

    if (D3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameContexts[0].CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList)) != S_OK)
    {
        return ErrorCode::CommandListCreateFailed;
    }

    if (CommandList->Close() != S_OK)
    {
        return ErrorCode::CommandListCloseFailed;
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

        D3DSRVDescriptorHeapAllocator.Create(D3DDevice.Get(), D3DSRVCBVDescHeap.Get(), FrameNumInFlight);
    }

    D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
    CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CommandQueueDesc.NodeMask = 0;
    if (D3DDevice->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&D3DCommandQueue)) != S_OK)
    {
        return ErrorCode::Failed;
    }
    
    D3D12_COMMAND_QUEUE_DESC UploadQueueDesc = {};
    UploadQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    UploadQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
    UploadQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    UploadQueueDesc.NodeMask = 0;
    
    if (FAILED(D3DDevice->CreateCommandQueue(&UploadQueueDesc, IID_PPV_ARGS(&UploadQueue))))
    {
        return ErrorCode::Failed;
    }
    
    if (FAILED(D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&UploadFence))))
    {
        return ErrorCode::FenceCreateFailed;
    }
    
    UploadFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (UploadFenceEvent == nullptr)
    {
        return ErrorCode::FenceEventCreateFailed;
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

    // Create root signature
    ErrorCode Result = CreateRootSignature();
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CreateMeshShaderPipelinestate();
    if (Result != ErrorCode::OK)
    {
        return Result;
    }
    
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
    }
    
    if (UploadCommandAllocator)
    {
        UploadCommandAllocator->Release();
        UploadCommandAllocator = nullptr;
    }
    
    if (UploadCommandList)
    {
        UploadCommandList->Release();
        UploadCommandList = nullptr;
    }
    
    FrameContexts.clear();

    if (CommandList)
    {
        CommandList->Release();
        CommandList = nullptr;
    }

    if (D3DCommandQueue)
    {
        D3DCommandQueue->Release();
        D3DCommandQueue = nullptr;
    }
    
    if (UploadQueue)
    {
        UploadQueue->Release();
        UploadQueue = nullptr;
    }
    
    if (UploadFence)
    {
        UploadFence->Release();
        UploadFence = nullptr;
    }
    
    if (UploadFenceEvent)
    {
        CloseHandle(UploadFenceEvent);
        UploadFenceEvent = nullptr;
    }

    if (D3DRTVDescHeap)
    {
        D3DRTVDescHeap->Release();
        D3DRTVDescHeap = nullptr;
    }

    if (D3DDSDescHeap)
    {
        D3DDSDescHeap->Release();
        D3DDSDescHeap = nullptr;
    }

    if (D3DSRVCBVDescHeap)
    {
        D3DSRVCBVDescHeap->Release();
        D3DSRVCBVDescHeap = nullptr;
    }

    D3DSRVDescriptorHeapAllocator.Destroy();

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

    if (RootSignature)
    {
        RootSignature->Release();
        RootSignature = nullptr;
    }

    if (MeshShaderPipelineState)
    {
        MeshShaderPipelineState->Release();
        MeshShaderPipelineState = nullptr;
    }

    if (D3DDevice)
    {
        D3DDevice->Release();
        D3DDevice = nullptr;
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
    
void PipelineInterface::InsertIMGUIRenderTargetBarrier(D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, D3D12_RESOURCE_BARRIER_TYPE BarrierType, D3D12_RESOURCE_BARRIER_FLAGS BarrierFlag) const
{
    D3D12_RESOURCE_BARRIER Barrier;
    Barrier.Type = BarrierType;
    Barrier.Flags = BarrierFlag;
    Barrier.Transition.pResource = IMGUIRenderTargetResources[SwapChain->GetCurrentBackBufferIndex()].Get();
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    Barrier.Transition.StateBefore = StateBefore;
    Barrier.Transition.StateAfter = StateAfter;
    CommandList->ResourceBarrier(1, &Barrier);
}
    
void PipelineInterface::ClearIMGUIRenderTargetView(const float ColorRGBA[4], unsigned int NumRects, const D3D12_RECT* pRects) const
{
    CommandList->ClearRenderTargetView(IMGUIRenderTargetDescriptorHandles[SwapChain->GetCurrentBackBufferIndex()], ColorRGBA, NumRects, pRects);
}

void PipelineInterface::OMSetIMGUIRenderTargets(unsigned NumRenderTargetDescriptors, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) const
{
    CommandList->OMSetRenderTargets(NumRenderTargetDescriptors, &IMGUIRenderTargetDescriptorHandles[SwapChain->GetCurrentBackBufferIndex()], RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}

void PipelineInterface::ExecuteCommandLists() const
{
    ID3D12GraphicsCommandList* RawPointer = CommandList.Get();
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
    return CommandList->Reset(
        FrameContexts[FrameContextIndex].CommandAllocator.Get(),
        nullptr);
}

void PipelineInterface::Signal(unsigned long FenceValue) const
{
    D3DCommandQueue->Signal(Fence.Get(), FenceValue);
}
    
ID3D12GraphicsCommandList6* PipelineInterface::GetCommandList() const
{
    return CommandList.Get();
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

void PipelineInterface::CreateMeshletDataProxyBuffer(const vector<Vertex>& Vertices, const MeshletData* MeshletDataInstance, MeshletDataProxy* MeshletDataProxyInstance)
{
    const unsigned int VertexBufferSize = sizeof(Vertex) * static_cast<unsigned int>(Vertices.size());
    const unsigned int MeshletsBufferSize = sizeof(Meshlet) * static_cast<unsigned int>(MeshletDataInstance->Meshlets.size());
    const unsigned int MeshletVerticesBufferSize = sizeof(unsigned int) * static_cast<unsigned int>(MeshletDataInstance->MeshletVertices.size());
    const unsigned int MeshletTrianglesBufferSize = sizeof(unsigned int) * static_cast<unsigned int>(MeshletDataInstance->MeshletIndices.size());
    const unsigned int MeshletBoundsBufferSize = sizeof(meshopt_Bounds) * static_cast<unsigned int>(MeshletDataInstance->MeshletBounds.size());
    
    CD3DX12_HEAP_PROPERTIES DefaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    
    CD3DX12_RESOURCE_DESC VertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);
    D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &VertexBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->VertexBuffer));
    
    CD3DX12_RESOURCE_DESC MeshletsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletsBufferSize);
    D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletsBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletsBuffer));
    
    CD3DX12_RESOURCE_DESC MeshletVerticesBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletVerticesBufferSize);
    D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletVerticesBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletVerticesBuffer));
    
    CD3DX12_RESOURCE_DESC MeshletTrianglesBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletTrianglesBufferSize);
    D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletTrianglesBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletTrianglesBuffer));
    
    CD3DX12_RESOURCE_DESC MeshletBoundsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletBoundsBufferSize);
    D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletBoundsBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletBoundsBuffer));
    
    CD3DX12_HEAP_PROPERTIES UploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    
    D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &VertexBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->VertexBufferUpload));
    
    D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletsBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletsBufferUpload));
    
    D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletVerticesBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletVerticesBufferUpload));
    
    D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletTrianglesBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletTrianglesBufferUpload));
    
    D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletBoundsBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletBoundsBufferUpload));
    
    Vertex* VertexDataBegin;
    CD3DX12_RANGE VertexReadRange(0, 0);
    MeshletDataProxyInstance->VertexBufferUpload->Map(0, &VertexReadRange, reinterpret_cast<void**>(&VertexDataBegin));
    memcpy(VertexDataBegin, Vertices.data(), VertexBufferSize);
    MeshletDataProxyInstance->VertexBufferUpload->Unmap(0, nullptr);
    
    Meshlet* MeshletDataBegin;
    CD3DX12_RANGE MeshletReadRange(0, 0);
    MeshletDataProxyInstance->MeshletsBufferUpload->Map(0, &MeshletReadRange, reinterpret_cast<void**>(&MeshletDataBegin));
    
    for (size_t i = 0; i < MeshletDataInstance->Meshlets.size(); ++i)
    {
        MeshletDataBegin[i].VertexOffset = MeshletDataInstance->Meshlets[i].vertex_offset;
        MeshletDataBegin[i].TriangleOffset = MeshletDataInstance->Meshlets[i].triangle_offset;
        MeshletDataBegin[i].VertexCount = MeshletDataInstance->Meshlets[i].vertex_count;
        MeshletDataBegin[i].TriangleCount = MeshletDataInstance->Meshlets[i].triangle_count;
    }
    
    MeshletDataProxyInstance->MeshletsBufferUpload->Unmap(0, nullptr);
    
    unsigned int* MeshletVerticesDataBegin;
    CD3DX12_RANGE MeshletVerticesReadRange(0, 0);
    MeshletDataProxyInstance->MeshletVerticesBufferUpload->Map(0, &MeshletVerticesReadRange, reinterpret_cast<void**>(&MeshletVerticesDataBegin));
    memcpy(MeshletVerticesDataBegin, MeshletDataInstance->MeshletVertices.data(), MeshletVerticesBufferSize);
    MeshletDataProxyInstance->MeshletVerticesBufferUpload->Unmap(0, nullptr);
    
    unsigned int* TriangleDataBegin;
    CD3DX12_RANGE TriangleReadRange(0, 0);
    MeshletDataProxyInstance->MeshletTrianglesBufferUpload->Map(0, &TriangleReadRange, reinterpret_cast<void**>(&TriangleDataBegin));
    memcpy(TriangleDataBegin, MeshletDataInstance->MeshletIndices.data(), MeshletTrianglesBufferSize);
    MeshletDataProxyInstance->MeshletTrianglesBufferUpload->Unmap(0, nullptr);
    
    meshopt_Bounds* BoundsDataBegin;
    CD3DX12_RANGE BoundsDataReadRange(0, 0);
    MeshletDataProxyInstance->MeshletBoundsBufferUpload->Map(0, &BoundsDataReadRange, reinterpret_cast<void**>(&BoundsDataBegin));
    memcpy(BoundsDataBegin, MeshletDataInstance->MeshletBounds.data(), MeshletBoundsBufferSize);
    MeshletDataProxyInstance->MeshletBoundsBufferUpload->Unmap(0, nullptr);
    
    UploadCommandAllocator->Reset();
    UploadCommandList->Reset(UploadCommandAllocator.Get(), nullptr);
    
    UploadCommandList->CopyBufferRegion(
        MeshletDataProxyInstance->VertexBuffer.Get(), 0,
        MeshletDataProxyInstance->VertexBufferUpload.Get(), 0,
        VertexBufferSize);
    
    CD3DX12_RESOURCE_BARRIER VertexBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        MeshletDataProxyInstance->VertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &VertexBarrier);
    
    UploadCommandList->CopyBufferRegion(
        MeshletDataProxyInstance->MeshletsBuffer.Get(), 0,
        MeshletDataProxyInstance->MeshletsBufferUpload.Get(), 0,
        MeshletsBufferSize);
    
    CD3DX12_RESOURCE_BARRIER MeshletsBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        MeshletDataProxyInstance->MeshletsBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &MeshletsBarrier);
    
    UploadCommandList->CopyBufferRegion(
        MeshletDataProxyInstance->MeshletVerticesBuffer.Get(), 0,
        MeshletDataProxyInstance->MeshletVerticesBufferUpload.Get(), 0,
        MeshletVerticesBufferSize);
    
    CD3DX12_RESOURCE_BARRIER VerticesBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        MeshletDataProxyInstance->MeshletVerticesBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &VerticesBarrier);
    
    UploadCommandList->CopyBufferRegion(
        MeshletDataProxyInstance->MeshletTrianglesBuffer.Get(), 0,
        MeshletDataProxyInstance->MeshletTrianglesBufferUpload.Get(), 0,
        MeshletTrianglesBufferSize);
    
    CD3DX12_RESOURCE_BARRIER TrianglesBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        MeshletDataProxyInstance->MeshletTrianglesBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &TrianglesBarrier);
    
    UploadCommandList->CopyBufferRegion(
        MeshletDataProxyInstance->MeshletBoundsBuffer.Get(), 0,
        MeshletDataProxyInstance->MeshletBoundsBufferUpload.Get(), 0,
        MeshletBoundsBufferSize);
    
    CD3DX12_RESOURCE_BARRIER BoundsBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        MeshletDataProxyInstance->MeshletBoundsBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &BoundsBarrier);
    
    UploadCommandList->Close();
    
    ID3D12CommandList* UploadCommandLists[] = { UploadCommandList.Get() };
    UploadQueue->ExecuteCommandLists(_countof(UploadCommandLists), UploadCommandLists);
    
    UploadFenceValue++;
    UploadQueue->Signal(UploadFence.Get(), UploadFenceValue);
    
    D3DCommandQueue->Wait(UploadFence.Get(), UploadFenceValue);
}

void PipelineInterface::CreateConstantBuffer(const CameraActor* CameraActorInstance)
{
    const unsigned int ByteSize = MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(CameraConstantBuffer));
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
}

void PipelineInterface::CreateConstantBuffer(const StaticMeshActor* ActorInstance)
{
    const unsigned int ByteSize = MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(StaticMeshActorConstantBuffer));
    ConstantBufferProxy* BufferProxy = ActorInstance->GetConstantBufferProxy();
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
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].RenderTarget.GetAddressOf()));

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
        
        CommandList->ResourceBarrier(1, &BufferBarrier);

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

void PipelineInterface::RenderLevelMeshlet(unsigned int FrameContextIndex, const Level* LevelInstance)
{
    D3D12_RESOURCE_BARRIER Barrier = {};
    Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    Barrier.Transition.pResource = FrameContexts[FrameContextIndex].RenderTarget.Get();
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    CommandList->ResourceBarrier(1, &Barrier);
    CommandList->OMSetRenderTargets(1, &FrameContexts[FrameContextIndex].RenderTargetCPUDescriptorHandle, false, &FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle);

    ID3D12DescriptorHeap* RawHeap = D3DSRVCBVDescHeap.Get();
    CommandList->SetDescriptorHeaps(1, &RawHeap);
    
    const float ClearColor[] = { 0, 0, 0, 1.0f };
    CommandList->ClearRenderTargetView(FrameContexts[FrameContextIndex].RenderTargetCPUDescriptorHandle, ClearColor, 0, nullptr);

    CommandList->ClearDepthStencilView(FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    
    CD3DX12_VIEWPORT ViewPort = CD3DX12_VIEWPORT(0.f, 0.f, ViewportSize.x, ViewportSize.y);
    CD3DX12_RECT ScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(ViewportSize.x), static_cast<LONG>(ViewportSize.y));
    CommandList->RSSetViewports(1, &ViewPort);
    CommandList->RSSetScissorRects(1, &ScissorRect);

    CommandList->SetGraphicsRootSignature(RootSignature.Get());
    CommandList->SetPipelineState(MeshShaderPipelineState.Get());

    if (LevelInstance->GetCameraActors().size() > 0)
    {
        const CameraActor* CameraActorInstance = LevelInstance->GetCameraActors()[0];
        const D3D12_GPU_VIRTUAL_ADDRESS CameraConstantBufferAddress = CameraActorInstance->GetConstantBufferProxy()->UploadBuffer->GetGPUVirtualAddress();
        CommandList->SetGraphicsRootConstantBufferView(0, CameraConstantBufferAddress);
    }

    for (int I = 0; I < static_cast<int>(LevelInstance->GetStaticMeshActors().size()); ++I)
    {
        const StaticMeshActor* StaticMeshActorInstance = LevelInstance->GetStaticMeshActors()[I];
        
        const D3D12_GPU_VIRTUAL_ADDRESS ActorConstantBufferAddress = StaticMeshActorInstance->GetConstantBufferProxy()->UploadBuffer->GetGPUVirtualAddress();
        CommandList->SetGraphicsRootConstantBufferView(1, ActorConstantBufferAddress);
        
        const auto& MeshletDataProxyInstances = StaticMeshActorInstance->GetMeshletDataProxyInstances();
        const auto& MeshletDataInstances = StaticMeshActorInstance->GetMeshletDataInstances();

        //  XXX:    We have hard record the number of LODs.
        for (int lodLevel = 0; lodLevel < MeshLODSettings::GetInstance().NumLODs; ++lodLevel)
        {
            if (MeshletDataProxyInstances.size() > lodLevel && MeshletDataProxyInstances[lodLevel])
            {
                const auto& ProxyInstance = MeshletDataProxyInstances[lodLevel];
                const int BaseParamIndex = 2 + lodLevel * 5;
                
                CommandList->SetGraphicsRootShaderResourceView(BaseParamIndex + 0, ProxyInstance->VertexBuffer->GetGPUVirtualAddress());
                CommandList->SetGraphicsRootShaderResourceView(BaseParamIndex + 1, ProxyInstance->MeshletsBuffer->GetGPUVirtualAddress());
                CommandList->SetGraphicsRootShaderResourceView(BaseParamIndex + 2, ProxyInstance->MeshletVerticesBuffer->GetGPUVirtualAddress());
                CommandList->SetGraphicsRootShaderResourceView(BaseParamIndex + 3, ProxyInstance->MeshletTrianglesBuffer->GetGPUVirtualAddress());
                CommandList->SetGraphicsRootShaderResourceView(BaseParamIndex + 4, ProxyInstance->MeshletBoundsBuffer->GetGPUVirtualAddress());
            }
        }
        
        const unsigned int ASGroupCount = (static_cast<unsigned int>(MeshletDataInstances[0]->Meshlets.size()) + 32 - 1) / 32;
        CommandList->DispatchMesh(ASGroupCount, 1, 1);
    }

    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    CommandList->ResourceBarrier(1, &Barrier);
}