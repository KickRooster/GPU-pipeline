#include "PipelineInterface.h"
#include "../misc/Math.h"
#include "../misc/FileTool.h"
#include "../actor/Camera.h"
#include "../actor/StaticMesh.h"
#include "../actor/SkyLight.h"
#include "../actor/TerrainActor.h"
#include "../asset/Mesh.h"
#include "../asset/MeshLoader.h"
#include "../asset/CubemapTexture.h"
#include "MaterialProxy.h"
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <iostream>
#include <string>
#include <d3dx12.h>

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

    // VB pass only: Camera + ClusterCount + Nanite buffers (t20-t24) + ScenePrimitives (t26)
    CD3DX12_ROOT_PARAMETER1 RootParameters[8] = {};

    // Parameter 0: Camera Constants (b0)
    RootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);

    // Parameter 1: Cluster Count Buffer (b1)
    RootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);

    // Parameters 2-6: Nanite buffers (t20-t24)
    RootParameters[2].InitAsShaderResourceView(20, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // NaniteVertices
    RootParameters[3].InitAsShaderResourceView(21, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // NaniteUniqueVertices
    RootParameters[4].InitAsShaderResourceView(22, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // NaniteLocalIndices
    RootParameters[5].InitAsShaderResourceView(23, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // NaniteClusters
    RootParameters[6].InitAsShaderResourceView(24, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_AMPLIFICATION); // NaniteGroupBounds (LOD selection in AS only)

    // Parameter 7: ScenePrimitiveBuffer (t26)
    RootParameters[7].InitAsShaderResourceView(26, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // ScenePrimitives

    D3D12_ROOT_SIGNATURE_FLAGS RootSignatureFlags =
                D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

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
    
    const ComPtr<ID3D12PipelineState> OldMeshPSO = MeshShaderPipelineState;
    const ComPtr<ID3D12PipelineState> OldResolvePSO = MaterialResolvePipelineState;
    const ComPtr<ID3D12PipelineState> OldTerrainPSO = TerrainPipelineState;

    MeshShaderPipelineState.Reset();
    MaterialResolvePipelineState.Reset();
    TerrainPipelineState.Reset();

    ErrorCode Result = CreateMeshShaderPipelineState();
    if (Result != ErrorCode::OK)
    {
        MeshShaderPipelineState = OldMeshPSO;
        MaterialResolvePipelineState = OldResolvePSO;
        TerrainPipelineState = OldTerrainPSO;
        return Result;
    }
    
    Result = CreateMaterialResolveComputePipelineState();
    if (Result != ErrorCode::OK)
    {
        MeshShaderPipelineState = OldMeshPSO;
        MaterialResolvePipelineState = OldResolvePSO;
        TerrainPipelineState = OldTerrainPSO;
        return Result;
    }

    Result = CreateTerrainPipelineState();
    if (Result != ErrorCode::OK)
    {
        MeshShaderPipelineState = OldMeshPSO;
        MaterialResolvePipelineState = OldResolvePSO;
        TerrainPipelineState = OldTerrainPSO;
        return Result;
    }

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::SetFillMode(D3D12_FILL_MODE FillMode)
{
    if (FillMode == CurrentFillMode)
    {
        return ErrorCode::OK;
    }
    CurrentFillMode = FillMode;

    return RecompileShaders();
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

    Result = CompileShaderDXC(FileTool::GetInstance().GetPixelShaderPath(), L"main", L"ps_6_5", PixelShader);
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
    PSODesc.RasterizerState.FillMode = CurrentFillMode;
    PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    
    PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState.DepthEnable = TRUE;
    PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    PSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    PSODesc.DepthStencilState.StencilEnable = FALSE;
    
    PSODesc.SampleMask = UINT_MAX;
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets = 1;
    PSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_UINT;
    PSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    PSODesc.SampleDesc.Count = 1;

    CD3DX12_PIPELINE_MESH_STATE_STREAM PipelineStream(PSODesc);

    D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc;
    StreamDesc.pPipelineStateSubobjectStream = &PipelineStream;
    StreamDesc.SizeInBytes = sizeof(PipelineStream);

    HRESULT hResult = D3DDevice->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(&MeshShaderPipelineState));
    if (FAILED(hResult))
    {
        char Buffer[256];
        sprintf_s(Buffer, "Failed to create mesh shader pipeline state. HRESULT: 0x%08X\n", hResult);
        OutputDebugStringA(Buffer);
        return ErrorCode::MeshShaderPipelineStateCreateFailed;
    }

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CreateMaterialResolveComputePipelineState()
{
    ComPtr<IDxcBlob> ComputeShader;
    ErrorCode Result = CompileShaderDXC(FileTool::GetInstance().GetMaterialResolveCSPath(), L"main", L"cs_6_0", ComputeShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }
    
    // Root signature for Material Resolve compute shader
    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(D3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    CD3DX12_ROOT_PARAMETER1 RootParameters[8] = {};

    // 0: Camera CBV (b0)
    RootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    // 1: SkyLight CBV (b1)
    RootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);

    // 2: Bindless textures (t30+ space0)
    CD3DX12_DESCRIPTOR_RANGE1 TextureRange = {};
    TextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    TextureRange.NumDescriptors = UINT_MAX;
    TextureRange.BaseShaderRegister = 30;
    TextureRange.RegisterSpace = 0;
    TextureRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    TextureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    RootParameters[2].InitAsDescriptorTable(1, &TextureRange, D3D12_SHADER_VISIBILITY_ALL);

    // 3: Bindless cubemaps (t0+ space1)
    CD3DX12_DESCRIPTOR_RANGE1 CubemapRange = {};
    CubemapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    CubemapRange.NumDescriptors = UINT_MAX;
    CubemapRange.BaseShaderRegister = 0;
    CubemapRange.RegisterSpace = 1;
    CubemapRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    CubemapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    RootParameters[3].InitAsDescriptorTable(1, &CubemapRange, D3D12_SHADER_VISIBILITY_ALL);

    // 4: VB SRV (t0 space2)
    CD3DX12_DESCRIPTOR_RANGE1 VBRange = {};
    VBRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    VBRange.NumDescriptors = 1;
    VBRange.BaseShaderRegister = 0;
    VBRange.RegisterSpace = 2;
    VBRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    VBRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    RootParameters[4].InitAsDescriptorTable(1, &VBRange, D3D12_SHADER_VISIBILITY_ALL);

    // 5: Output UAV (u0)
    CD3DX12_DESCRIPTOR_RANGE1 UAVRange = {};
    UAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    UAVRange.NumDescriptors = 1;
    UAVRange.BaseShaderRegister = 0;
    UAVRange.RegisterSpace = 0;
    UAVRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    UAVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    RootParameters[5].InitAsDescriptorTable(1, &UAVRange, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_DESCRIPTOR_RANGE1 BufferRange = {};
    BufferRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    BufferRange.NumDescriptors = 9;
    BufferRange.BaseShaderRegister = 20;
    BufferRange.RegisterSpace = 0;
    BufferRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    BufferRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    RootParameters[6].InitAsDescriptorTable(1, &BufferRange, D3D12_SHADER_VISIBILITY_ALL);

    // 7: Terrain resolve CBV (b2)
    RootParameters[7].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);

    // Static sampler
    CD3DX12_STATIC_SAMPLER_DESC StaticSamplers[1] = {};
    StaticSamplers[0].Filter = D3D12_FILTER_ANISOTROPIC;
    StaticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    StaticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    StaticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    StaticSamplers[0].MipLODBias = 0.0f;
    StaticSamplers[0].MaxAnisotropy = 16;
    StaticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    StaticSamplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    StaticSamplers[0].MinLOD = 0.0f;
    StaticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    StaticSamplers[0].ShaderRegister = 0;
    StaticSamplers[0].RegisterSpace = 0;
    StaticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc;
    RootSignatureDesc.Init_1_1(_countof(RootParameters), RootParameters, _countof(StaticSamplers), StaticSamplers, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    HRESULT hResult = D3DX12SerializeVersionedRootSignature(&RootSignatureDesc, FeatureData.HighestVersion, &Signature, &Error);
    if (FAILED(hResult))
    {
        return ErrorCode::SerializeVersionedRootSignatureFailed;
    }
    
    hResult = D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&MaterialResolveRootSignature));
    if (FAILED(hResult))
    {
        return ErrorCode::RootSignatureCreationFailed;
    }
    
    // Create compute PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.pRootSignature = MaterialResolveRootSignature.Get();
    PSODesc.CS = CD3DX12_SHADER_BYTECODE(ComputeShader->GetBufferPointer(), ComputeShader->GetBufferSize());

    hResult = D3DDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&MaterialResolvePipelineState));
    if (FAILED(hResult))
    {
        return ErrorCode::ComputePipelineStateCreateFailed;
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

    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVHeapDesc.NumDescriptors = BackBufferCount + FrameNumInFlight * 2;
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    RTVHeapDesc.NodeMask = 0;
    if (D3DDevice->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&D3DRTVDescHeap)) != S_OK)
    {
        return ErrorCode::DescriptorHeapCreateFailed;
    }

    SIZE_T RTVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = D3DRTVDescHeap->GetCPUDescriptorHandleForHeapStart();
    for (int I = 0; I < BackBufferCount; ++I)
    {
        IMGUIRenderTargetDescriptorHandles.push_back(RTVHandle);
        RTVHandle.ptr += RTVDescriptorSize;
    }

    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        FrameContexts[I].RenderTargetCPUDescriptorHandle = RTVHandle;
        RTVHandle.ptr += RTVDescriptorSize;
    }

    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        FrameContexts[I].VisibilityBufferRTVHandle = RTVHandle;
        RTVHandle.ptr += RTVDescriptorSize;
    }

    D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc = {};
    DSVHeapDesc.NumDescriptors = FrameNumInFlight;
    DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DSVHeapDesc.NodeMask = 0;

    if (D3DDevice->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(&D3DDSDescHeap)) != S_OK)
    {
        return ErrorCode::DescriptorHeapCreateFailed;
    }

    SIZE_T DSVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle = D3DDSDescHeap->GetCPUDescriptorHandleForHeapStart();

    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        FrameContexts[I].DepthStencilCPUDescriptorHandle = DSVHandle;
        DSVHandle.ptr += DSVDescriptorSize;
    }

    D3D12_DESCRIPTOR_HEAP_DESC SRVHeapDesc = {};
    SRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    SRVHeapDesc.NumDescriptors = SRVHeapSize;
    SRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (D3DDevice->CreateDescriptorHeap(&SRVHeapDesc, IID_PPV_ARGS(&D3DSRVCBVDescHeap)) != S_OK)
    {
        return ErrorCode::DescriptorHeapCreateFailed;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = D3DSRVCBVDescHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = D3DSRVCBVDescHeap->GetGPUDescriptorHandleForHeapStart();
    unsigned int IncrementSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

        FrameContexts[I].VisibilityBufferSRVCPUHandle = CPUHandle;
        FrameContexts[I].VisibilityBufferSRVGPUHandle = GPUHandle;
        CPUHandle.ptr += IncrementSize;
        GPUHandle.ptr += IncrementSize;

        FrameContexts[I].RenderTargetUAVCPUHandle = CPUHandle;
        FrameContexts[I].RenderTargetUAVGPUHandle = GPUHandle;
        CPUHandle.ptr += IncrementSize;
        GPUHandle.ptr += IncrementSize;
    }

    D3DSRVDescriptorHeapAllocator.Create(D3DDevice.Get(), D3DSRVCBVDescHeap.Get(), FrameNumInFlight * 4);
    D3DSRVDescriptorHeapAllocator.AllocRange(MaterialResolveBufferDescCount, &MaterialResolveBufferTableCPU, &MaterialResolveBufferTableGPU);

    const unsigned int ResolveBufferSize = MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(TerrainResolveConstants));
    CD3DX12_HEAP_PROPERTIES ResolveUploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC ResolveDesc = CD3DX12_RESOURCE_DESC::Buffer(ResolveBufferSize);
    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &ResolveUploadHeapProps, D3D12_HEAP_FLAG_NONE, &ResolveDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&TerrainResolveBuffer[I]));
        
        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
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
        ComPtr<ID3D12Resource> BackBuffer;
        SwapChain->GetBuffer(I, IID_PPV_ARGS(BackBuffer.GetAddressOf()));
        D3DDevice->CreateRenderTargetView(BackBuffer.Get(), nullptr, IMGUIRenderTargetDescriptorHandles[I]);
        IMGUIRenderTargetResources.push_back(std::move(BackBuffer));
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

    Result = CreateMaterialResolveComputePipelineState();
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CreateTerrainPipelineState();
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
    //  Wait for ALL in-flight frames
    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        FrameContext* FC = &FrameContexts[I];
        UINT64 FV = FC->FenceValue;
        if (FV != 0)
        {
            FC->FenceValue = 0;
            if (Fence->GetCompletedValue() < FV)
            {
                Fence->SetEventOnCompletion(FV, FenceEvent);
                WaitForSingleObject(FenceEvent, INFINITE);
            }
        }
    }

    //  CleanupRenderTarget
    IMGUIRenderTargetResources.clear();
    IMGUIRenderTargetDescriptorHandles.clear();

    // Reset allocators
    TextureAllocator.Reset();
    CubemapAllocator.Reset();

    // Cleanup ClusterCountBuffer
    if (ClusterCountBuffer && ClusterCountBufferMapped)
    {
        ClusterCountBuffer->Unmap(0, nullptr);
        ClusterCountBufferMapped = nullptr;
    }
    ClusterCountBuffer.Reset();

    // Cleanup GPU Scene buffers

    //  Do clean
    if (SwapChain)
    {
        SwapChain->SetFullscreenState(false, nullptr);
    }
    SwapChain.Reset();

    if (SwapChainWaitableObject != nullptr)
    {
        CloseHandle(SwapChainWaitableObject);
        SwapChainWaitableObject = nullptr;
    }

    FrameContexts.clear();

    UploadCommandAllocator.Reset();
    UploadCommandList.Reset();
    CommandList.Reset();
    D3DCommandQueue.Reset();
    UploadQueue.Reset();
    UploadFence.Reset();

    if (UploadFenceEvent)
    {
        CloseHandle(UploadFenceEvent);
        UploadFenceEvent = nullptr;
    }

    D3DSRVDescriptorHeapAllocator.Destroy();
    D3DRTVDescHeap.Reset();
    D3DDSDescHeap.Reset();
    D3DSRVCBVDescHeap.Reset();

    Fence.Reset();

    if (FenceEvent)
    {
        CloseHandle(FenceEvent);
        FenceEvent = nullptr;
    }

    MeshShaderRootSignature.Reset();
    MeshShaderPipelineState.Reset();
    ComputeShaderPipelineState.Reset();
    ComputeShaderRootSignature.Reset();
    MaterialResolveRootSignature.Reset();
    MaterialResolvePipelineState.Reset();

    GlobalVertexBuffer.Reset();
    GlobalVertexBufferUpload.Reset();
    GlobalUniqueVerticesBuffer.Reset();
    GlobalUniqueVerticesBufferUpload.Reset();
    GlobalLocalIndicesBuffer.Reset();
    GlobalLocalIndicesBufferUpload.Reset();
    GlobalClusterBuffer.Reset();
    GlobalClusterBufferUpload.Reset();
    GlobalGroupBoundsBuffer.Reset();
    GlobalGroupBoundsBufferUpload.Reset();
    GlobalTriangleMaterialIDsBuffer.Reset();
    GlobalTriangleMaterialIDsBufferUpload.Reset();
    GlobalMaterialTableBuffer.Reset();
    GlobalMaterialTableBufferUpload.Reset();
    ScenePrimitiveBuffer.Reset();
    ScenePrimitiveBufferUpload.Reset();

