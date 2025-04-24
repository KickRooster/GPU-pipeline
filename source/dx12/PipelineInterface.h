#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include "../base/Base.h"
#include "../base/DesignPatterns.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include <vector>
#include "ShapeProxy.h"
#include "../level/Level.h"
#include "../shape/Shape.h"

class CameraActor;
using namespace Microsoft::WRL;
using namespace std;

// Simple free list based allocator
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
        
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE CPUDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE GPUDescHandle)
    {
        int CpuIndex = (int)((CPUDescHandle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int GpuIndex = (int)((GPUDescHandle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(CpuIndex == GpuIndex);
        FreeIndices.push_back(CpuIndex);
    }
};

struct FrameContext
{
    ComPtr<ID3D12CommandAllocator> CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> CommandList;
    UINT64 FenceValue;
};

class PipelineInterface : public Singleton<PipelineInterface>
{
public:
    void Draw(unsigned int FrameContextIndex, const Actor* Actor);
private:
    
    friend class Singleton<PipelineInterface>;
    PipelineInterface() = default; 
    ~PipelineInterface() = default;
    
    const int BackBufferCount = 2;
    const int FrameNumInFlight = 2;
    const int SRVHeapSize = 64;
    
    ComPtr<ID3D12Device> D3DDevice = nullptr;
    ComPtr<ID3D12CommandQueue> D3DCommandQueue = nullptr;
    ComPtr<IDXGISwapChain3> SwapChain = nullptr;
    HANDLE SwapChainWaitableObject = nullptr;
    vector<FrameContext> FrameContexts;                                     //  FrameNumInFlight
    
    ComPtr<ID3D12DescriptorHeap> D3DRTVDescHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> D3DSRVCBVDescHeap = nullptr;
    vector<D3D12_CPU_DESCRIPTOR_HANDLE> IMGUIRenderTargetDescriptorHandles; //  BackBufferCount
    D3D12_CPU_DESCRIPTOR_HANDLE LevelRenderTargetDescriptorHandle;
    vector<ComPtr<ID3D12Resource>> IMGUIRenderTargetResources;              //  BackBufferCount
    ComPtr<ID3D12Resource> LevelRenderTargetResource;
        
    ImGUIDescriptorHeapAllocator D3DSRVDescriptorHeapAllocator;
    HANDLE FenceEvent = nullptr;
    ComPtr<ID3D12Fence> Fence = nullptr;
    unsigned int FrameIndex = 0;

    ComPtr<ID3D12Resource> RenderTarget = nullptr;
    ComPtr<ID3D12RootSignature> RootSignature;
    ComPtr<ID3D12PipelineState> PipelineState;
    D3D12_GPU_DESCRIPTOR_HANDLE LevelSRVGPUHandle;
    
    //  Constant buffer data
    D3D12_CPU_DESCRIPTOR_HANDLE ConstantBufferCPUHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE ConstantBufferGPUHandle;
    ComPtr<ID3D12Resource> ConstantBuffer_;
    unsigned int* ConstantBufferDataBegin;
public:
    ErrorCode Initialize(HWND hWnd);
    void CleanUp();
    void PackImGuiInitInfo(ImGui_ImplDX12_InitInfo& OutInitInfo);
    unsigned int WaitForNextFrameResources();
    void WaitForLastSubmittedFrame();
    HRESULT Present(unsigned int SyncInterval, unsigned int Flags) const;
    unsigned int GetCurrentBackBufferIndex() const;
    void InsertIMGUIRenderTargetBarrier(unsigned int FrameContextIndex, unsigned int BackbufferIndex, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, D3D12_RESOURCE_BARRIER_TYPE BarrierType = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAGS BarrierFlag = D3D12_RESOURCE_BARRIER_FLAG_NONE) const;
    void ClearIMGUIRenderTargetView(unsigned int FrameContextIndex, unsigned int BackBufferIndex, const float ColorRGBA[4], unsigned int NumRects, const D3D12_RECT *pRects) const;
    void OMSetIMGUIRenderTargets(unsigned int FrameContextIndex, unsigned int NumRenderTargetDescriptors, unsigned int BackBufferIndex, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) const;
    void SetSRVDescriptorHeaps(unsigned int FrameContextIndex, unsigned int NumDescriptorHeaps) const;
    void ExecuteCommandLists(unsigned int FrameContextIndex) const;
    void CreateIMGUIRenderTarget();
    void CleanupIMGUIRenderTarget();
    void ResetCommandAllocator(unsigned int FrameContextIndex) const;
    HRESULT ResetCommandList(unsigned int FrameContextIndex) const;
    void Signal(unsigned long FenceValue) const;
    ID3D12GraphicsCommandList* GetCommandList(unsigned int FrameContextIndex) const;
    IDXGISwapChain3* GetSwapChain();
    void UpdateFrameContextFenceValue(unsigned int FrameContextIndex, unsigned long FenceValue);
    D3D12_GPU_DESCRIPTOR_HANDLE GetLevelRenderTargetGPUHandle() const;
    void CreateVertexBuffer(const Shape* ShapeInstance, ShapeProxy* ShapeProxyInstance);
    void CreateShapeProxyBuffer(unsigned int FrameContextIndex, const Shape* ShapeInstance, ShapeProxy* ShapeProxyInstance);
    void CreateConstantBuffer(CameraActor* CameraActorInstance);
    
    //  Interface for render GPU pipeline only
    void RenderLevel(unsigned int FrameContextIndex, const Level* LevelInstance);
};