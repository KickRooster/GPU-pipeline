#include "PipelineInterface.h"
#include "../misc/Math.h"
#include "../misc/FileTool.h"
#include "../actor/Camera.h"
#include "../actor/StaticMesh.h"
#include "../asset/Mesh.h"
#include "../asset/MeshLoader.h"
#include "../asset/CubemapTexture.h"
#include "MaterialProxy.h"

#include <d3dcompiler.h>
#include <dxcapi.h>
#include <iostream>
#include <string>
#include <cmath>

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

ErrorCode BindlessAllocator::Initialize(ID3D12DescriptorHeap* ExistingHeap, unsigned int NumDescriptors, unsigned int StartOffset)
{
    ExternalHeap = ExistingHeap;
    MaxDescriptors = NumDescriptors;
    HeapStartOffset = StartOffset;
    NextDescriptorIndex = 0;
    
    Microsoft::WRL::ComPtr<ID3D12Device> Device;
    ExistingHeap->GetDevice(IID_PPV_ARGS(&Device));
    D3D12_DESCRIPTOR_HEAP_DESC Desc = ExistingHeap->GetDesc();
    DescriptorSize = Device->GetDescriptorHandleIncrementSize(Desc.Type);
    
    return ErrorCode::OK;
}

unsigned int BindlessAllocator::AllocateRange(unsigned int Count)
{
    if (NextDescriptorIndex + Count > MaxDescriptors)
    {
        return InvalidDescriptorIndex;
    }
    
    const unsigned int StartIndex = NextDescriptorIndex;
    NextDescriptorIndex += Count;
    
    return StartIndex;
}

void BindlessAllocator::Reset()
{
    NextDescriptorIndex = 0;
}

ID3D12DescriptorHeap* BindlessAllocator::GetHeap() const
{
    return ExternalHeap ? ExternalHeap : DescriptorHeap.Get();
}

unsigned int BindlessAllocator::GetDescriptorSize() const
{
    return DescriptorSize;
}