#ifdef DX12_ENABLE_DEBUG_LAYER
    {
        // Disable WARNING break before releasing device, so the debug layer
        // doesn't trigger a breakpoint for the device's own destruction warnings
        ID3D12InfoQueue* InfoQueue = nullptr;
        if (SUCCEEDED(D3DDevice->QueryInterface(IID_PPV_ARGS(&InfoQueue))))
        {
            InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
            InfoQueue->Release();
        }

        // Get debug interface BEFORE releasing device
        IDXGIDebug1* Debug = nullptr;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&Debug));

        D3DDevice.Reset();

        // Report AFTER device is released — only truly leaked objects remain
        if (Debug)
        {
            Debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
            Debug->Release();
        }
    }
#else
    D3DDevice.Reset();
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
        SwapChain->GetBuffer(I, IID_PPV_ARGS(IMGUIRenderTargetResources[I].ReleaseAndGetAddressOf()));
        D3DDevice->CreateRenderTargetView(IMGUIRenderTargetResources[I].Get(), nullptr, IMGUIRenderTargetDescriptorHandles[I]);
    }
}

void PipelineInterface::CleanupIMGUIRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (int I = 0; I < BackBufferCount; ++I)
    {
        IMGUIRenderTargetResources[I].Reset();
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

void PipelineInterface::ReleaseGlobalUploadBuffers()
{
    GlobalVertexBufferUpload.Reset();
    GlobalUniqueVerticesBufferUpload.Reset();
    GlobalLocalIndicesBufferUpload.Reset();
    GlobalClusterBufferUpload.Reset();
    GlobalGroupBoundsBufferUpload.Reset();
    GlobalTriangleMaterialIDsBufferUpload.Reset();
    GlobalMaterialTableBufferUpload.Reset();
}

ErrorCode PipelineInterface::CreateGlobalMergedMeshBuffers(const Level* LevelInstance)
{
    const auto& StaticMeshes = LevelInstance->GetStaticMeshes();
    if (StaticMeshes.empty())
    {
        return ErrorCode::OK;
    }

    // Step 1: Calculate total sizes across all meshes
    unsigned int TotalVertexCount = 0;
    unsigned int TotalUniqueVerticesCount = 0;
    unsigned int TotalLocalIndicesBytes = 0;
    unsigned int TotalClusterCount = 0;
    unsigned int TotalGroupBoundsCount = 0;
    unsigned int TotalTriangleMaterialIDsCount = 0;
    
    for (size_t MeshIdx = 0; MeshIdx < StaticMeshes.size(); ++MeshIdx)
    {
        const StaticMesh* MeshInstance = StaticMeshes[MeshIdx].get();
        const NaniteData* Data = MeshInstance->GetNaniteData();
        if (!Data)
        {
            continue;
        }
        
        TotalVertexCount += static_cast<unsigned int>(StaticMeshes[MeshIdx]->GetVertices().size());

        // Count from cluster data
        for (const ClusterData& Cluster : Data->Clusters)
        {
            TotalUniqueVerticesCount += static_cast<unsigned int>(Cluster.UniqueVertices.size());
            TotalLocalIndicesBytes += static_cast<unsigned int>(Cluster.LocalIndices.size());
            TotalTriangleMaterialIDsCount += static_cast<unsigned int>(Cluster.TriangleMaterialIDs.size());
        }

        TotalClusterCount += static_cast<unsigned int>(Data->Clusters.size());
        TotalGroupBoundsCount += static_cast<unsigned int>(Data->GroupBounds.size());
    }

    // Step 2: Create global buffers (5 Nanite buffers, indices 0-4)
    CD3DX12_HEAP_PROPERTIES DefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES UploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    // Global Vertex Buffer (index 0)
    {
        const unsigned int BufferSize = sizeof(Vertex) * TotalVertexCount;
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&GlobalVertexBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&GlobalVertexBufferUpload));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // Global UniqueVertices Buffer (index 1)
    {
        const unsigned int BufferSize = sizeof(unsigned int) * TotalUniqueVerticesCount;
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&GlobalUniqueVerticesBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&GlobalUniqueVerticesBufferUpload));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // Global LocalIndices Buffer (ByteAddressBuffer)
    {
        const unsigned int BufferSize = TotalLocalIndicesBytes;
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&GlobalLocalIndicesBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&GlobalLocalIndicesBufferUpload));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // Global Cluster Buffer
    {
        const unsigned int BufferSize = sizeof(GPUCluster) * TotalClusterCount;
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&GlobalClusterBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&GlobalClusterBufferUpload));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // Global GroupBounds Buffer
    {
        const unsigned int BufferSize = sizeof(GPUGroupBound) * TotalGroupBoundsCount;
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&GlobalGroupBoundsBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&GlobalGroupBoundsBufferUpload));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // Global TriangleMaterialIDs Buffer
    {
        const unsigned int BufferSize = sizeof(unsigned int) * TotalTriangleMaterialIDsCount;
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&GlobalTriangleMaterialIDsBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }

        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&GlobalTriangleMaterialIDsBufferUpload));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // Step 3: Fill upload buffers with merged data and copy to default heap
    // Note: UploadCommandList is already reset by caller (ResetUploadCommandList)

    // Track offsets for each mesh
    unsigned int CurrentVertexOffset = 0;
    unsigned int CurrentUniqueVerticesOffset = 0;
    unsigned int CurrentLocalIndicesOffset = 0;
    unsigned int CurrentClusterOffset = 0;
    unsigned int CurrentGroupBoundsOffset = 0;
    unsigned int CurrentTriangleMaterialIDsOffset = 0;

    // Map all upload buffers
    Vertex* VertexData = nullptr;
    unsigned int* UniqueVerticesData = nullptr;
    unsigned char* LocalIndicesData = nullptr;
    GPUCluster* GPUClusterData = nullptr;
    GPUGroupBound* GroupBoundsData = nullptr;
    unsigned int* TriangleMaterialIDsData = nullptr;

    CD3DX12_RANGE ReadRange(0, 0);
    GlobalVertexBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&VertexData));
    GlobalUniqueVerticesBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&UniqueVerticesData));
    GlobalLocalIndicesBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&LocalIndicesData));
    GlobalClusterBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&GPUClusterData));
    GlobalGroupBoundsBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&GroupBoundsData));
    GlobalTriangleMaterialIDsBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&TriangleMaterialIDsData));

    // Fill data for each mesh
    for (size_t MeshIdx = 0; MeshIdx < StaticMeshes.size(); ++MeshIdx)
    {
        const StaticMesh* MeshInstance = StaticMeshes[MeshIdx].get();
        const NaniteData* Data = MeshInstance->GetNaniteData();

        // Record group bounds offset for adjusting Refined/GroupId indices
        unsigned int MeshGroupBoundsOffset = CurrentGroupBoundsOffset;

        // Record mesh vertex offset for UniqueVertices index adjustment
        unsigned int MeshVertexOffset = CurrentVertexOffset;

        // Copy vertices
        const std::vector<Vertex>& Vertices = StaticMeshes[MeshIdx]->GetVertices();
        const unsigned int VertexCount = static_cast<unsigned int>(Vertices.size());
        memcpy(VertexData + CurrentVertexOffset, Vertices.data(), VertexCount * sizeof(Vertex));
        CurrentVertexOffset += VertexCount;

        // Copy unique vertices and local indices from clusters
        unsigned int MeshUniqueVerticesOffset = CurrentUniqueVerticesOffset;
        unsigned int MeshLocalIndicesOffset = CurrentLocalIndicesOffset;

        for (const ClusterData& Cluster : Data->Clusters)
        {
            // Copy unique vertices (adjust indices to global vertex buffer)
            const unsigned int UniqueVertCount = static_cast<unsigned int>(Cluster.UniqueVertices.size());
            // Each UniqueVertex index needs to be offset by this mesh's vertex base
            for (unsigned int J = 0; J < UniqueVertCount; ++J)
            {
                UniqueVerticesData[CurrentUniqueVerticesOffset + J] = Cluster.UniqueVertices[J] + MeshVertexOffset;
            }
            CurrentUniqueVerticesOffset += UniqueVertCount;

            // Copy local indices
            const unsigned int LocalIdxCount = static_cast<unsigned int>(Cluster.LocalIndices.size());
            memcpy(LocalIndicesData + CurrentLocalIndicesOffset,
                   Cluster.LocalIndices.data(),
                   LocalIdxCount * sizeof(unsigned char));
            CurrentLocalIndicesOffset += LocalIdxCount;
        }

        // Copy clusters (adjust offsets to be relative to mesh start in global buffers)
        unsigned int LocalUniqueVerticesOffset = MeshUniqueVerticesOffset;
        unsigned int LocalLocalIndicesOffset = MeshLocalIndicesOffset;

        for (size_t I = 0; I < Data->Clusters.size(); ++I)
        {
            const ClusterData& Cluster = Data->Clusters[I];

            GPUClusterData[CurrentClusterOffset + I].PrimitiveId = static_cast<unsigned int>(MeshIdx);
            GPUClusterData[CurrentClusterOffset + I].IndexCount = static_cast<unsigned int>(Cluster.LocalIndices.size());
            GPUClusterData[CurrentClusterOffset + I].UniqueVerticesOffset = LocalUniqueVerticesOffset;
            GPUClusterData[CurrentClusterOffset + I].UniqueVerticesCount = static_cast<unsigned int>(Cluster.UniqueVertices.size());
            GPUClusterData[CurrentClusterOffset + I].LocalIndicesOffset = LocalLocalIndicesOffset;
            GPUClusterData[CurrentClusterOffset + I].BoundCenter[0] = Cluster.Bound.Center[0];
            GPUClusterData[CurrentClusterOffset + I].BoundCenter[1] = Cluster.Bound.Center[1];
            GPUClusterData[CurrentClusterOffset + I].BoundCenter[2] = Cluster.Bound.Center[2];
            GPUClusterData[CurrentClusterOffset + I].BoundRadius = Cluster.Bound.Radius;

            GPUClusterData[CurrentClusterOffset + I].Refined = (Cluster.Refined == -1) ? -1 : (Cluster.Refined + static_cast<int>(MeshGroupBoundsOffset));

            GPUClusterData[CurrentClusterOffset + I].GroupId = Cluster.GroupId + static_cast<int>(MeshGroupBoundsOffset);
            GPUClusterData[CurrentClusterOffset + I].TriangleMaterialIDsOffset = CurrentTriangleMaterialIDsOffset;

            // Copy per-triangle material IDs
            const unsigned int TriMatCount = static_cast<unsigned int>(Cluster.TriangleMaterialIDs.size());
            memcpy(TriangleMaterialIDsData + CurrentTriangleMaterialIDsOffset,
                   Cluster.TriangleMaterialIDs.data(),
                   TriMatCount * sizeof(unsigned int));
            CurrentTriangleMaterialIDsOffset += TriMatCount;

            LocalUniqueVerticesOffset += static_cast<unsigned int>(Cluster.UniqueVertices.size());
            LocalLocalIndicesOffset += static_cast<unsigned int>(Cluster.LocalIndices.size());
        }

        // Copy group bounds
        for (size_t I = 0; I < Data->GroupBounds.size(); ++I)
        {
            GroupBoundsData[CurrentGroupBoundsOffset + I].Center[0] = Data->GroupBounds[I].Center[0];
            GroupBoundsData[CurrentGroupBoundsOffset + I].Center[1] = Data->GroupBounds[I].Center[1];
            GroupBoundsData[CurrentGroupBoundsOffset + I].Center[2] = Data->GroupBounds[I].Center[2];
            GroupBoundsData[CurrentGroupBoundsOffset + I].Radius = Data->GroupBounds[I].Radius;
            GroupBoundsData[CurrentGroupBoundsOffset + I].Error = Data->GroupBounds[I].Error;
        }

        // Update offsets
        CurrentClusterOffset += static_cast<unsigned int>(Data->Clusters.size());
        CurrentGroupBoundsOffset += static_cast<unsigned int>(Data->GroupBounds.size());

        StaticMeshes[MeshIdx]->ClearData();
    }

    // Unmap all buffers
    GlobalVertexBufferUpload->Unmap(0, nullptr);
    GlobalUniqueVerticesBufferUpload->Unmap(0, nullptr);
    GlobalLocalIndicesBufferUpload->Unmap(0, nullptr);
    GlobalClusterBufferUpload->Unmap(0, nullptr);
    GlobalGroupBoundsBufferUpload->Unmap(0, nullptr);
    GlobalTriangleMaterialIDsBufferUpload->Unmap(0, nullptr);

    // Step 4: Transition COMMON -> COPY_DEST, copy from upload to default heap, then transition to SRV
    D3D12_RESOURCE_BARRIER PreCopyBarriers[6];
    PreCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalVertexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    PreCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalUniqueVerticesBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    PreCopyBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalLocalIndicesBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    PreCopyBarriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalClusterBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    PreCopyBarriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalGroupBoundsBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    PreCopyBarriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalTriangleMaterialIDsBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    UploadCommandList->ResourceBarrier(6, PreCopyBarriers);

    UploadCommandList->CopyBufferRegion(GlobalVertexBuffer.Get(), 0, GlobalVertexBufferUpload.Get(), 0, sizeof(Vertex) * TotalVertexCount);
    UploadCommandList->CopyBufferRegion(GlobalUniqueVerticesBuffer.Get(), 0, GlobalUniqueVerticesBufferUpload.Get(), 0, sizeof(unsigned int) * TotalUniqueVerticesCount);
    UploadCommandList->CopyBufferRegion(GlobalLocalIndicesBuffer.Get(), 0, GlobalLocalIndicesBufferUpload.Get(), 0, TotalLocalIndicesBytes);
    UploadCommandList->CopyBufferRegion(GlobalClusterBuffer.Get(), 0, GlobalClusterBufferUpload.Get(), 0, sizeof(GPUCluster) * TotalClusterCount);
    UploadCommandList->CopyBufferRegion(GlobalGroupBoundsBuffer.Get(), 0, GlobalGroupBoundsBufferUpload.Get(), 0, sizeof(GPUGroupBound) * TotalGroupBoundsCount);
    UploadCommandList->CopyBufferRegion(GlobalTriangleMaterialIDsBuffer.Get(), 0, GlobalTriangleMaterialIDsBufferUpload.Get(), 0, sizeof(unsigned int) * TotalTriangleMaterialIDsCount);

    // Transition all buffers to shader resource state
    D3D12_RESOURCE_BARRIER Barriers[6];
    Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalUniqueVerticesBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalLocalIndicesBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalClusterBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalGroupBoundsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(GlobalTriangleMaterialIDsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(6, Barriers);

    // Step 5: Create and fill ScenePrimitiveBuffer (GPU Scene - UE5 style)
    // Collect primitive scene data from each StaticMesh (transforms + bounds + material indices)
    std::vector<FPrimitiveSceneData> PrimitiveSceneDataArray;
    PrimitiveSceneDataArray.reserve(StaticMeshes.size());

    for (size_t MeshIdx = 0; MeshIdx < StaticMeshes.size(); ++MeshIdx)
    {
        StaticMesh* MeshInstance = StaticMeshes[MeshIdx].get();

        // Get scene data from mesh (transforms)
        FPrimitiveSceneData* SceneData = MeshInstance->GetSceneData();
        PrimitiveSceneDataArray.push_back(*SceneData);
    }

    // Create ScenePrimitiveBuffer
    if (!PrimitiveSceneDataArray.empty())
    {
        const unsigned int PrimitiveCount = static_cast<unsigned int>(PrimitiveSceneDataArray.size());
        const unsigned int BufferSize = sizeof(FPrimitiveSceneData) * PrimitiveCount;
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &DefaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &BufferDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&ScenePrimitiveBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&ScenePrimitiveBufferUpload));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        // Fill ScenePrimitiveBuffer
        FPrimitiveSceneData* PrimitiveDataMapped = nullptr;
        ScenePrimitiveBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&PrimitiveDataMapped));
        memcpy(PrimitiveDataMapped, PrimitiveSceneDataArray.data(), BufferSize);
        ScenePrimitiveBufferUpload->Unmap(0, nullptr);

        // Transition COMMON -> COPY_DEST, then copy to default heap
        CD3DX12_RESOURCE_BARRIER PrimitiveToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
            ScenePrimitiveBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST);
        UploadCommandList->ResourceBarrier(1, &PrimitiveToCopyDest);

        UploadCommandList->CopyBufferRegion(ScenePrimitiveBuffer.Get(), 0, ScenePrimitiveBufferUpload.Get(), 0, BufferSize);

        // Transition COPY_DEST -> SRV
        CD3DX12_RESOURCE_BARRIER PrimitiveBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            ScenePrimitiveBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        UploadCommandList->ResourceBarrier(1, &PrimitiveBarrier);

        // Note: ScenePrimitiveBuffer uses Root Descriptor (t26), no SRV descriptor needed
    }

    // Step 7: Create and fill MaterialTableBuffer (global material table for multi-material)
    {
        const std::vector<MaterialProxy>& MaterialProxies = LevelInstance->GetAllMaterialProxies();
        const unsigned int MaterialCount = static_cast<unsigned int>(MaterialProxies.size());

        if (MaterialCount > 0)
        {
            const unsigned int BufferSize = sizeof(GPUMaterial) * MaterialCount;
            CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

            HRESULT hResult = D3DDevice->CreateCommittedResource(
                &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
                D3D12_RESOURCE_STATE_COMMON, nullptr,
                IID_PPV_ARGS(&GlobalMaterialTableBuffer));

            if (FAILED(hResult))
            {
                return ErrorCode::CommittedResourceCreateFailed;
            }

            hResult = D3DDevice->CreateCommittedResource(
                &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&GlobalMaterialTableBufferUpload));

            if (FAILED(hResult))
            {
                return ErrorCode::CommittedResourceCreateFailed;
            }

            // Fill material table
            GPUMaterial* MaterialData = nullptr;
            GlobalMaterialTableBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&MaterialData));

            for (unsigned int I = 0; I < MaterialCount; ++I)
            {
                MaterialData[I].AlbedoTextureIndex = MaterialProxies[I].AlbedoTextureIndex;
                MaterialData[I].NormalTextureIndex = MaterialProxies[I].NormalTextureIndex;
                MaterialData[I].MetallicTextureIndex = MaterialProxies[I].MetallicTextureIndex;
                MaterialData[I].RoughnessTextureIndex = MaterialProxies[I].RoughnessTextureIndex;
            }

            GlobalMaterialTableBufferUpload->Unmap(0, nullptr);

            // Transition COMMON -> COPY_DEST
            CD3DX12_RESOURCE_BARRIER MaterialToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
                GlobalMaterialTableBuffer.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_DEST);
            UploadCommandList->ResourceBarrier(1, &MaterialToCopyDest);

            UploadCommandList->CopyBufferRegion(GlobalMaterialTableBuffer.Get(), 0, GlobalMaterialTableBufferUpload.Get(), 0, BufferSize);

            // Transition COPY_DEST -> SRV
            CD3DX12_RESOURCE_BARRIER MaterialBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                GlobalMaterialTableBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            UploadCommandList->ResourceBarrier(1, &MaterialBarrier);
        }
    }

    // Create ClusterCountBuffer (persistently mapped upload buffer for GPU-Driven rendering)
    {
        const unsigned int BufferSize = 256;  // CBV requires 256-byte alignment
        CD3DX12_HEAP_PROPERTIES UploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &BufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&ClusterCountBuffer));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        // Persistently map the buffer
        CD3DX12_RANGE ReadRange(0, 0);
        hResult = ClusterCountBuffer->Map(0, &ReadRange, &ClusterCountBufferMapped);
        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
        
        // Initialize to 0
        *static_cast<unsigned int*>(ClusterCountBufferMapped) = 0;
    }

    // Create SRV descriptors for material resolve buffer table (slots 0-7, Nanite + ScenePrimitive + Material)
    // Slot 4 (t25, TerrainPatches) and slot 8 (t29, NeighborLod) are filled by CreateTerrainResources
    {
        const unsigned int DescriptorIncrement = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = MaterialResolveBufferTableCPU;

        for (unsigned int S = 0; S < MaterialResolveBufferDescCount; ++S)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC NullDesc = {};
            NullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            NullDesc.Format = DXGI_FORMAT_R32_UINT;
            NullDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            D3D12_CPU_DESCRIPTOR_HANDLE Handle = CPUHandle;
            Handle.ptr += S * DescriptorIncrement;
            D3DDevice->CreateShaderResourceView(nullptr, &NullDesc, Handle);
        }

        auto CreateStructuredSRV = [&](ID3D12Resource* Buffer, unsigned int Stride, unsigned int Count, unsigned int Slot)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC Desc = {};
            Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            Desc.Format = DXGI_FORMAT_UNKNOWN;
            Desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            Desc.Buffer.NumElements = Count;
            Desc.Buffer.StructureByteStride = Stride;
            D3D12_CPU_DESCRIPTOR_HANDLE Handle = CPUHandle;
            Handle.ptr += Slot * DescriptorIncrement;
            D3DDevice->CreateShaderResourceView(Buffer, &Desc, Handle);
        };

        CreateStructuredSRV(GlobalVertexBuffer.Get(), sizeof(Vertex), TotalVertexCount, 0);
        CreateStructuredSRV(GlobalUniqueVerticesBuffer.Get(), sizeof(unsigned int), TotalUniqueVerticesCount, 1);
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC Desc = {};
            Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            Desc.Format = DXGI_FORMAT_R32_TYPELESS;
            Desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            Desc.Buffer.NumElements = static_cast<unsigned int>(TotalLocalIndicesBytes / 4);
            Desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            D3D12_CPU_DESCRIPTOR_HANDLE Handle = CPUHandle;
            Handle.ptr += 2 * DescriptorIncrement;
            D3DDevice->CreateShaderResourceView(GlobalLocalIndicesBuffer.Get(), &Desc, Handle);
        }
        CreateStructuredSRV(GlobalClusterBuffer.Get(), sizeof(GPUCluster), TotalClusterCount, 3);
        if (ScenePrimitiveBuffer)
        {
            CreateStructuredSRV(ScenePrimitiveBuffer.Get(), sizeof(FPrimitiveSceneData), static_cast<unsigned int>(PrimitiveSceneDataArray.size()), 5);
        }
        CreateStructuredSRV(GlobalTriangleMaterialIDsBuffer.Get(), sizeof(unsigned int), TotalTriangleMaterialIDsCount, 6);
        if (GlobalMaterialTableBuffer)
        {
            CreateStructuredSRV(GlobalMaterialTableBuffer.Get(), sizeof(GPUMaterial),
                static_cast<unsigned int>(LevelInstance->GetAllMaterialProxies().size()), 7);
        }
    }

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::UpdateScenePrimitiveBuffer(const Level* LevelInstance)
{
    const auto& StaticMeshes = LevelInstance->GetStaticMeshes();
    if (StaticMeshes.empty() || !ScenePrimitiveBuffer)
    {
        return ErrorCode::OK;
    }

    // Collect updated primitive transform data
    std::vector<FPrimitiveSceneData> PrimitiveSceneDataArray;
    PrimitiveSceneDataArray.reserve(StaticMeshes.size());

    for (size_t MeshIdx = 0; MeshIdx < StaticMeshes.size(); ++MeshIdx)
    {
        StaticMesh* MeshInstance = StaticMeshes[MeshIdx].get();

        // Get scene data from mesh (transforms)
        FPrimitiveSceneData* SceneData = MeshInstance->GetSceneData();
        PrimitiveSceneDataArray.push_back(*SceneData);
    }

    // Update ScenePrimitiveBuffer
    const unsigned int PrimitiveCount = static_cast<unsigned int>(PrimitiveSceneDataArray.size());
    const unsigned int BufferSize = sizeof(FPrimitiveSceneData) * PrimitiveCount;

    // Map upload buffer and copy data
    FPrimitiveSceneData* PrimitiveDataMapped = nullptr;
    CD3DX12_RANGE ReadRange(0, 0);
    HRESULT hResult = ScenePrimitiveBufferUpload->Map(0, &ReadRange, reinterpret_cast<void**>(&PrimitiveDataMapped));

    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    memcpy(PrimitiveDataMapped, PrimitiveSceneDataArray.data(), BufferSize);
    ScenePrimitiveBufferUpload->Unmap(0, nullptr);

    // Transition from shader resource to copy dest
    CD3DX12_RESOURCE_BARRIER BarrierToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
        ScenePrimitiveBuffer.Get(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST);
    CommandList->ResourceBarrier(1, &BarrierToCopyDest);

    // Copy to default heap
    CommandList->CopyBufferRegion(ScenePrimitiveBuffer.Get(), 0, ScenePrimitiveBufferUpload.Get(), 0, BufferSize);

    // Transition back to shader resource (both pixel and non-pixel)
    CD3DX12_RESOURCE_BARRIER BarrierToShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(
        ScenePrimitiveBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    CommandList->ResourceBarrier(1, &BarrierToShaderResource);

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CreateTexture(const Texture* TextureInstance, unsigned int DescriptorIndex, TextureProxy* TextureProxyInstance, bool ImmediateExecute)
{
    if (!TextureInstance || TextureInstance->Mips.empty() || !TextureProxyInstance)
    {
        return ErrorCode::InvalidedTextureData;
    }

    const int MipCount = TextureInstance->GetMipCount();

    if (ImmediateExecute)
    {
        UploadCommandAllocator->Reset();
        UploadCommandList->Reset(UploadCommandAllocator.Get(), nullptr);
    }

    D3D12_RESOURCE_DESC TextureDesc = {};
    TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    TextureDesc.Alignment = 0;
    TextureDesc.Width = TextureInstance->GetWidth();
    TextureDesc.Height = TextureInstance->GetHeight();
    TextureDesc.DepthOrArraySize = 1;
    TextureDesc.MipLevels = static_cast<UINT16>(MipCount);
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

    // Query footprints for all mip levels
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> PlacedFootprints(MipCount);
    std::vector<unsigned int> MipNumRows(MipCount);
    std::vector<unsigned long long> MipRowSizeInBytes(MipCount);
    unsigned long long TextureUploadBufferSize = 0;

    D3DDevice->GetCopyableFootprints(&TextureDesc, 0, MipCount, 0,
        PlacedFootprints.data(), MipNumRows.data(), MipRowSizeInBytes.data(), &TextureUploadBufferSize);

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

    // Copy all mip levels into the upload buffer
    void* MappedData = nullptr;
    hResult = TextureProxyInstance->UploadBuffer->Map(0, nullptr, &MappedData);
    if (SUCCEEDED(hResult))
    {
        const int BytesPerPixel = TextureInstance->Channels * (TextureInstance->IsHDR ? 4 : (TextureInstance->Is16Bit ? 2 : 1));

        for (int Mip = 0; Mip < MipCount; ++Mip)
        {
            unsigned char* pData = reinterpret_cast<unsigned char*>(MappedData) + PlacedFootprints[Mip].Offset;
            const unsigned char* SrcData = TextureInstance->Mips[Mip].Data.data();
            const unsigned int SrcRowPitch = TextureInstance->Mips[Mip].Width * BytesPerPixel;

            for (unsigned int Y = 0; Y < MipNumRows[Mip]; ++Y)
            {
                memcpy(pData + Y * PlacedFootprints[Mip].Footprint.RowPitch,
                       SrcData + Y * SrcRowPitch,
                       MipRowSizeInBytes[Mip]);
            }
        }

        TextureProxyInstance->UploadBuffer->Unmap(0, nullptr);
    }

    // Issue copy commands for all mip levels
    for (int Mip = 0; Mip < MipCount; ++Mip)
    {
        D3D12_TEXTURE_COPY_LOCATION SrcLocation = {};
        SrcLocation.pResource = TextureProxyInstance->UploadBuffer.Get();
        SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        SrcLocation.PlacedFootprint = PlacedFootprints[Mip];

        D3D12_TEXTURE_COPY_LOCATION DstLocation = {};
        DstLocation.pResource = TextureProxyInstance->Resource.Get();
        DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.SubresourceIndex = Mip;

        UploadCommandList->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);
    }

    CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        TextureProxyInstance->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    UploadCommandList->ResourceBarrier(1, &Barrier);

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = TextureInstance->Format;
    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = static_cast<UINT>(MipCount);
    SRVDesc.Texture2D.PlaneSlice = 0;
    SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle = D3DSRVCBVDescHeap->GetCPUDescriptorHandleForHeapStart();
    SRVHandle.ptr += (BindlessTextureStartIndex + DescriptorIndex) * D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3DDevice->CreateShaderResourceView(TextureProxyInstance->Resource.Get(), &SRVDesc, SRVHandle);

    TextureProxyInstance->Format = TextureInstance->Format;
    TextureProxyInstance->DescriptorIndex = DescriptorIndex;
    TextureProxyInstance->Width = TextureInstance->GetWidth();
    TextureProxyInstance->Height = TextureInstance->GetHeight();

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

        TextureProxyInstance->UploadBuffer.Reset();
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

        for (const auto& Mapping : PhysicalToLogical)
        {
            int PhysicalIndex = Mapping.first;
            int LogicalIndex = Mapping.second;
            
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
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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

        CubemapProxyInstance->UploadBuffer.Reset();
    }

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CreateConstantBuffer(const Actor* ActorInstance) const
{
    unsigned int StructSize = 0;

    if (dynamic_cast<const Camera*>(ActorInstance))
    {
        StructSize = sizeof(CameraConstantBuffer);
    }
    else if (dynamic_cast<const StaticMesh*>(ActorInstance))
    {
        // StaticMesh no longer uses constant buffers - data is in GPU Scene
        return ErrorCode::OK;
    }
    else if (dynamic_cast<const SkyLight*>(ActorInstance))
    {
        StructSize = sizeof(SkyLightConstantBuffer);
    }

    const unsigned int ByteSize = MathTool::GetInstance().CalcConstantBufferByteSize(StructSize);
    ConstantBufferProxy* BufferProxy = ActorInstance->GetConstantBufferProxy();
    BufferProxy->ElementByteSize = ByteSize;

    CD3DX12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ByteSize);

    for (int FrameIndex = 0; FrameIndex < FrameNumInFlight; ++FrameIndex)
    {
        HRESULT hResult = D3DDevice->CreateCommittedResource(
                &HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &BufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&BufferProxy->UploadBuffer[FrameIndex]));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }

        BufferProxy->UploadBuffer[FrameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&BufferProxy->MappedData[FrameIndex]));
    }

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
        RenderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].RenderTarget.ReleaseAndGetAddressOf()));

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
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].TransitionTexture.ReleaseAndGetAddressOf()));

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

        // Create UAV for RenderTarget (Material Resolve writes to it)
        D3D12_UNORDERED_ACCESS_VIEW_DESC RTUAVDesc = {};
        RTUAVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        RTUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        RTUAVDesc.Texture2D.MipSlice = 0;
        D3DDevice->CreateUnorderedAccessView(FrameContexts[FrameContextIndex].RenderTarget.Get(), nullptr, &RTUAVDesc, FrameContexts[FrameContextIndex].RenderTargetUAVCPUHandle);

        // Create Visibility Buffer (R32G32B32A32_UINT: packed ID + barycentrics)
        D3D12_RESOURCE_DESC VBDesc = {};
        VBDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        VBDesc.Width = static_cast<UINT64>(NewViewportSize.x);
        VBDesc.Height = static_cast<UINT64>(NewViewportSize.y);
        VBDesc.DepthOrArraySize = 1;
        VBDesc.MipLevels = 1;
        VBDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
        VBDesc.SampleDesc.Count = 1;
        VBDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE VBClearValue = {};
        VBClearValue.Format = DXGI_FORMAT_R32G32B32A32_UINT;
        // All zeros — packed=0 is background sentinel (geometry writes packed+1)

        hResult = D3DDevice->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &VBDesc,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            &VBClearValue,
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].VisibilityBuffer.ReleaseAndGetAddressOf()));

        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }

        // VB RTV
        D3D12_RENDER_TARGET_VIEW_DESC VBRTVDesc = {};
        VBRTVDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
        VBRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        VBRTVDesc.Texture2D.MipSlice = 0;
        D3DDevice->CreateRenderTargetView(FrameContexts[FrameContextIndex].VisibilityBuffer.Get(), &VBRTVDesc, FrameContexts[FrameContextIndex].VisibilityBufferRTVHandle);

        // VB SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC VBSRVDesc = {};
        VBSRVDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
        VBSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        VBSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        VBSRVDesc.Texture2D.MipLevels = 1;
        D3DDevice->CreateShaderResourceView(FrameContexts[FrameContextIndex].VisibilityBuffer.Get(), &VBSRVDesc, FrameContexts[FrameContextIndex].VisibilityBufferSRVCPUHandle);
        
        D3D12_RESOURCE_DESC DepthStencilDesc;
        DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        DepthStencilDesc.Alignment = 0;
        DepthStencilDesc.Width = static_cast<UINT64>(NewViewportSize.x);
        DepthStencilDesc.Height = static_cast<UINT64>(NewViewportSize.y);
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
            IID_PPV_ARGS(FrameContexts[FrameContextIndex].DepthStencilBuffer.ReleaseAndGetAddressOf()));

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

