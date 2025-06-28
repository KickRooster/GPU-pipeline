#pragma once
#include <DirectXMath.h>

enum class ErrorCode
{
    Illegal = -1,
    OK,
    //  DX12 error code begin.
    Failed,
    DebugInterfaceNotFound,
    SerializeVersionedRootSignatureFailed,
    RootSignatureCreationFailed,
    DxcUtilsCreateFailed,
    DxcCompilerCreateFailed,
    DxcCompileResultGetOutputFailed,
    DefaultIncludeHandlerCreateFailed,
    ShaderLoadFailed,
    GetShaderByteCodeFailed,
    ShaderCompileFailed,
    MeshShaderPipelineStateCreateFailed,
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
    DirectX::XMFLOAT4 Color;
    DirectX::XMFLOAT2 UV0;
};

struct Meshlet
{
    uint32_t VertexOffset;
    uint32_t TriangleOffset;
    uint32_t VertexCount;
    uint32_t TriangleCount;
};

struct Transform
{
    DirectX::XMFLOAT4 Rotation;
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Scale;
};