ErrorCode PipelineInterface::CreateRootSignature()
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(D3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    // 3个CBV + 4级LOD×5个SRV + 1个bindless纹理描述符表 + 1个bindless cubemap描述符表 = 25个参数
    CD3DX12_ROOT_PARAMETER1 RootParameters[25] = {};
    
    // Parameter 0: Camera Constants (b0)
    RootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    
    // Parameter 1: Actor Constants (b1)
    RootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    
    // Parameter 2: SkyLight Constants (b2)
    RootParameters[2].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    
    // Parameter 3: Bindless纹理描述符表 (t20, space0) - 用于访问所有纹理
    CD3DX12_DESCRIPTOR_RANGE1 TextureRange = {};
    TextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    TextureRange.NumDescriptors = UINT_MAX; // Bindless - 无限制数量
    TextureRange.BaseShaderRegister = 20;   // 从t20开始
    TextureRange.RegisterSpace = 0;
    TextureRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    TextureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    RootParameters[3].InitAsDescriptorTable(1, &TextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
    
    // Parameter 4: Bindless Cubemap描述符表 (t0, space1) - 用于访问所有cubemap
    CD3DX12_DESCRIPTOR_RANGE1 CubemapRange = {};
    CubemapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    CubemapRange.NumDescriptors = UINT_MAX; // Bindless - 无限制数量
    CubemapRange.BaseShaderRegister = 0;    // 从t0开始
    CubemapRange.RegisterSpace = 1;         // 使用space1避免冲突
    CubemapRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    CubemapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    RootParameters[4].InitAsDescriptorTable(1, &CubemapRange, D3D12_SHADER_VISIBILITY_PIXEL);
    
    // LOD 0 资源 (参数 5-9, 寄存器 t0-t4)
    RootParameters[5].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 Vertices
    RootParameters[6].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 Meshlets
    RootParameters[7].InitAsShaderResourceView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 UniqueVertexIndices
    RootParameters[8].InitAsShaderResourceView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 MeshletTriangles
    RootParameters[9].InitAsShaderResourceView(4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);   // LOD0 MeshletBounds
    
    // LOD 1 资源 (参数 10-14, 寄存器 t5-t9)
    RootParameters[10].InitAsShaderResourceView(5, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);    // LOD1 Vertices
    RootParameters[11].InitAsShaderResourceView(6, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);    // LOD1 Meshlets
    RootParameters[12].InitAsShaderResourceView(7, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);    // LOD1 UniqueVertexIndices
    RootParameters[13].InitAsShaderResourceView(8, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);  // LOD1 MeshletTriangles
    RootParameters[14].InitAsShaderResourceView(9, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);  // LOD1 MeshletBounds
    
    // LOD 2 资源 (参数 15-19, 寄存器 t10-t14)
    RootParameters[15].InitAsShaderResourceView(10, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 Vertices
    RootParameters[16].InitAsShaderResourceView(11, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 Meshlets
    RootParameters[17].InitAsShaderResourceView(12, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 UniqueVertexIndices
    RootParameters[18].InitAsShaderResourceView(13, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 MeshletTriangles
    RootParameters[19].InitAsShaderResourceView(14, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD2 MeshletBounds
    
    // LOD 3 资源 (参数 20-24, 寄存器 t15-t19)
    RootParameters[20].InitAsShaderResourceView(15, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 Vertices
    RootParameters[21].InitAsShaderResourceView(16, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 Meshlets
    RootParameters[22].InitAsShaderResourceView(17, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 UniqueVertexIndices
    RootParameters[23].InitAsShaderResourceView(18, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 MeshletTriangles
    RootParameters[24].InitAsShaderResourceView(19, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // LOD3 MeshletBounds
    
    // 静态采样器设置 - 用于bindless纹理采样
    CD3DX12_STATIC_SAMPLER_DESC StaticSamplers[1] = {};
    StaticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    StaticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    StaticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    StaticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    StaticSamplers[0].MipLODBias = 0.0f;
    StaticSamplers[0].MaxAnisotropy = 16;
    StaticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    StaticSamplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    StaticSamplers[0].MinLOD = 0.0f;
    StaticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    StaticSamplers[0].ShaderRegister = 0;  // s0
    StaticSamplers[0].RegisterSpace = 0;
    StaticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_FLAGS RootSignatureFlags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc;
    RootSignatureDesc.Init_1_1(_countof(RootParameters), RootParameters, _countof(StaticSamplers), StaticSamplers, RootSignatureFlags);

    // 序列化并创建根签名
    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    
    HRESULT hResult = D3DX12SerializeVersionedRootSignature(&RootSignatureDesc, FeatureData.HighestVersion, &Signature, &Error);
    if (FAILED(hResult))
    {
        return ErrorCode::SerializeVersionedRootSignatureFailed;
    }
    
    hResult = D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), 
                                      IID_PPV_ARGS(&MeshShaderRootSignature));
    
    if (FAILED(hResult))
    {
        return ErrorCode::RootSignatureCreationFailed;
    }
    
    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CompileShaderFXC(const string& ShaderPath, const string& EntryPoint, const string& TargetProfile, ComPtr<ID3DBlob>& OutShaderBlob) const
{
#ifdef DX12_ENABLE_DEBUG_LAYER
    UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT CompileFlags = 0;
#endif

    ComPtr<ID3DBlob> ErrorBlob;
    
    HRESULT hResult = D3DCompileFromFile(
        FileTool::GetInstance().StringToWString(ShaderPath).c_str(),
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

ErrorCode PipelineInterface::CompileShaderDXC(const string& ShaderPath, const wstring& EntryPoint, const wstring& TargetProfile, ComPtr<IDxcBlob>& OutShaderBlob) const
{
    ComPtr<IDxcUtils> DxcUtils;
    ComPtr<IDxcCompiler3> DxcCompiler;
    
    HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&DxcUtils));
    if (FAILED(hResult))
    {
        return ErrorCode::UtilsCreateFailed;
    }
    
    hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&DxcCompiler));
    if (FAILED(hResult))
    {
        return ErrorCode::CompilerCreateFailed;
    }
    
    ComPtr<IDxcIncludeHandler> IncludeHandler;
    hResult = DxcUtils->CreateDefaultIncludeHandler(&IncludeHandler);
    if (FAILED(hResult))
    {
        return ErrorCode::DefaultIncludeHandlerCreateFailed;
    }

    ComPtr<IDxcBlobEncoding> ShaderSource;
    hResult = DxcUtils->LoadFile(FileTool::GetInstance().StringToWString(ShaderPath.c_str()).c_str(), nullptr, &ShaderSource);
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

ErrorCode PipelineInterface::RecompileShaders()
{
    WaitForLastSubmittedFrame();
    
    const ComPtr<ID3D12PipelineState> OldPipelineState = MeshShaderPipelineState;
    
    MeshShaderPipelineState.Reset();
    
    const ErrorCode Result = CreateMeshShaderPipelineState();
    
    if (Result != ErrorCode::OK)
    {
        MeshShaderPipelineState = OldPipelineState;
        return Result;
    }
    
    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CreateMeshShaderPipelineState()
{
    ComPtr<IDxcBlob> AmplificationShader;
    ComPtr<IDxcBlob> MeshShader;
    ComPtr<IDxcBlob> PixelShader;
    
    ErrorCode Result = CompileShaderDXC(FileTool::GetInstance().GetAmplificationShaderPath(), L"main", L"as_6_5", AmplificationShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CompileShaderDXC(FileTool::GetInstance().GetMeshShaderPath(), L"main", L"ms_6_5", MeshShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CompileShaderDXC(FileTool::GetInstance().GetPixelShaderPath(), L"main", L"ps_6_0", PixelShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }
    
    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.pRootSignature = MeshShaderRootSignature.Get();
    
    PSODesc.AS = CD3DX12_SHADER_BYTECODE(AmplificationShader->GetBufferPointer(), AmplificationShader->GetBufferSize());
    PSODesc.MS = CD3DX12_SHADER_BYTECODE(MeshShader->GetBufferPointer(), MeshShader->GetBufferSize());
    PSODesc.PS = CD3DX12_SHADER_BYTECODE(PixelShader->GetBufferPointer(), PixelShader->GetBufferSize());
    
    PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    
    PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState.DepthEnable = TRUE;
    PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    PSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
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

ErrorCode PipelineInterface::CreatePostProcessComputePipelineState()
{
    ComPtr<ID3DBlob> ComputeShader;
    
    ErrorCode Result = CompileShaderFXC(FileTool::GetInstance().GetToneMappingPath(), "main", "cs_5_0", ComputeShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }
    
    // Create compute root signature
    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(D3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    CD3DX12_ROOT_PARAMETER1 RootParameters[3];
    
    // Parameter 0: Viewport constants CBV  
    RootParameters[0].InitAsConstants(6, 0, 0, D3D12_SHADER_VISIBILITY_ALL); // 6 32-bit values for 2 float2 + 2 float
    
    // Parameter 1: Input texture SRV
    CD3DX12_DESCRIPTOR_RANGE1 SRVRange = {};
    SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    SRVRange.NumDescriptors = 1;
    SRVRange.BaseShaderRegister = 0;
    SRVRange.RegisterSpace = 0;
    SRVRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    SRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    RootParameters[1].InitAsDescriptorTable(1, &SRVRange, D3D12_SHADER_VISIBILITY_ALL);
    
    // Parameter 2: Output texture UAV
    CD3DX12_DESCRIPTOR_RANGE1 UAVRange = {};
    UAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    UAVRange.NumDescriptors = 1;
    UAVRange.BaseShaderRegister = 0;
    UAVRange.RegisterSpace = 0;
    UAVRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    UAVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    RootParameters[2].InitAsDescriptorTable(1, &UAVRange, D3D12_SHADER_VISIBILITY_ALL);

    D3D12_ROOT_SIGNATURE_FLAGS RootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc;
    RootSignatureDesc.Init_1_1(_countof(RootParameters), RootParameters, 0, nullptr, RootSignatureFlags);

    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    
    HRESULT hResult = D3DX12SerializeVersionedRootSignature(&RootSignatureDesc, FeatureData.HighestVersion, &Signature, &Error);
    if (FAILED(hResult))
    {
        return ErrorCode::SerializeVersionedRootSignatureFailed;
    }
    
    hResult = D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), 
                                      IID_PPV_ARGS(&ComputeShaderRootSignature));
    if (FAILED(hResult))
    {
        return ErrorCode::RootSignatureCreationFailed;
    }

    // Create compute PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC ComputePSODesc = {};
    ComputePSODesc.pRootSignature = ComputeShaderRootSignature.Get();
    ComputePSODesc.CS = CD3DX12_SHADER_BYTECODE(ComputeShader->GetBufferPointer(), ComputeShader->GetBufferSize());

    hResult = D3DDevice->CreateComputePipelineState(&ComputePSODesc, IID_PPV_ARGS(&ComputeShaderPipelineState));
    if (FAILED(hResult))
    {
        return ErrorCode::ComputePipelineStateCreateFailed;
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
        //  First FrameNumInFlight*2 D3D12_CPU_DESCRIPTOR_HANDLE of D3DSRVDescHeap is reserved for level's render target (SRV+TransitionUAV).
        for (int I = 0; I < FrameNumInFlight; ++I)
        {
            FrameContexts[I].RenderTargetSRVCPUDescriptorHandle = CPUHandle;
            FrameContexts[I].RenderTargetSRVGPUDescriptorHandle = GPUHandle;
            CPUHandle.ptr += IncrementSize;
            GPUHandle.ptr += IncrementSize;
            
            FrameContexts[I].TransitionUAVCPUDescriptorHandle = CPUHandle;
            FrameContexts[I].TransitionUAVGPUDescriptorHandle = GPUHandle;
            CPUHandle.ptr += IncrementSize;
            GPUHandle.ptr += IncrementSize;
        }

        D3DSRVDescriptorHeapAllocator.Create(D3DDevice.Get(), D3DSRVCBVDescHeap.Get(), FrameNumInFlight * 2);
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

    Result = CreateMeshShaderPipelineState();
    if (Result != ErrorCode::OK)
    {
        return Result;
    }
    
    Result = CreatePostProcessComputePipelineState();
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = TextureAllocator.Initialize(
        D3DSRVCBVDescHeap.Get(),
        MaxTextureDescriptors,
        BindlessTextureStartIndex
    );

    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CubemapAllocator.Initialize(
        D3DSRVCBVDescHeap.Get(),
        MaxCubemapDescriptors,
        BindlessTextureStartIndex + MaxTextureDescriptors
    );

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

    // Reset allocators
    TextureAllocator.Reset();
    CubemapAllocator.Reset();

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

    if (MeshShaderRootSignature)
    {
        MeshShaderRootSignature->Release();
        MeshShaderRootSignature = nullptr;
    }

    if (MeshShaderPipelineState)
    {
        MeshShaderPipelineState->Release();
        MeshShaderPipelineState = nullptr;
    }
    
    if (ComputeShaderPipelineState)
    {
        ComputeShaderPipelineState->Release();
        ComputeShaderPipelineState = nullptr;
    }
    
    if (ComputeShaderRootSignature)
    {
        ComputeShaderRootSignature->Release();
        ComputeShaderRootSignature = nullptr;
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

    const unsigned long long FenceValue = FrameContext->FenceValue;
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

BindlessAllocator& PipelineInterface::GetTextureBindlessAllocator()
{
    return TextureAllocator;
}

BindlessAllocator& PipelineInterface::GetCubemapBindlessAllocator()
{
    return CubemapAllocator;    
}

void PipelineInterface::UpdateFrameContextFenceValue(unsigned FrameContextIndex, unsigned long FenceValue)
{
    FrameContexts[FrameContextIndex].FenceValue = FenceValue;
}

D3D12_GPU_DESCRIPTOR_HANDLE PipelineInterface::GetRenderTargetSRVGPUHandle(unsigned int FrameContextIndex) const
{
    return FrameContexts[FrameContextIndex].RenderTargetSRVGPUDescriptorHandle;
}

void PipelineInterface::ResetUploadCommandAllocator() const
{
    UploadCommandAllocator->Reset();
}

void PipelineInterface::ResetUploadCommandList() const
{
    UploadCommandList->Reset(UploadCommandAllocator.Get(), nullptr);
}

void PipelineInterface::ExecuteAndWaitUploadCommandList()
{
    UploadCommandList->Close();
    
    ID3D12CommandList* UploadCommandLists[] = { UploadCommandList.Get() };
    UploadQueue->ExecuteCommandLists(_countof(UploadCommandLists), UploadCommandLists);

    UploadFenceValue++;
    UploadQueue->Signal(UploadFence.Get(), UploadFenceValue);
    
    if (UploadFence->GetCompletedValue() < UploadFenceValue)
    {
        UploadFence->SetEventOnCompletion(UploadFenceValue, UploadFenceEvent);
        WaitForSingleObject(UploadFenceEvent, INFINITE);
    }
    
    // Reset allocator after execution completes
    UploadCommandAllocator->Reset();
}

ErrorCode PipelineInterface::CreateMeshletDataProxyBuffer(const vector<Vertex>& Vertices, const MeshletData* MeshletDataInstance, MeshletDataProxy* MeshletDataProxyInstance, bool ImmediateExecute)
{
    if (ImmediateExecute)
    {
        UploadCommandAllocator->Reset();
        UploadCommandList->Reset(UploadCommandAllocator.Get(), nullptr);
    }
    
    const unsigned int VertexBufferSize = sizeof(Vertex) * static_cast<unsigned int>(Vertices.size());
    const unsigned int MeshletsBufferSize = sizeof(Meshlet) * static_cast<unsigned int>(MeshletDataInstance->Meshlets.size());
    const unsigned int MeshletVerticesBufferSize = sizeof(unsigned int) * static_cast<unsigned int>(MeshletDataInstance->MeshletVertices.size());
    const unsigned int MeshletTrianglesBufferSize = sizeof(unsigned int) * static_cast<unsigned int>(MeshletDataInstance->MeshletIndices.size());
    const unsigned int MeshletBoundsBufferSize = sizeof(meshopt_Bounds) * static_cast<unsigned int>(MeshletDataInstance->MeshletBounds.size());
    
    CD3DX12_HEAP_PROPERTIES DefaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    
    CD3DX12_RESOURCE_DESC VertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);
    HRESULT hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &VertexBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->VertexBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    CD3DX12_RESOURCE_DESC MeshletsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletsBufferSize);
    hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletsBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletsBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    CD3DX12_RESOURCE_DESC MeshletVerticesBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletVerticesBufferSize);
    hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletVerticesBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletVerticesBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    CD3DX12_RESOURCE_DESC MeshletTrianglesBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletTrianglesBufferSize);
    hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletTrianglesBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletTrianglesBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    CD3DX12_RESOURCE_DESC MeshletBoundsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletBoundsBufferSize);
    hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletBoundsBufferDesc, 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletBoundsBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    CD3DX12_HEAP_PROPERTIES UploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &VertexBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->VertexBufferUpload));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletsBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletsBufferUpload));
    
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletVerticesBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletVerticesBufferUpload));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletTrianglesBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletTrianglesBufferUpload));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProperties, 
        D3D12_HEAP_FLAG_NONE, 
        &MeshletBoundsBufferDesc, 
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&MeshletDataProxyInstance->MeshletBoundsBufferUpload));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }
    
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
    
    if (ImmediateExecute)
    {
        UploadCommandList->Close();
        
        ID3D12CommandList* UploadCommandLists[] = { UploadCommandList.Get() };
        UploadQueue->ExecuteCommandLists(_countof(UploadCommandLists), UploadCommandLists);

        UploadFenceValue++;
        UploadQueue->Signal(UploadFence.Get(), UploadFenceValue);
        
        if (UploadFence->GetCompletedValue() < UploadFenceValue)
        {
            UploadFence->SetEventOnCompletion(UploadFenceValue, UploadFenceEvent);
            WaitForSingleObject(UploadFenceEvent, INFINITE);
        }
    }
}