void PipelineInterface::RenderVisibilityPass(unsigned int FrameContextIndex, const Level* LevelInstance) const
{
    // Transition VB to render target
    D3D12_RESOURCE_BARRIER VBBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].VisibilityBuffer.Get(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    CommandList->ResourceBarrier(1, &VBBarrier);

    // Render to VB + Depth
    CommandList->OMSetRenderTargets(1, &FrameContexts[FrameContextIndex].VisibilityBufferRTVHandle, false, &FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle);

    ID3D12DescriptorHeap* MainHeap = D3DSRVCBVDescHeap.Get();
    CommandList->SetDescriptorHeaps(1, &MainHeap);

    // Clear VB to 0 (background sentinel; geometry writes packed+1)
    float VBClearColor[4] = {};
    CommandList->ClearRenderTargetView(FrameContexts[FrameContextIndex].VisibilityBufferRTVHandle, VBClearColor, 0, nullptr);
    CommandList->ClearDepthStencilView(FrameContexts[FrameContextIndex].DepthStencilCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
    
    const CD3DX12_VIEWPORT ViewPort = CD3DX12_VIEWPORT(0.f, 0.f, ViewportSize.x, ViewportSize.y);
    const CD3DX12_RECT ScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(ViewportSize.x), static_cast<LONG>(ViewportSize.y));
    CommandList->RSSetViewports(1, &ViewPort);
    CommandList->RSSetScissorRects(1, &ScissorRect);

    CommandList->SetGraphicsRootSignature(MeshShaderRootSignature.Get());
    CommandList->SetPipelineState(MeshShaderPipelineState.Get());

    if (LevelInstance->GetCameras().size() > 0)
    {
        const Camera* CameraInstance = LevelInstance->GetCameras()[0];
        const D3D12_GPU_VIRTUAL_ADDRESS CameraConstantBufferAddress = CameraInstance->GetConstantBufferProxy()->UploadBuffer[FrameContextIndex]->GetGPUVirtualAddress();
        CommandList->SetGraphicsRootConstantBufferView(0, CameraConstantBufferAddress);
    }

    unsigned int TotalClusterCount = 0;
    for (const auto& MeshInstance : LevelInstance->GetStaticMeshes())
    {
        const NaniteData* Data = MeshInstance->GetNaniteData();
        TotalClusterCount += static_cast<unsigned int>(Data->Clusters.size());
    }

    if (ClusterCountBufferMapped)
    {
        *static_cast<unsigned int*>(ClusterCountBufferMapped) = TotalClusterCount;
    }

    CommandList->SetGraphicsRootConstantBufferView(1, ClusterCountBuffer->GetGPUVirtualAddress());
    CommandList->SetGraphicsRootShaderResourceView(2, GlobalVertexBuffer->GetGPUVirtualAddress());
    CommandList->SetGraphicsRootShaderResourceView(3, GlobalUniqueVerticesBuffer->GetGPUVirtualAddress());
    CommandList->SetGraphicsRootShaderResourceView(4, GlobalLocalIndicesBuffer->GetGPUVirtualAddress());
    CommandList->SetGraphicsRootShaderResourceView(5, GlobalClusterBuffer->GetGPUVirtualAddress());
    CommandList->SetGraphicsRootShaderResourceView(6, GlobalGroupBoundsBuffer->GetGPUVirtualAddress());

    if (ScenePrimitiveBuffer)
    {
        CommandList->SetGraphicsRootShaderResourceView(7, ScenePrimitiveBuffer->GetGPUVirtualAddress());
    }

    const unsigned int ASGroupCount = (TotalClusterCount + 32 - 1) / 32;
    CommandList->DispatchMesh(ASGroupCount, 1, 1);

    // VB stays in RENDER_TARGET state for terrain pass (transition to SRV happens after terrain)
}

