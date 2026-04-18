#pragma once
#include "../misc/Base.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include "../misc/DesignPatterns.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include <vector>
#include "MeshProxy.h"
#include "../level/Level.h"

struct ImGUIDescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    ImVector<int>               FreeIndices;

    void Create(ID3D12Device* Device, ID3D12DescriptorHeap* Heap, UINT StartOffset = 0)
    {
        IM_ASSERT(this->Heap == nullptr && FreeIndices.empty());
        this->Heap = Heap;
        D3D12_DESCRIPTOR_HEAP_DESC Desc = Heap->GetDesc();
        HeapType = Desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = Device->GetDescriptorHandleIncrementSize(HeapType);
            
        SIZE_T ReservedSize = StartOffset * HeapHandleIncrement;
        HeapStartCpu.ptr += ReservedSize;
        HeapStartGpu.ptr += ReservedSize;
            
        FreeIndices.reserve((int)(Desc.NumDescriptors - StartOffset));
        for (int N = Desc.NumDescriptors - StartOffset; N > 0; --N)
        {
            FreeIndices.push_back(N - 1);
        }
    }
        
    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }
    
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* OutCPUDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* OutGPUDescHandle)
    {
        IM_ASSERT(FreeIndices.Size > 0);
        unsigned long long Index = FreeIndices.back();
        FreeIndices.pop_back();
        OutCPUDescHandle->ptr = HeapStartCpu.ptr + (Index * HeapHandleIncrement);
        OutGPUDescHandle->ptr = HeapStartGpu.ptr + (Index * HeapHandleIncrement);
    }
        
    void AllocRange(UINT Count, D3D12_CPU_DESCRIPTOR_HANDLE* OutFirstCPU, D3D12_GPU_DESCRIPTOR_HANDLE* OutFirstGPU)
    {
        if (Count == 0)
        {
            return;
        }
        
        ImVector<int> Sorted;
        Sorted.resize(FreeIndices.Size);
        memcpy(Sorted.Data, FreeIndices.Data, FreeIndices.Size * sizeof(int));
        for (int I = 1; I < Sorted.Size; ++I)
        {
            int Key = Sorted[I];
            int J = I - 1;
            while (J >= 0 && Sorted[J] > Key)
            {
                Sorted[J + 1] = Sorted[J];
                --J;
            }
            Sorted[J + 1] = Key;
        }
        
        int RunStart = -1;
        for (int I = 0; I <= Sorted.Size - (int)Count; ++I)
        {
            bool Found = true;
            for (UINT K = 1; K < Count; ++K)
            {
                if (Sorted[I + K] != Sorted[I] + (int)K)
                {
                    Found = false;
                    break;
                }
            }
            if (Found)
            {
                RunStart = I;
                break;
            }
        }
        
        IM_ASSERT(RunStart >= 0);
        int FirstIndex = Sorted[RunStart];
        for (UINT K = 0; K < Count; ++K)
        {
            int Target = FirstIndex + (int)K;
            for (int I = 0; I < FreeIndices.Size; ++I)
            {
                if (FreeIndices[I] == Target)
                {
                    FreeIndices.erase(FreeIndices.Data + I);
                    break;
                }
            }
        }
        
        OutFirstCPU->ptr = HeapStartCpu.ptr + (FirstIndex * HeapHandleIncrement);
        OutFirstGPU->ptr = HeapStartGpu.ptr + (FirstIndex * HeapHandleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE CPUDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE GPUDescHandle)
    {
        int CpuIndex = (int)((CPUDescHandle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int GpuIndex = (int)((GPUDescHandle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(CpuIndex == GpuIndex);
        FreeIndices.push_back(CpuIndex);
    }
};

class BindlessAllocator
{
public:
    ErrorCode Initialize(ID3D12DescriptorHeap* ExistingHeap, unsigned int NumDescriptors, unsigned int StartOffset = 0);
    unsigned int AllocateRange(unsigned int Count);
    void Reset();
    ID3D12DescriptorHeap* GetHeap() const;
    unsigned int GetDescriptorSize() const;
    
private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
    ID3D12DescriptorHeap* ExternalHeap = nullptr;
    unsigned int DescriptorSize = 0;
    unsigned int NextDescriptorIndex = 0;
    unsigned int MaxDescriptors = 0;
    unsigned int HeapStartOffset = 0;
};

struct FrameContext
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
    UINT64 FenceValue;
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetCPUDescriptorHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilCPUDescriptorHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetSRVCPUDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE RenderTargetSRVGPUDescriptorHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE TransitionUAVCPUDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE TransitionUAVGPUDescriptorHandle;
    Microsoft::WRL::ComPtr<ID3D12Resource> RenderTarget = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> TransitionTexture = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> DepthStencilBuffer = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VisibilityBuffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE VisibilityBufferRTVHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE VisibilityBufferSRVCPUHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE VisibilityBufferSRVGPUHandle;

    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetUAVCPUHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE RenderTargetUAVGPUHandle;
};

struct IDxcBlob;
struct Texture;

class PipelineInterface : public Singleton<PipelineInterface>
{
    friend class Singleton<PipelineInterface>;
    PipelineInterface() = default; 
    ~PipelineInterface() override = default;
    
    const int BackBufferCount = 2;
    const unsigned int BindlessTextureStartIndex = 32;
    const int SRVHeapSize = 32768;
    const unsigned int MaxTextureDescriptors = 16384;
    const unsigned int MaxCubemapDescriptors = 1024;
    
    Microsoft::WRL::ComPtr<ID3D12Device2> D3DDevice = nullptr;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> CommandList;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> D3DCommandQueue = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> SwapChain = nullptr;
    HANDLE SwapChainWaitableObject = nullptr;
    std::vector<FrameContext> FrameContexts;                                     //  FrameNumInFlight
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> UploadCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> UploadCommandList;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> UploadQueue = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Fence> UploadFence = nullptr;
    UINT64 UploadFenceValue = 0;
    HANDLE UploadFenceEvent = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> D3DRTVDescHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> D3DDSDescHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> D3DSRVCBVDescHeap = nullptr;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> IMGUIRenderTargetDescriptorHandles; //  BackBufferCount
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> IMGUIRenderTargetResources;              //  BackBufferCount
    
    ImGUIDescriptorHeapAllocator D3DSRVDescriptorHeapAllocator;
    HANDLE FenceEvent = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Fence> Fence = nullptr;
    unsigned int FrameIndex = 0;
    
    Microsoft::WRL::ComPtr<ID3D12RootSignature> MeshShaderRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> MeshShaderPipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> ComputeShaderRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> ComputeShaderPipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> MaterialResolveRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> MaterialResolvePipelineState;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> TerrainRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> TerrainPipelineState;

    Microsoft::WRL::ComPtr<ID3D12Resource> TerrainParamsBuffer[FrameNumInFlight];
    Microsoft::WRL::ComPtr<ID3D12Resource> TerrainResolveBuffer[FrameNumInFlight];

    D3D12_GPU_DESCRIPTOR_HANDLE TerrainHeightmapSRVGPU = {};
    D3D12_CPU_DESCRIPTOR_HANDLE MaterialResolveBufferTableCPU = {};
    D3D12_GPU_DESCRIPTOR_HANDLE MaterialResolveBufferTableGPU = {};
    static constexpr unsigned int MaterialResolveBufferDescCount = 9;

    BindlessAllocator TextureAllocator;
    BindlessAllocator CubemapAllocator;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalVertexBufferUpload;

    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalUniqueVerticesBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalUniqueVerticesBufferUpload;

    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalLocalIndicesBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalLocalIndicesBufferUpload;

    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalClusterBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalClusterBufferUpload;

    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalGroupBoundsBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalGroupBoundsBufferUpload;

    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalTriangleMaterialIDsBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalTriangleMaterialIDsBufferUpload;

    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalMaterialTableBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> GlobalMaterialTableBufferUpload;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> ScenePrimitiveBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> ScenePrimitiveBufferUpload;

    Microsoft::WRL::ComPtr<ID3D12Resource> ClusterCountBuffer;
    void* ClusterCountBufferMapped = nullptr;

    DirectX::XMFLOAT2 ViewportSize = DirectX::XMFLOAT2(0, 0);
    bool bResizedLastFrame = false;

    D3D12_FILL_MODE CurrentFillMode = D3D12_FILL_MODE_SOLID;

public:
    ErrorCode CreateRootSignature();
    ErrorCode CompileShaderFXC(const std::string& ShaderPath, const std::string& EntryPoint, const std::string& TargetProfile, Microsoft::WRL::ComPtr<ID3DBlob>& OutShaderBlob) const;
    ErrorCode CompileShaderDXC(const std::string& ShaderPath, const std::wstring& EntryPoint, const std::wstring& TargetProfile, Microsoft::WRL::ComPtr<IDxcBlob>& OutShaderBlob) const;
    ErrorCode RecompileShaders();
    ErrorCode CreateMeshShaderPipelineState();
    ErrorCode CreateMaterialResolveComputePipelineState();
    ErrorCode CreatePostProcessComputePipelineState();
    ErrorCode Initialize(HWND hWnd);
    void CleanUp();
    void PackImGuiInitInfo(ImGui_ImplDX12_InitInfo& OutInitInfo);
    unsigned int WaitForNextFrameResources();
    void WaitForLastSubmittedFrame();
    HRESULT Present(unsigned int SyncInterval, unsigned int Flags) const;
    void InsertIMGUIRenderTargetBarrier(D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, D3D12_RESOURCE_BARRIER_TYPE BarrierType = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAGS BarrierFlag = D3D12_RESOURCE_BARRIER_FLAG_NONE) const;
    void ClearIMGUIRenderTargetView(const float ColorRGBA[4], unsigned int NumRects, const D3D12_RECT *pRects) const;
    void OMSetIMGUIRenderTargets(unsigned int NumRenderTargetDescriptors, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) const;
    void ExecuteCommandLists() const;
    void CreateIMGUIRenderTarget();
    void CleanupIMGUIRenderTarget();
    void ResetCommandAllocator(unsigned int FrameContextIndex) const;
    HRESULT ResetCommandList(unsigned int FrameContextIndex) const;
    void Signal(unsigned long FenceValue) const;
    ID3D12GraphicsCommandList6* GetCommandList() const;
    IDXGISwapChain3* GetSwapChain();
    BindlessAllocator& GetTextureBindlessAllocator();
    BindlessAllocator& GetCubemapBindlessAllocator();
    void UpdateFrameContextFenceValue(unsigned int FrameContextIndex, unsigned long FenceValue);
    D3D12_GPU_DESCRIPTOR_HANDLE GetRenderTargetSRVGPUHandle(unsigned int FrameContextIndex) const;
    void ResetUploadCommandAllocator() const;
    void ResetUploadCommandList() const;
    void ExecuteAndWaitUploadCommandList();
    void ReleaseGlobalUploadBuffers();
    ErrorCode CreateGlobalMergedMeshBuffers(const Level* LevelInstance);
    ErrorCode UpdateScenePrimitiveBuffer(const Level* LevelInstance);
    ErrorCode CreateTexture(const Texture* TextureInstance, unsigned int DescriptorIndex, TextureProxy* TextureProxyInstance, bool ImmediateExecute = true);
    ErrorCode CreateCubemap(const CubemapTexture* CubemapInstance, unsigned int DescriptorIndex, CubemapTextureProxy* CubemapProxyInstance, bool ImmediateExecute = true);
    ErrorCode CreateConstantBuffer(const Actor* ActorInstance) const;
    ErrorCode UpdateViewport(unsigned int FrameContextIndex, ImVec2 NewViewportSize);
    void RenderVisibilityPass(unsigned int FrameContextIndex, const Level* LevelInstance) const;
    void RenderTerrainVisibilityPass(unsigned int FrameContextIndex, const Level* LevelInstance) const;
    void RenderMaterialResolve(unsigned int FrameContextIndex, const Level* LevelInstance) const;
    void RenderPostProcessCompute(unsigned int FrameContextIndex) const;
    ErrorCode CreateTerrainPipelineState();
    ErrorCode CreateTerrainResources(class TerrainActor* Terrain);
    D3D12_FILL_MODE GetFillMode() const { return CurrentFillMode; }
    ErrorCode SetFillMode(D3D12_FILL_MODE FillMode);
};