ErrorCode PipelineInterface::CreateTexture(const Texture* TextureInstance, unsigned int DescriptorIndex, TextureProxy* TextureProxyInstance, bool ImmediateExecute)
{
    if (!TextureInstance || !TextureInstance->Data || !TextureProxyInstance)
    {
        return ErrorCode::InvalidedTextureData;
    }

    if (ImmediateExecute)
    {
        UploadCommandAllocator->Reset();
        UploadCommandList->Reset(UploadCommandAllocator.Get(), nullptr);
    }

    D3D12_RESOURCE_DESC TextureDesc = {};
    TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    TextureDesc.Alignment = 0;
    TextureDesc.Width = TextureInstance->Width;
    TextureDesc.Height = TextureInstance->Height;
    TextureDesc.DepthOrArraySize = 1;
    TextureDesc.MipLevels = 1;
    TextureDesc.Format = TextureInstance->Format;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    TextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES DefaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &TextureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&TextureProxyInstance->Resource));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    unsigned long long TextureUploadBufferSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedFootprint = {};
    unsigned int NumRows = 0;
    unsigned long long RowSizeInBytes = 0;
    
    D3DDevice->GetCopyableFootprints(&TextureDesc, 0, 1, 0, &PlacedFootprint, &NumRows, &RowSizeInBytes, &TextureUploadBufferSize);

    CD3DX12_HEAP_PROPERTIES UploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC UploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(TextureUploadBufferSize);
    
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &UploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&TextureProxyInstance->UploadBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    void* MappedData = nullptr;
    hResult = TextureProxyInstance->UploadBuffer->Map(0, nullptr, &MappedData);
    if (SUCCEEDED(hResult))
    {
        unsigned char* pData = reinterpret_cast<unsigned char*>(MappedData);
        pData += PlacedFootprint.Offset;

        const unsigned char* SrcData = reinterpret_cast<const unsigned char*>(TextureInstance->Data);
        const unsigned int SrcRowPitch = TextureInstance->Width * (TextureInstance->Channels * (TextureInstance->IsHDR ? 4 : 1));

        for (unsigned int Y = 0; Y < NumRows; ++Y)
        {
            memcpy(pData + Y * PlacedFootprint.Footprint.RowPitch, 
                   SrcData + Y * SrcRowPitch,
                   RowSizeInBytes);
        }

        TextureProxyInstance->UploadBuffer->Unmap(0, nullptr);
    }

    D3D12_TEXTURE_COPY_LOCATION SrcLocation = {};
    SrcLocation.pResource = TextureProxyInstance->UploadBuffer.Get();
    SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    SrcLocation.PlacedFootprint = PlacedFootprint;

    D3D12_TEXTURE_COPY_LOCATION DstLocation = {};
    DstLocation.pResource = TextureProxyInstance->Resource.Get();
    DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    DstLocation.SubresourceIndex = 0;

    UploadCommandList->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);

    CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        TextureProxyInstance->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    
    UploadCommandList->ResourceBarrier(1, &Barrier);

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = TextureInstance->Format;
    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;
    SRVDesc.Texture2D.PlaneSlice = 0;
    SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle = D3DSRVCBVDescHeap->GetCPUDescriptorHandleForHeapStart();
    SRVHandle.ptr += (BindlessTextureStartIndex + DescriptorIndex) * D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3DDevice->CreateShaderResourceView(TextureProxyInstance->Resource.Get(), &SRVDesc, SRVHandle);

    TextureProxyInstance->Format = TextureInstance->Format;
    TextureProxyInstance->DescriptorIndex = DescriptorIndex;
    TextureProxyInstance->Width = TextureInstance->Width;
    TextureProxyInstance->Height = TextureInstance->Height;

    if (ImmediateExecute)
    {
        UploadCommandList->Close();
        ID3D12CommandList* CommandLists[] = { UploadCommandList.Get() };
        UploadQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

        UploadFenceValue++;
        UploadQueue->Signal(UploadFence.Get(), UploadFenceValue);
        
        if (UploadFence->GetCompletedValue() < UploadFenceValue)
        {
            UploadFence->SetEventOnCompletion(UploadFenceValue, UploadFenceEvent);
            WaitForSingleObject(UploadFenceEvent, INFINITE);
        }
    }

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CreateCubemap(const CubemapTexture* CubemapInstance, unsigned int DescriptorIndex, CubemapTextureProxy* CubemapProxyInstance, bool ImmediateExecute)
{
    if (!CubemapInstance || !CubemapProxyInstance)
    {
        return ErrorCode::InvalidedCubemapData;
    }

    if (ImmediateExecute)
    {
        UploadCommandAllocator->Reset();
        UploadCommandList->Reset(UploadCommandAllocator.Get(), nullptr);
    }

    D3D12_RESOURCE_DESC CubemapDesc = {};
    CubemapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    CubemapDesc.Alignment = 0;
    CubemapDesc.Width = CubemapInstance->GetSize(0);
    CubemapDesc.Height = CubemapInstance->GetSize(0);
    CubemapDesc.DepthOrArraySize = 6;  // 6 faces for cubemap
    CubemapDesc.MipLevels = CubemapInstance->GetMipLevels();
    CubemapDesc.Format = CubemapInstance->GetFormat();
    CubemapDesc.SampleDesc.Count = 1;
    CubemapDesc.SampleDesc.Quality = 0;
    CubemapDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    CubemapDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES DefaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &CubemapDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&CubemapProxyInstance->Resource));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    int SubresourceCount = CubemapInstance->GetMipLevels() * 6;
    
    // Calculate upload buffer size for all faces and mip levels
    unsigned long long UploadBufferSize = 0;
    vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> PlacedFootprints(SubresourceCount);
    vector<unsigned int> NumRows(SubresourceCount);
    vector<unsigned long long> RowSizeInBytes(SubresourceCount);
    
    D3DDevice->GetCopyableFootprints(
        &CubemapDesc,
        0,
        SubresourceCount,
        0, 
        PlacedFootprints.data(),
        NumRows.data(), 
        RowSizeInBytes.data(),
        &UploadBufferSize);
    
    // DirectX12 uses Face-first ordering for cubemap subresources
    vector<pair<int, int>> PhysicalToLogical; // (PhysicalIndex, LogicalIndex)
    
    for (int I = 0; I < SubresourceCount; ++I)
    {
        unsigned int Width = PlacedFootprints[I].Footprint.Width;
        
        // Determine MipLevel from size
        int MipLevel = -1;
        for (int Mip = 0; Mip < CubemapInstance->GetMipLevels(); ++Mip)
        {
            if (static_cast<unsigned int>(CubemapInstance->GetSize(Mip)) == Width)
            {
                MipLevel = Mip;
                break;
            }
        }
        
        // Calculate FaceIndex from DirectX12's Face-first layout  
        int FaceIndex = I / CubemapInstance->GetMipLevels();
        int LogicalIndex = MipLevel * 6 + FaceIndex;
        PhysicalToLogical.push_back({I, LogicalIndex});
    }

    CD3DX12_HEAP_PROPERTIES UploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC UploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);
    
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &UploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&CubemapProxyInstance->UploadBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    void* MappedData = nullptr;
    hResult = CubemapProxyInstance->UploadBuffer->Map(0, nullptr, &MappedData);
    if (SUCCEEDED(hResult))
    {
        unsigned char* pData = static_cast<unsigned char*>(MappedData);

        for (const auto& mapping : PhysicalToLogical)
        {
            int PhysicalIndex = mapping.first;
            int LogicalIndex = mapping.second;
            
            // Extract MipLevel and FaceIndex from LogicalIndex
            int MipLevel = LogicalIndex / 6;
            int FaceIndex = LogicalIndex % 6;
            int MipSize = CubemapInstance->GetSize(MipLevel);
            
            const void* FaceData = CubemapInstance->GetFaceData(MipLevel, static_cast<ECubeFace>(FaceIndex));
            if (!FaceData)
            {
                continue;
            }
            
            unsigned char* FaceDestData = pData + PlacedFootprints[PhysicalIndex].Offset;
            const unsigned char* FaceSrcData = static_cast<const unsigned char*>(FaceData);
            const unsigned int SrcRowPitch = MipSize * CubemapInstance->GetChannelCount() * sizeof(float);

            for (unsigned int Y = 0; Y < NumRows[PhysicalIndex]; ++Y)
            {
                size_t SrcOffset = static_cast<size_t>(Y) * SrcRowPitch;
                size_t DestOffset = static_cast<size_t>(Y) * PlacedFootprints[PhysicalIndex].Footprint.RowPitch;
                
                memcpy(FaceDestData + DestOffset, 
                       FaceSrcData + SrcOffset, 
                       RowSizeInBytes[PhysicalIndex]);
            }
        }

        CubemapProxyInstance->UploadBuffer->Unmap(0, nullptr);
    }

    // Use direct 1:1 mapping since both source and destination use Face-first ordering
    for (const auto& Mapping : PhysicalToLogical)
    {
        int PhysicalIndex = Mapping.first;
        int DestinationIndex = PhysicalIndex;
        
        D3D12_TEXTURE_COPY_LOCATION SrcLocation = {};
        SrcLocation.pResource = CubemapProxyInstance->UploadBuffer.Get();
        SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        SrcLocation.PlacedFootprint = PlacedFootprints[PhysicalIndex];

        D3D12_TEXTURE_COPY_LOCATION DstLocation = {};
        DstLocation.pResource = CubemapProxyInstance->Resource.Get();
        DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.SubresourceIndex = DestinationIndex;

        UploadCommandList->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);
    }

    CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        CubemapProxyInstance->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    
    UploadCommandList->ResourceBarrier(1, &Barrier);

    // Create SRV for cubemap
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = CubemapInstance->GetFormat();
    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.TextureCube.MostDetailedMip = 0;
    SRVDesc.TextureCube.MipLevels = CubemapInstance->GetMipLevels();
    SRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle = D3DSRVCBVDescHeap->GetCPUDescriptorHandleForHeapStart();
    SRVHandle.ptr += (BindlessTextureStartIndex + MaxTextureDescriptors + DescriptorIndex) * D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3DDevice->CreateShaderResourceView(CubemapProxyInstance->Resource.Get(), &SRVDesc, SRVHandle);

    CubemapProxyInstance->Format = CubemapInstance->GetFormat();
    CubemapProxyInstance->DescriptorIndex = DescriptorIndex;
    CubemapProxyInstance->Size = CubemapInstance->GetSize(0);
    CubemapProxyInstance->MipLevels = CubemapInstance->GetMipLevels();

    if (ImmediateExecute)
    {
        UploadCommandList->Close();
        ID3D12CommandList* CommandLists[] = { UploadCommandList.Get() };
        UploadQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

        UploadFenceValue++;
        UploadQueue->Signal(UploadFence.Get(), UploadFenceValue);

        if (UploadFence->GetCompletedValue() < UploadFenceValue)
        {
            UploadFence->SetEventOnCompletion(UploadFenceValue, UploadFenceEvent);
            WaitForSingleObject(UploadFenceEvent, INFINITE);
        }
    }

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CreateConstantBuffer(const Actor* ActorInstance) const
{
    const unsigned int ByteSize = MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(SkyLightConstantBuffer));
    ConstantBufferProxy* BufferProxy = ActorInstance->GetConstantBufferProxy();
    BufferProxy->ElementByteSize = ByteSize;

    CD3DX12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ByteSize);
    HRESULT hResult = D3DDevice->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&BufferProxy->UploadBuffer));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    BufferProxy->UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&BufferProxy->MappedData));

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::UpdateViewport(unsigned int FrameContextIndex, ImVec2 NewViewportSize)
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

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &RenderTargetDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &ClearValue,
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].RenderTarget.GetAddressOf()));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        D3DDevice->CreateRenderTargetView(FrameContexts[FrameContextIndex].RenderTarget.Get(), nullptr, FrameContexts[FrameContextIndex].RenderTargetCPUDescriptorHandle);

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2D.MipLevels = 1;
        D3DDevice->CreateShaderResourceView(FrameContexts[FrameContextIndex].RenderTarget.Get(), &SRVDesc, FrameContexts[FrameContextIndex].RenderTargetSRVCPUDescriptorHandle);
        
        // Create transition texture for post-processing (same size and format as render target)
        D3D12_RESOURCE_DESC TransitionTextureDesc = RenderTargetDesc;
        TransitionTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // Only need UAV for transition texture
        
        hResult = D3DDevice->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &TransitionTextureDesc,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            nullptr,
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].TransitionTexture.GetAddressOf()));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        // Create UAV for transition texture  
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        UAVDesc.Texture2D.MipSlice = 0;
        D3DDevice->CreateUnorderedAccessView(FrameContexts[FrameContextIndex].TransitionTexture.Get(), nullptr, &UAVDesc, FrameContexts[FrameContextIndex].TransitionUAVCPUDescriptorHandle);
        // Note: TransitionUAVGPUDescriptorHandle is already set during descriptor allocation
        
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
        DepthStencilClearValue.DepthStencil.Depth = 0;
        DepthStencilClearValue.DepthStencil.Stencil = 0;

        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        hResult = D3DDevice->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &DepthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &DepthStencilClearValue,
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].DepthStencilBuffer.GetAddressOf()));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
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

    return ErrorCode::OK;
}