void PipelineInterface::RenderMaterialResolve(unsigned int FrameContextIndex, const Level* LevelInstance) const
{
    // Transition RT to UAV for compute shader writing
    D3D12_RESOURCE_BARRIER RTBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].RenderTarget.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    CommandList->ResourceBarrier(1, &RTBarrier);

    ID3D12DescriptorHeap* Heaps[] = { D3DSRVCBVDescHeap.Get() };
    CommandList->SetDescriptorHeaps(1, Heaps);

    CommandList->SetComputeRootSignature(MaterialResolveRootSignature.Get());
    CommandList->SetPipelineState(MaterialResolvePipelineState.Get());

    // Bind Camera CBV (parameter 0)
    if (LevelInstance->GetCameras().size() > 0)
    {
        const Camera* CameraInstance = LevelInstance->GetCameras()[0];
        CommandList->SetComputeRootConstantBufferView(0, CameraInstance->GetConstantBufferProxy()->UploadBuffer[FrameContextIndex]->GetGPUVirtualAddress());
    }

    // Bind SkyLight CBV (parameter 1)
    if (LevelInstance->GetSkyLights().size() > 0)
    {
        const SkyLight* SkyLightInstance = LevelInstance->GetSkyLights()[0];
        CommandList->SetComputeRootConstantBufferView(1, SkyLightInstance->GetConstantBufferProxy()->UploadBuffer[FrameContextIndex]->GetGPUVirtualAddress());
    }

    const unsigned int MainHeapDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Bind bindless textures (parameter 2)
    D3D12_GPU_DESCRIPTOR_HANDLE BindlessTextureHandle = D3DSRVCBVDescHeap->GetGPUDescriptorHandleForHeapStart();
    BindlessTextureHandle.ptr += BindlessTextureStartIndex * MainHeapDescriptorSize;
    CommandList->SetComputeRootDescriptorTable(2, BindlessTextureHandle);

    // Bind bindless cubemaps (parameter 3)
    D3D12_GPU_DESCRIPTOR_HANDLE BindlessCubemapHandle = D3DSRVCBVDescHeap->GetGPUDescriptorHandleForHeapStart();
    BindlessCubemapHandle.ptr += (BindlessTextureStartIndex + MaxTextureDescriptors) * MainHeapDescriptorSize;
    CommandList->SetComputeRootDescriptorTable(3, BindlessCubemapHandle);

    // Bind VB SRV (parameter 4)
    CommandList->SetComputeRootDescriptorTable(4, FrameContexts[FrameContextIndex].VisibilityBufferSRVGPUHandle);

    // Bind RT UAV (parameter 5)
    CommandList->SetComputeRootDescriptorTable(5, FrameContexts[FrameContextIndex].RenderTargetUAVGPUHandle);

    // Bind buffer SRVs descriptor table (parameter 6: t20-t23, t25-t29)
    CommandList->SetComputeRootDescriptorTable(6, MaterialResolveBufferTableGPU);

    const unsigned int NaniteClusterCount = ClusterCountBufferMapped
        ? *static_cast<unsigned int*>(ClusterCountBufferMapped) : 0;
    const TerrainActor* Terrain = LevelInstance->GetTerrain();
    TerrainResolveConstants ResolveConstants = {};
    ResolveConstants.NaniteClusterCount = NaniteClusterCount;
    ResolveConstants.HeightmapIndex = 0xFFFFFFFF;
    ResolveConstants.DebugScale = 1.0f;
    
    for (unsigned int Index = 0; Index < 4; ++Index)
    {
        ResolveConstants.SplatmapAndLayerInfo[Index] = 0xFFFFFFFF;
    }
    
    ResolveConstants.SplatmapAndLayerInfo[3] = 0;
    for (unsigned int Index = 0; Index < 12; ++Index)
    {
        ResolveConstants.AlbedoIndices[Index] = 0xFFFFFFFF;
        ResolveConstants.NormalIndices[Index] = 0xFFFFFFFF;
        ResolveConstants.RoughnessIndices[Index] = 0xFFFFFFFF;
    }
    
    if (Terrain)
    {
        const TerrainMaterialTextures& MaterialTextures = Terrain->MaterialTextures;
        ResolveConstants.HeightmapIndex = MaterialTextures.HeightmapIndex;
        ResolveConstants.WorldSize = Terrain->TerrainSize;
        ResolveConstants.HeightScale = Terrain->HeightScale;
        
        for (unsigned int Index = 0; Index < 3; ++Index)
        {
            ResolveConstants.SplatmapAndLayerInfo[Index] = MaterialTextures.SplatmapIndices[Index];
        }
        
        ResolveConstants.SplatmapAndLayerInfo[3] = MaterialTextures.LayerCount;
        for (unsigned int Index = 0; Index < 12; ++Index)
        {
            ResolveConstants.AlbedoIndices[Index] = MaterialTextures.AlbedoIndices[Index];
            ResolveConstants.NormalIndices[Index] = MaterialTextures.NormalIndices[Index];
            ResolveConstants.RoughnessIndices[Index] = MaterialTextures.RoughnessIndices[Index];
        }
        
        ResolveConstants.DebugScale = Terrain->DebugScale;
        ResolveConstants.DebugLODColors = Terrain->DebugLODColors ? 1u : 0u;
    }

    void* ResolveMapped = nullptr;
    TerrainResolveBuffer[FrameContextIndex]->Map(0, nullptr, &ResolveMapped);
    memcpy(ResolveMapped, &ResolveConstants, sizeof(TerrainResolveConstants));
    TerrainResolveBuffer[FrameContextIndex]->Unmap(0, nullptr);
    CommandList->SetComputeRootConstantBufferView(7, TerrainResolveBuffer[FrameContextIndex]->GetGPUVirtualAddress());

    // Dispatch
    D3D12_RESOURCE_DESC RTDesc = FrameContexts[FrameContextIndex].RenderTarget->GetDesc();
    const unsigned int GroupsX = (static_cast<unsigned int>(RTDesc.Width) + 7) / 8;
    const unsigned int GroupsY = (RTDesc.Height + 7) / 8;
    CommandList->Dispatch(GroupsX, GroupsY, 1);

    // Transition RT from UAV to PIXEL_SHADER_RESOURCE for tone mapping
    RTBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].RenderTarget.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    CommandList->ResourceBarrier(1, &RTBarrier);
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
    
    // Bind viewport constants (Parametser 0)
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

