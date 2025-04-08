#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include "../base/Base.h"
#include "../base/DesignPatterns.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"

namespace dev
{
    // Simple free list based allocator
    struct ImGUIDescriptorHeapAllocator
    {
        ID3D12DescriptorHeap* Heap = nullptr;
        D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
        D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
        D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
        UINT                        HeapHandleIncrement;
        ImVector<int>               FreeIndices;

        void Create(ID3D12Device* Device, ID3D12DescriptorHeap* Heap)
        {
            IM_ASSERT(this->Heap == nullptr && FreeIndices.empty());
            this->Heap = Heap;
            D3D12_DESCRIPTOR_HEAP_DESC Desc = Heap->GetDesc();
            HeapType = Desc.Type;
            HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
            HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
            HeapHandleIncrement = Device->GetDescriptorHandleIncrementSize(HeapType);
            FreeIndices.reserve((int)Desc.NumDescriptors);
            for (int N = Desc.NumDescriptors; N > 0; --N)
                FreeIndices.push_back(N - 1);
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
        ID3D12CommandAllocator* CommandAllocator;
        UINT64                      FenceValue;
    };
    
    class PipelineInterface : public Singleton<PipelineInterface>
    {
        friend class Singleton<PipelineInterface>;
        PipelineInterface() = default; 
        ~PipelineInterface() = default;
        
        static const int BackBufferCount = 2;
        static const int FrameNumInFlight = 2;
        const int SrvHeapSize = 64;
        
        ID3D12Device* D3DDevice = nullptr;
        ID3D12DescriptorHeap* D3DRtvDescHeap = nullptr;
        ID3D12DescriptorHeap* D3DSrvDescHeap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE MainRenderTargetDescriptors[BackBufferCount];
        ImGUIDescriptorHeapAllocator D3DSrvDescHeapAlloc;
        ID3D12CommandQueue* D3DCommandQueue = nullptr;
        FrameContext FrameContexts[FrameNumInFlight] = {};
        ID3D12GraphicsCommandList* D3DCommandList = nullptr;
        HANDLE FenceEvent = nullptr;
        IDXGISwapChain3* SwapChain = nullptr;
        HANDLE SwapChainWaitableObject = nullptr;
        ID3D12Resource* MainRenderTargetResources[BackBufferCount] = {};
        UINT FrameIndex = 0;
        ID3D12Fence* Fence = nullptr;
        
    public:
        ErrorCode Initialize(HWND hWnd);
        void CleanUp();
        void PackImGuiInitInfo(ImGui_ImplDX12_InitInfo& OutInitInfo);
        FrameContext* WaitForNextFrameResources();
        void WaitForLastSubmittedFrame();
        HRESULT Present(unsigned int SyncInterval, unsigned int Flags) const;
        unsigned int GetCurrentBackBufferIndex() const;
        void InsertRenderTargetBarrier(FrameContext* frameCtx, unsigned int BackbufferIndex, D3D12_RESOURCE_BARRIER& OutBarrier) const;
        void ClearRenderTargetView(unsigned int BackBufferIndex, const float ColorRGBA[4], unsigned int NumRects, const D3D12_RECT *pRects) const;
        void OMSetRenderTargets(unsigned int NumRenderTargetDescriptors, unsigned int BackBufferIndex, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) const;
        //  XXX:     Current set for srv only.
        void SetDescriptorHeaps(unsigned int NumDescriptorHeaps) const;
        void ExecuteCommandLists();
        void CreateRenderTarget();
        void CleanupRenderTarget();
        ID3D12GraphicsCommandList* GetCommandList();
        ID3D12CommandQueue* GetCommandQueue();
        IDXGISwapChain3* GetSwapChain();
        ID3D12Fence* GetFence();
    };
}