void PipelineInterface::RenderLevelMeshlet(unsigned int FrameContextIndex, const Level* LevelInstance) const
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

    ID3D12DescriptorHeap* MainHeap = D3DSRVCBVDescHeap.Get();
    CommandList->SetDescriptorHeaps(1, &MainHeap);
    
    constexpr float ClearColor[] = { 0, 0, 0, 1.0f };
    CommandList->ClearRenderTargetView(FrameContexts[FrameContextIndex].RenderTargetCPUDescriptorHandle, ClearColor, 0, nullptr);
    CommandList->ClearDepthStencilView(FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
    
    const CD3DX12_VIEWPORT ViewPort = CD3DX12_VIEWPORT(0.f, 0.f, ViewportSize.x, ViewportSize.y);
    const CD3DX12_RECT ScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(ViewportSize.x), static_cast<LONG>(ViewportSize.y));
    CommandList->RSSetViewports(1, &ViewPort);
    CommandList->RSSetScissorRects(1, &ScissorRect);

    CommandList->SetGraphicsRootSignature(MeshShaderRootSignature.Get());
    CommandList->SetPipelineState(MeshShaderPipelineState.Get());
    
    const unsigned int MainHeapDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    // 绑定bindless纹理描述符表 (根参数3) - 指向主堆中的bindless区域
    D3D12_GPU_DESCRIPTOR_HANDLE BindlessTextureHandle = D3DSRVCBVDescHeap->GetGPUDescriptorHandleForHeapStart();
    BindlessTextureHandle.ptr += BindlessTextureStartIndex * MainHeapDescriptorSize;
    CommandList->SetGraphicsRootDescriptorTable(3, BindlessTextureHandle);

    // 绑定bindless cubemap描述符表 (根参数4) - 指向cubemap专用堆
    D3D12_GPU_DESCRIPTOR_HANDLE BindlessCubemapHandle = D3DSRVCBVDescHeap->GetGPUDescriptorHandleForHeapStart();
    BindlessCubemapHandle.ptr += (BindlessTextureStartIndex + MaxTextureDescriptors) * MainHeapDescriptorSize;
    CommandList->SetGraphicsRootDescriptorTable(4, BindlessCubemapHandle);

    if (LevelInstance->GetCameras().size() > 0)
    {
        const Camera* CameraInstance = LevelInstance->GetCameras()[0];
        const D3D12_GPU_VIRTUAL_ADDRESS CameraConstantBufferAddress = CameraInstance->GetConstantBufferProxy()->UploadBuffer->GetGPUVirtualAddress();
        CommandList->SetGraphicsRootConstantBufferView(0, CameraConstantBufferAddress);
    }
    
    // 绑定SkyLight常量缓冲区 (根参数2)
    if (LevelInstance->GetSkyLights().size() > 0)
    {
        const SkyLight* SkyLightInstance = LevelInstance->GetSkyLights()[0];
        const D3D12_GPU_VIRTUAL_ADDRESS SkyLightConstantBufferAddress = SkyLightInstance->GetConstantBufferProxy()->UploadBuffer->GetGPUVirtualAddress();
        CommandList->SetGraphicsRootConstantBufferView(2, SkyLightConstantBufferAddress);
    }

    for (int I = 0; I < static_cast<int>(LevelInstance->GetStaticMeshes().size()); ++I)
    {
        const StaticMesh* StaticMeshInstance = LevelInstance->GetStaticMeshes()[I];
        
        const D3D12_GPU_VIRTUAL_ADDRESS ActorConstantBufferAddress = StaticMeshInstance->GetConstantBufferProxy()->UploadBuffer->GetGPUVirtualAddress();
        CommandList->SetGraphicsRootConstantBufferView(1, ActorConstantBufferAddress);
        
        const auto& MeshletDataProxyInstances = StaticMeshInstance->GetMeshletDataProxyInstances();
        const auto& MeshletDataInstances = StaticMeshInstance->GetMeshletDataInstances();

        //  XXX:    We have hard recorded the number of LODs.
        for (int lodLevel = 0; lodLevel < MeshLODSettings::GetInstance().NumLODs; ++lodLevel)
        {
            if (MeshletDataProxyInstances.size() > lodLevel && MeshletDataProxyInstances[lodLevel])
            {
                const auto& ProxyInstance = MeshletDataProxyInstances[lodLevel];
                // XXX: - Parameter 0-2: CBVs
                //      - Parameter 3-4: Descriptor tables  
                //      - Parameter 5+: LOD SRVs (BaseParamIndex = 5)
                const int BaseParamIndex = 5 + lodLevel * 5;
                
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

void PipelineInterface::RenderPostProcessCompute(unsigned int FrameContextIndex) const
{
    // Transition RenderTarget from PIXEL_SHADER_RESOURCE to NON_PIXEL_SHADER_RESOURCE for compute shader access
    D3D12_RESOURCE_BARRIER InputBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].RenderTarget.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    CommandList->ResourceBarrier(1, &InputBarrier);
    
    // TransitionTexture is always in COPY_SOURCE state (both initial creation and end of previous frame)
    // Transition it to UNORDERED_ACCESS for compute shader writing
    D3D12_RESOURCE_BARRIER TransitionToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].TransitionTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    
    CommandList->ResourceBarrier(1, &TransitionToUAV);
    
    // Set descriptor heap for compute shader
    ID3D12DescriptorHeap* Heaps[] = { D3DSRVCBVDescHeap.Get() };
    CommandList->SetDescriptorHeaps(1, Heaps);
    
    // Set compute pipeline
    CommandList->SetComputeRootSignature(ComputeShaderRootSignature.Get());
    CommandList->SetPipelineState(ComputeShaderPipelineState.Get());
    
    // Get actual render target dimensions (not viewport size, in case of mismatch)
    D3D12_RESOURCE_DESC RTDesc = FrameContexts[FrameContextIndex].RenderTarget->GetDesc();
    const unsigned int ActualWidth = static_cast<unsigned int>(RTDesc.Width);
    const unsigned int ActualHeight = RTDesc.Height;
    
    // Bind viewport constants (Parameter 0)
    // Layout: float2 InputSize, float2 OutputSize, float Exposure, float Contrast
    const float ViewportConstants[6] = {
        static_cast<float>(ActualWidth),      // InputSize.x
        static_cast<float>(ActualHeight),     // InputSize.y
        static_cast<float>(ActualWidth),      // OutputSize.x (same as input - 1:1 mapping)
        static_cast<float>(ActualHeight),     // OutputSize.y (same as input - 1:1 mapping)
        1.0f,                                 // Exposure (default: 1.0)
        1.0f                                  // Contrast (default: 1.0)
    };
    CommandList->SetComputeRoot32BitConstants(0, 6, ViewportConstants, 0);
    
    // Bind input: scene render target SRV (Parameter 1)
    CommandList->SetComputeRootDescriptorTable(1, FrameContexts[FrameContextIndex].RenderTargetSRVGPUDescriptorHandle);
    
    // Bind output: transition texture UAV (Parameter 2) 
    CommandList->SetComputeRootDescriptorTable(2, FrameContexts[FrameContextIndex].TransitionUAVGPUDescriptorHandle);
    
    // Calculate dispatch dimensions (8x8 thread groups) based on render target size
    const unsigned int GroupsX = (ActualWidth + 7) / 8;
    const unsigned int GroupsY = (ActualHeight + 7) / 8;
    
    // Dispatch compute shader
    CommandList->Dispatch(GroupsX, GroupsY, 1);
    
    // Transition texture from UAV to copy source
    D3D12_RESOURCE_BARRIER TransitionToCopySource = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].TransitionTexture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
        
    // Transition render target from NON_PIXEL_SHADER_RESOURCE to copy dest
    D3D12_RESOURCE_BARRIER RenderTargetToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].RenderTarget.Get(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST);
    
    D3D12_RESOURCE_BARRIER PreCopyBarriers[] = { TransitionToCopySource, RenderTargetToCopyDest };
    CommandList->ResourceBarrier(2, PreCopyBarriers);
    
    // Copy processed result back to render target
    CommandList->CopyResource(FrameContexts[FrameContextIndex].RenderTarget.Get(), 
                             FrameContexts[FrameContextIndex].TransitionTexture.Get());
    
    // Transition render target back to SRV state for ImGui display
    D3D12_RESOURCE_BARRIER FinalBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].RenderTarget.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    
    CommandList->ResourceBarrier(1, &FinalBarrier);
}