ErrorCode PipelineInterface::CreateTerrainPipelineState()
{
    ComPtr<IDxcBlob> AmplificationShader;
    ComPtr<IDxcBlob> MeshShader;
    ComPtr<IDxcBlob> PixelShader;

    ErrorCode Result = CompileShaderDXC(FileTool::GetInstance().GetTerrainASPath(), L"main", L"as_6_5", AmplificationShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    Result = CompileShaderDXC(FileTool::GetInstance().GetTerrainMSPath(), L"main", L"ms_6_5", MeshShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    // Terrain-specific pixel shader — outputs WorldXZ to Visibility Buffer
    Result = CompileShaderDXC(FileTool::GetInstance().GetTerrainPSPath(), L"main", L"ps_6_5", PixelShader);
    if (Result != ErrorCode::OK)
    {
        return Result;
    }

    // Terrain Root Signature:
    //   0: Terrain Camera CBV (b0)
    //   1: Terrain Params CBV (b1)
    //   2: Terrain Patches SRV (t0) - root descriptor
    //   3: Terrain Bounds SRV (t1) - root descriptor
    //   4: Active Indices SRV (t3) - root descriptor
    //   5: Heightmap SRV (t2) - descriptor table
    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(D3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 HeightmapRange = {};
    HeightmapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    HeightmapRange.NumDescriptors = 1;
    HeightmapRange.BaseShaderRegister = 2;  // t2
    HeightmapRange.RegisterSpace = 0;
    HeightmapRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
    HeightmapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    CD3DX12_ROOT_PARAMETER1 RootParameters[7] = {};
    // Camera CB: used by both AS (frustum planes) and MS (ViewProj)
    RootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    // Terrain Params CB: used by both AS (PatchCount) and MS (ClusterBase, WorldSize, etc)
    RootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    // Patch data: MS only (reads patch WorldOffset, PatchSize, LodLevel)
    RootParameters[2].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_MESH);
    // Bounds: AS only (frustum culling)
    RootParameters[3].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_AMPLIFICATION);
    // Active indices: AS only (LOD selection result)
    RootParameters[4].InitAsShaderResourceView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_AMPLIFICATION);
    // Heightmap: MS only (vertex height sampling)
    RootParameters[5].InitAsDescriptorTable(1, &HeightmapRange, D3D12_SHADER_VISIBILITY_MESH);
    // Neighbor LOD buffer: MS only (edge stitching, indexed by active patch index)
    RootParameters[6].InitAsShaderResourceView(4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_MESH);

    // Static sampler for heightmap (linear clamp)
    D3D12_STATIC_SAMPLER_DESC HeightmapSampler = {};
    HeightmapSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    HeightmapSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    HeightmapSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    HeightmapSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    HeightmapSampler.MipLODBias = 0.0f;
    HeightmapSampler.MaxAnisotropy = 1;
    HeightmapSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    HeightmapSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    HeightmapSampler.MinLOD = 0.0f;
    HeightmapSampler.MaxLOD = D3D12_FLOAT32_MAX;
    HeightmapSampler.ShaderRegister = 0;  // s0
    HeightmapSampler.RegisterSpace = 0;
    HeightmapSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_FLAGS RootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc;
    RootSignatureDesc.Init_1_1(_countof(RootParameters), RootParameters, 1, &HeightmapSampler, RootSignatureFlags);

    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    HRESULT hResult = D3DX12SerializeVersionedRootSignature(&RootSignatureDesc, FeatureData.HighestVersion, &Signature, &Error);
    if (FAILED(hResult))
    {
        return ErrorCode::SerializeVersionedRootSignatureFailed;
    }

    hResult = D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(),
        IID_PPV_ARGS(&TerrainRootSignature));
    if (FAILED(hResult))
    {
        return ErrorCode::RootSignatureCreationFailed;
    }

    // Create terrain mesh shader PSO
    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.pRootSignature = TerrainRootSignature.Get();
    PSODesc.AS = CD3DX12_SHADER_BYTECODE(AmplificationShader->GetBufferPointer(), AmplificationShader->GetBufferSize());
    PSODesc.MS = CD3DX12_SHADER_BYTECODE(MeshShader->GetBufferPointer(), MeshShader->GetBufferSize());
    PSODesc.PS = CD3DX12_SHADER_BYTECODE(PixelShader->GetBufferPointer(), PixelShader->GetBufferSize());

    PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PSODesc.RasterizerState.FillMode = CurrentFillMode;
    PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState.DepthEnable = TRUE;
    PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    PSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    PSODesc.DepthStencilState.StencilEnable = FALSE;

    PSODesc.SampleMask = UINT_MAX;
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets = 1;
    PSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_UINT;
    PSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    PSODesc.SampleDesc.Count = 1;

    CD3DX12_PIPELINE_MESH_STATE_STREAM PipelineStream(PSODesc);
    D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc;
    StreamDesc.pPipelineStateSubobjectStream = &PipelineStream;
    StreamDesc.SizeInBytes = sizeof(PipelineStream);

    hResult = D3DDevice->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(&TerrainPipelineState));
    if (FAILED(hResult))
    {
        char Buffer[256];
        sprintf_s(Buffer, "Failed to create terrain pipeline state. HRESULT: 0x%08X\n", hResult);
        OutputDebugStringA(Buffer);
        return ErrorCode::MeshShaderPipelineStateCreateFailed;
    }

    return ErrorCode::OK;
}

