#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXMath.h>

inline constexpr int FrameNumInFlight = 2;

enum class ErrorCode : int
{
    Illegal = -1,
    OK,
    //  DX12 error code begin.
    Failed,
    DebugInterfaceNotFound,
    SerializeVersionedRootSignatureFailed,
    RootSignatureCreationFailed,
    UtilsCreateFailed,
    CompilerCreateFailed,
    DefaultIncludeHandlerCreateFailed,
    ShaderLoadFailed,
    GetShaderByteCodeFailed,
    ShaderCompileFailed,
    MeshShaderPipelineStateCreateFailed,
    ComputePipelineStateCreateFailed,
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
    CommittedResourceCreateFailed,
    InvalidedTextureData,
    InvalidedCubemapData,
    //  Assimp begin.
    MeshDataIncomplete,
    //  Texture begin.
    TextureNotExist,
    TextureLoadFailed,
    AllocateTextureMemoryFailed,
    //  Others
    ErrorCode_Num
};

struct UIState
{
    bool WDown;
    bool SDown;
    bool ADown;
    bool DDown;
    bool QDown;
    bool EDown;
    bool RDown;

    bool LeftButtonDown;
    bool RightButtonDown;

    float DeltaX;
    float DeltaY;
    float MouseWheel;

    float MoveSpeed;
    float RotateSpeed;
};

struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Tangent;
    DirectX::XMFLOAT2 UV0;
};

struct Transform
{
    DirectX::XMFLOAT4 Rotation;
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Scale;
};