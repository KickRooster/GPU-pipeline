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
    struct ExampleDescriptorHeapAllocator
    {
        ID3D12DescriptorHeap* Heap = nullptr;
        D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
        D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
        D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
        UINT                        HeapHandleIncrement;
        ImVector<int>               FreeIndices;

        void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
        {
            IM_ASSERT(Heap == nullptr && FreeIndices.empty());
            Heap = heap;
            D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
            HeapType = desc.Type;
            HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
            HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
            HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
            FreeIndices.reserve((int)desc.NumDescriptors);
            for (int n = desc.NumDescriptors; n > 0; n--)
                FreeIndices.push_back(n - 1);
        }
        void Destroy()
        {
            Heap = nullptr;
            FreeIndices.clear();
        }
        void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
        {
            IM_ASSERT(FreeIndices.Size > 0);
            int idx = FreeIndices.back();
            FreeIndices.pop_back();
            out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
            out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
        }
        void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
        {
            int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
            int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
            IM_ASSERT(cpu_idx == gpu_idx);
            FreeIndices.push_back(cpu_idx);
        }
    };

    struct FrameContext
    {
        ID3D12CommandAllocator* CommandAllocator;
        UINT64                      FenceValue;
    };
    
    class PipelineInterface : public Singleton<PipelineInterface>
    {
    private:
        friend class Singleton<PipelineInterface>;
        PipelineInterface() = default; 
        ~PipelineInterface() = default;

        //  TODO:   make it non static
        static const int APP_NUM_BACK_BUFFERS = 2;
        const int APP_SRV_HEAP_SIZE = 64;
        static const int APP_NUM_FRAMES_IN_FLIGHT = 2;

        //  TODO:   hide it
    public:
        ID3D12Device* g_pd3dDevice = nullptr;
    private:
        ID3D12DescriptorHeap* g_pd3dRtvDescHeap = nullptr;
    public:
        D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[APP_NUM_BACK_BUFFERS];
    private:
        ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
        ExampleDescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;
        //  TODO:   hide it
    public:
        ID3D12CommandQueue* g_pd3dCommandQueue = nullptr;
    private:
        FrameContext                 g_frameContext[APP_NUM_FRAMES_IN_FLIGHT] = {};
    public:
        ID3D12GraphicsCommandList* g_pd3dCommandList = nullptr;
        ID3D12Fence* g_fence = nullptr;
    private:
        HANDLE                       g_fenceEvent = nullptr;
    public:
        IDXGISwapChain3* g_pSwapChain = nullptr;
    private:
        HANDLE                       g_hSwapChainWaitableObject = nullptr;
        ID3D12Resource* g_mainRenderTargetResource[APP_NUM_BACK_BUFFERS] = {};
        UINT                         g_frameIndex = 0;
        
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
        void OMSetRenderTargets(unsigned int NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, bool RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) const;
        //  XXX:     Current set for srv only.
        void SetDescriptorHeaps(unsigned int NumDescriptorHeaps) const;
        void ExecuteCommandLists();
        void CreateRenderTarget();
        void CleanupRenderTarget();
        ID3D12GraphicsCommandList* GetCommandList();
        ID3D12CommandQueue* GetCommandQueue();
    };
}