ErrorCode PipelineInterface::CreateTerrainResources(TerrainActor* Terrain)
{
    if (!Terrain || Terrain->PatchCount == 0)
    {
        return ErrorCode::OK;
    }

    // Load heightmap
    const Texture& HeightmapTexture = *Terrain->HeightmapTexture;

    Terrain->ProxyInstance = std::make_unique<TerrainProxy>();
    TerrainProxy* Proxy = Terrain->GetProxy();

    const unsigned int PatchCount = Terrain->PatchCount;
    const unsigned int TotalNodeCount = Terrain->TotalNodeCount;

    // Read Nanite cluster count from the already-mapped ClusterCountBuffer
    const unsigned int NaniteClusterCount = ClusterCountBufferMapped
        ? *static_cast<unsigned int*>(ClusterCountBufferMapped) : 0;

    CD3DX12_HEAP_PROPERTIES DefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES UploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    // --- Upload patch buffer from TerrainActor data ---
    const unsigned int PatchBufferSize = sizeof(TerrainPatchData) * TotalNodeCount;
    CD3DX12_RESOURCE_DESC PatchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(PatchBufferSize);

    HRESULT hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &PatchBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&Proxy->PatchBuffer));
    
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    ComPtr<ID3D12Resource> PatchUpload;
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &PatchBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&PatchUpload));
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    void* PatchMapped = nullptr;
    PatchUpload->Map(0, nullptr, &PatchMapped);
    memcpy(PatchMapped, Terrain->Patches.data(), PatchBufferSize);
    PatchUpload->Unmap(0, nullptr);

    ResetUploadCommandAllocator();
    ResetUploadCommandList();

    CD3DX12_RESOURCE_BARRIER PatchToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
        Proxy->PatchBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    UploadCommandList->ResourceBarrier(1, &PatchToCopy);
    UploadCommandList->CopyBufferRegion(Proxy->PatchBuffer.Get(), 0, PatchUpload.Get(), 0, PatchBufferSize);
    CD3DX12_RESOURCE_BARRIER PatchToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        Proxy->PatchBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &PatchToSRV);
    ExecuteAndWaitUploadCommandList();

    // --- Create triple-buffered NeighborLod upload heap buffers ---
    // Upload heap: CPU-writable, GPU-readable directly (no copy needed)
    // Indexed by patch index so both MS and material resolve can use it
    const unsigned int NeighborLodBufferSize = sizeof(unsigned int) * TotalNodeCount;
    CD3DX12_RESOURCE_DESC NeighborLodDesc = CD3DX12_RESOURCE_DESC::Buffer(NeighborLodBufferSize);

    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &NeighborLodDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&Proxy->NeighborLodUpload[I]));
        
        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // --- Upload bound buffer from TerrainActor data ---
    const unsigned int BoundBufferSize = sizeof(TerrainPatchBound) * TotalNodeCount;
    CD3DX12_RESOURCE_DESC BoundBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BoundBufferSize);

    hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BoundBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&Proxy->BoundBuffer));
    
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    ComPtr<ID3D12Resource> BoundUpload;
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BoundBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&BoundUpload));
    
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    void* BoundMapped = nullptr;
    BoundUpload->Map(0, nullptr, &BoundMapped);
    memcpy(BoundMapped, Terrain->Bounds.data(), BoundBufferSize);
    BoundUpload->Unmap(0, nullptr);

    ResetUploadCommandAllocator();
    ResetUploadCommandList();

    CD3DX12_RESOURCE_BARRIER BoundToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
        Proxy->BoundBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    UploadCommandList->ResourceBarrier(1, &BoundToCopy);
    UploadCommandList->CopyBufferRegion(Proxy->BoundBuffer.Get(), 0, BoundUpload.Get(), 0, BoundBufferSize);
    CD3DX12_RESOURCE_BARRIER BoundToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        Proxy->BoundBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &BoundToSRV);
    ExecuteAndWaitUploadCommandList();

    // --- Create terrain params CB ---
    const unsigned int ParamsBufferSize = MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(TerrainParams));
    CD3DX12_RESOURCE_DESC ParamsDesc = CD3DX12_RESOURCE_DESC::Buffer(ParamsBufferSize);

    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &ParamsDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&TerrainParamsBuffer[I]));
        
        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    TerrainParams Params = {};
    Params.ActivePatchCount = PatchCount;
    Params.ClusterBase = NaniteClusterCount;
    Params.PatchSize = Terrain->PatchSize;
    Params.HeightScale = Terrain->HeightScale;
    Params.WorldSize = Terrain->TerrainSize;
    Params.TotalPatchCount = TotalNodeCount;
    Params.EdgeStitchingEnabled = 1u;
    Params.DebugScale = Terrain->DebugScale;
    
    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        void* ParamsMapped = nullptr;
        TerrainParamsBuffer[I]->Map(0, nullptr, &ParamsMapped);
        memcpy(ParamsMapped, &Params, sizeof(TerrainParams));
        TerrainParamsBuffer[I]->Unmap(0, nullptr);
    }

    // --- Load heightmap to GPU texture ---
    const unsigned int HeightmapWidth = static_cast<unsigned int>(HeightmapTexture.GetWidth());
    const unsigned int HeightmapHeight = static_cast<unsigned int>(HeightmapTexture.GetHeight());

    D3D12_RESOURCE_DESC TexDesc = {};
    TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    TexDesc.Width = HeightmapWidth;
    TexDesc.Height = HeightmapHeight;
    TexDesc.DepthOrArraySize = 1;
    TexDesc.MipLevels = 1;
    TexDesc.Format = HeightmapTexture.Format;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    TexDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &TexDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&Proxy->Heightmap));
    
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    const UINT64 TexUploadSize = GetRequiredIntermediateSize(Proxy->Heightmap.Get(), 0, 1);

    CD3DX12_RESOURCE_DESC TexUploadDesc = CD3DX12_RESOURCE_DESC::Buffer(TexUploadSize);
    ComPtr<ID3D12Resource> HeightmapUpload;
    hResult = D3DDevice->CreateCommittedResource(
        &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &TexUploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&HeightmapUpload));
    
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    ResetUploadCommandAllocator();
    ResetUploadCommandList();

    const unsigned int BytesPerPixel = HeightmapTexture.Channels * (HeightmapTexture.IsHDR ? 4 : (HeightmapTexture.Is16Bit ? 2 : 1));
    D3D12_SUBRESOURCE_DATA SubresourceData = {};
    SubresourceData.pData = HeightmapTexture.GetData();
    SubresourceData.RowPitch = HeightmapWidth * BytesPerPixel;
    SubresourceData.SlicePitch = SubresourceData.RowPitch * HeightmapHeight;

    UpdateSubresources(UploadCommandList.Get(), Proxy->Heightmap.Get(), HeightmapUpload.Get(), 0, 0, 1, &SubresourceData);

    CD3DX12_RESOURCE_BARRIER HeightmapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        Proxy->Heightmap.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &HeightmapBarrier);

    ExecuteAndWaitUploadCommandList();
    HeightmapUpload.Reset();

    // Create SRV for heightmap
    D3D12_CPU_DESCRIPTOR_HANDLE HeightmapSRVCPU = {};
    D3DSRVDescriptorHeapAllocator.Alloc(&HeightmapSRVCPU, &TerrainHeightmapSRVGPU);
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.Format = HeightmapTexture.Format;
    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;
    D3DDevice->CreateShaderResourceView(Proxy->Heightmap.Get(), &SRVDesc, HeightmapSRVCPU);

    // --- Create ActiveIndices buffer (triple buffered for per-frame updates) ---
    const unsigned int MaxActiveIndices = PatchCount; // Worst case: all patches active
    const unsigned int IndicesBufferSize = sizeof(unsigned int) * MaxActiveIndices;
    CD3DX12_RESOURCE_DESC IndicesBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(IndicesBufferSize);

    // Default heap buffer
    hResult = D3DDevice->CreateCommittedResource(
        &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &IndicesBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&Proxy->ActiveIndicesBuffer));
    
    if (FAILED(hResult))
    {
        return ErrorCode::CommittedResourceCreateFailed;
    }

    ResetUploadCommandAllocator();
    ResetUploadCommandList();
    CD3DX12_RESOURCE_BARRIER InitBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        Proxy->ActiveIndicesBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    UploadCommandList->ResourceBarrier(1, &InitBarrier);
    ExecuteAndWaitUploadCommandList();

    // Upload heap buffers (triple buffered)
    for (int I = 0; I < FrameNumInFlight; ++I)
    {
        hResult = D3DDevice->CreateCommittedResource(
            &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &IndicesBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&Proxy->ActiveIndicesUpload[I]));
        
        if (FAILED(hResult))
        {
            return ErrorCode::CommittedResourceCreateFailed;
        }
    }

    // CPU-side heightmap data no longer needed after GPU upload
    Terrain->HeightmapTexture.reset();

    // Fill material resolve descriptor table slots for terrain buffers
    {
        const unsigned int DescriptorIncrement = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = MaterialResolveBufferTableCPU;

        {
            D3D12_SHADER_RESOURCE_VIEW_DESC Desc = {};
            Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            Desc.Format = DXGI_FORMAT_UNKNOWN;
            Desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            Desc.Buffer.NumElements = TotalNodeCount;
            Desc.Buffer.StructureByteStride = sizeof(TerrainPatchData);
            D3D12_CPU_DESCRIPTOR_HANDLE Handle = CPUHandle;
            Handle.ptr += 4 * DescriptorIncrement;
            D3DDevice->CreateShaderResourceView(Proxy->PatchBuffer.Get(), &Desc, Handle);
        }
        
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC Desc = {};
            Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            Desc.Format = DXGI_FORMAT_UNKNOWN;
            Desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            Desc.Buffer.NumElements = TotalNodeCount;
            Desc.Buffer.StructureByteStride = sizeof(unsigned int);
            D3D12_CPU_DESCRIPTOR_HANDLE Handle = CPUHandle;
            Handle.ptr += 8 * DescriptorIncrement;
            D3DDevice->CreateShaderResourceView(Proxy->NeighborLodUpload[0].Get(), &Desc, Handle);
        }
    }

    return ErrorCode::OK;
}

void PipelineInterface::RenderTerrainVisibilityPass(unsigned int FrameContextIndex, const Level* LevelInstance) const
{
    TerrainActor* Terrain = LevelInstance->GetTerrain();
    const TerrainProxy* Proxy = Terrain ? Terrain->GetProxy() : nullptr;

    // Render terrain patches if available
    if (Proxy && !Terrain->ActivePatchIndices.empty() && Proxy->PatchBuffer && TerrainPipelineState && Proxy->BoundBuffer)
    {
        const unsigned int ActivePatchCount = static_cast<unsigned int>(Terrain->ActivePatchIndices.size());

        // Update TerrainParams CB with actual active patch count
        void* ParamsMapped = nullptr;
        TerrainParamsBuffer[FrameContextIndex]->Map(0, nullptr, &ParamsMapped);
        TerrainParams Params = {};
        Params.ActivePatchCount = ActivePatchCount;
        Params.ClusterBase = *static_cast<unsigned int*>(ClusterCountBufferMapped);
        Params.PatchSize = Terrain->PatchSize;
        Params.HeightScale = Terrain->HeightScale;
        Params.WorldSize = Terrain->TerrainSize;
        Params.TotalPatchCount = Terrain->TotalNodeCount;
        Params.EdgeStitchingEnabled = Terrain->EdgeStitchingEnabled ? 1u : 0u;
        Params.DebugScale = Terrain->DebugScale;
        memcpy(ParamsMapped, &Params, sizeof(TerrainParams));
        TerrainParamsBuffer[FrameContextIndex]->Unmap(0, nullptr);

        // Upload active indices to GPU only when changed
        if (Terrain->ActivePatchIndicesDirty)
        {
            void* MappedData = nullptr;
            Proxy->ActiveIndicesUpload[FrameContextIndex]->Map(0, nullptr, &MappedData);
            memcpy(MappedData, Terrain->ActivePatchIndices.data(), ActivePatchCount * sizeof(unsigned int));
            Proxy->ActiveIndicesUpload[FrameContextIndex]->Unmap(0, nullptr);

            CD3DX12_RESOURCE_BARRIER IndicesToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
                Proxy->ActiveIndicesBuffer.Get(),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_COPY_DEST);
            CommandList->ResourceBarrier(1, &IndicesToCopy);
            CommandList->CopyBufferRegion(Proxy->ActiveIndicesBuffer.Get(), 0,
                Proxy->ActiveIndicesUpload[FrameContextIndex].Get(), 0, ActivePatchCount * sizeof(unsigned int));
            CD3DX12_RESOURCE_BARRIER IndicesToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
                Proxy->ActiveIndicesBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            CommandList->ResourceBarrier(1, &IndicesToSRV);

            Terrain->ActivePatchIndicesDirty = false;
        }

        void* MappedData = nullptr;
        Proxy->NeighborLodUpload[FrameContextIndex]->Map(0, nullptr, &MappedData);
        unsigned int* DstData = static_cast<unsigned int*>(MappedData);
        memset(DstData, 0xFF, sizeof(unsigned int) * Terrain->TotalNodeCount);
        
        for (unsigned int I = 0; I < ActivePatchCount; ++I)
        {
            const unsigned int PatchIdx = Terrain->ActivePatchIndices[I];
            DstData[PatchIdx] = Terrain->Patches[PatchIdx].NeighborLodPacked;
        }
        
        Proxy->NeighborLodUpload[FrameContextIndex]->Unmap(0, nullptr);

        const unsigned int DescriptorIncrement = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_SHADER_RESOURCE_VIEW_DESC NLDesc = {};
        NLDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        NLDesc.Format = DXGI_FORMAT_UNKNOWN;
        NLDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        NLDesc.Buffer.NumElements = Terrain->TotalNodeCount;
        NLDesc.Buffer.StructureByteStride = sizeof(unsigned int);
        D3D12_CPU_DESCRIPTOR_HANDLE NLHandle = MaterialResolveBufferTableCPU;
        NLHandle.ptr += 8 * DescriptorIncrement;
        D3DDevice->CreateShaderResourceView(Proxy->NeighborLodUpload[FrameContextIndex].Get(), &NLDesc, NLHandle);

        CommandList->SetGraphicsRootSignature(TerrainRootSignature.Get());
        CommandList->SetPipelineState(TerrainPipelineState.Get());

        // Parameter 0: Camera CBV (shared with Nanite — same Camera::Update data)
        if (LevelInstance->GetCameras().size() > 0)
        {
            const Camera* CameraInstance = LevelInstance->GetCameras()[0];
            const D3D12_GPU_VIRTUAL_ADDRESS CameraCBAddress = CameraInstance->GetConstantBufferProxy()->UploadBuffer[FrameContextIndex]->GetGPUVirtualAddress();
            CommandList->SetGraphicsRootConstantBufferView(0, CameraCBAddress);
        }

        // Parameter 1: Terrain Params CBV
        CommandList->SetGraphicsRootConstantBufferView(1, TerrainParamsBuffer[FrameContextIndex]->GetGPUVirtualAddress());

        // Parameter 2: Terrain Patches SRV (t0)
        CommandList->SetGraphicsRootShaderResourceView(2, Proxy->PatchBuffer->GetGPUVirtualAddress());

        // Parameter 3: Terrain Bounds SRV (t1)
        CommandList->SetGraphicsRootShaderResourceView(3, Proxy->BoundBuffer->GetGPUVirtualAddress());

        // Parameter 4: Active Indices SRV (t3)
        CommandList->SetGraphicsRootShaderResourceView(4, Proxy->ActiveIndicesBuffer->GetGPUVirtualAddress());

        // Parameter 5: Heightmap SRV descriptor table (t2)
        CommandList->SetGraphicsRootDescriptorTable(5, TerrainHeightmapSRVGPU);

        // Parameter 6: Neighbor LOD SRV (t4) — upload heap, GPU reads directly
        CommandList->SetGraphicsRootShaderResourceView(6, Proxy->NeighborLodUpload[FrameContextIndex]->GetGPUVirtualAddress());

        // Dispatch: use ActivePatchIndices count
        // AS group size is 64, so divide by 64
        const unsigned int ASGroupCount = (ActivePatchCount + 63) / 64;
        CommandList->DispatchMesh(ASGroupCount, 1, 1);
    }

    // Transition VB to SRV for material resolve (after all VB writes are done)
    D3D12_RESOURCE_BARRIER VBBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        FrameContexts[FrameContextIndex].VisibilityBuffer.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    CommandList->ResourceBarrier(1, &VBBarrier);
}
