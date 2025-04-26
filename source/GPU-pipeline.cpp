// Dear ImGui: standalone example application for DirectX 12

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include "dx12/PipelineInterface.h"
#include "level/Level.h"
#include "shape/Cube.h"

static unsigned long                FenceLastSignaledValue = 0;
static bool                         SwapChainOccluded = false;

// Forward declarations of helper functions
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEXW WC = { sizeof(WC), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
	::RegisterClassExW(&WC);
	HWND hwnd = ::CreateWindowW(WC.lpszClassName, L"GPU pipeline", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, WC.hInstance, nullptr);
	
	if (PipelineInterface::GetInstance().Initialize(hwnd) != ErrorCode::OK)
	{
		PipelineInterface::GetInstance().CleanUp();
		::UnregisterClassW(WC.lpszClassName, WC.hInstance);
		return 1;
	}

	//	Initialize the level manually.
	const Actor* CubeActorInstance = Level::GetInstance().InstantiateCubeActor();
	PipelineInterface::GetInstance().CreateShapeProxyBuffer(CubeActorInstance->GetShapeInstance(), CubeActorInstance->GetShapeProxyInstance());
	const CameraActor* CameraActorInstance = static_cast<CameraActor*>(Level::GetInstance().InstantiateCameraActor());
	PipelineInterface::GetInstance().CreateConstantBuffer(CameraActorInstance);
	
	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);

	ImGui_ImplDX12_InitInfo InitInfo = {};
	PipelineInterface::GetInstance().PackImGuiInitInfo(InitInfo);
	ImGui_ImplDX12_Init(&InitInfo);

	// Before 1.91.6: our signature was using a single descriptor. From 1.92, specifying SrvDescriptorAllocFn/SrvDescriptorFreeFn will be required to benefit from new features.
	//ImGui_ImplDX12_Init(g_pd3dDevice, APP_NUM_FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap, g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != nullptr);
	
	// Our state
	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Main loop
	bool done = false;
	while (!done)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;
		
		// Handle window screen locked
		if (SwapChainOccluded && PipelineInterface::GetInstance().Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			::Sleep(10);
			continue;
		}
		SwapChainOccluded = false;

		//	Update the level.
		//Level::GetInstance().Update(0);
		
		//	Render the level.
		const unsigned int FrameContextIndex = PipelineInterface::GetInstance().WaitForNextFrameResources();
		
		//	After we have submitted the rendering commands for a complete frame to the
		//	GPU, we would like to reuse the memory in the command allocator for the next
		//	frame. The ID3D12CommandAllocator::Reset method may be used for this:
		PipelineInterface::GetInstance().ResetCommandAllocator(FrameContextIndex);
		HRESULT Result = PipelineInterface::GetInstance().ResetCommandList(FrameContextIndex);
		if (FAILED(Result))
		{
			return 0;
		}

		Level::GetInstance().Update(0);
		PipelineInterface::GetInstance().RenderLevel(FrameContextIndex, &Level::GetInstance());
		
		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Scene View");
		ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
		ImGui::Image((ImTextureID)PipelineInterface::GetInstance().GetLevelRenderTargetGPUHandle().ptr, ViewportSize);
		ImGui::End();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show_another_window);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (show_another_window)
		{
			ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				show_another_window = false;
			ImGui::End();
		}

		// Rendering
		ImGui::Render();
		
		unsigned int BackBufferIndex = PipelineInterface::GetInstance().GetCurrentBackBufferIndex();
		PipelineInterface::GetInstance().InsertIMGUIRenderTargetBarrier(FrameContextIndex, BackBufferIndex,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
		
		// Render Dear ImGui graphics
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		PipelineInterface::GetInstance().ClearIMGUIRenderTargetView(FrameContextIndex, BackBufferIndex, clear_color_with_alpha, 0, nullptr);
		PipelineInterface::GetInstance().OMSetIMGUIRenderTargets(FrameContextIndex, 1, BackBufferIndex, false, nullptr);
		//	Has been call during Level:Render()
		//PipelineInterface::GetInstance().SetSRVDescriptorHeaps(FrameContextIndex, 1);
	
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), PipelineInterface::GetInstance().GetCommandList(FrameContextIndex));
		
		PipelineInterface::GetInstance().InsertIMGUIRenderTargetBarrier(FrameContextIndex, BackBufferIndex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		PipelineInterface::GetInstance().GetCommandList(FrameContextIndex)->Close();
		PipelineInterface::GetInstance().ExecuteCommandLists(FrameContextIndex);
		
		// Update and Render additional Platform Windows
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		// Present
		HRESULT HR = PipelineInterface::GetInstance().Present(1, 0) ;   // Present with vsync
		//HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
		SwapChainOccluded = (HR == DXGI_STATUS_OCCLUDED);

		unsigned long FenceValue = FenceLastSignaledValue + 1;
		PipelineInterface::GetInstance().Signal(FenceValue);
		FenceLastSignaledValue = FenceValue;
		PipelineInterface::GetInstance().UpdateFrameContextFenceValue(FrameContextIndex, FenceValue);
	}

	PipelineInterface::GetInstance().WaitForLastSubmittedFrame();

	// Cleanup
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	PipelineInterface::GetInstance().CleanUp();
	::DestroyWindow(hwnd);
	::UnregisterClassW(WC.lpszClassName, WC.hInstance);

	return 0;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (/*PipelineInterface::GetInstance().g_pd3dDevice != nullptr &&*/ wParam != SIZE_MINIMIZED)
		{
			PipelineInterface::GetInstance().WaitForLastSubmittedFrame();
			PipelineInterface::GetInstance().CleanupIMGUIRenderTarget();
			HRESULT Result = PipelineInterface::GetInstance().GetSwapChain()->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
			assert(SUCCEEDED(Result) && "Failed to resize swapchain.");
			PipelineInterface::GetInstance().CreateIMGUIRenderTarget();
		}
		return 0;
	case WM_PAINT:
		
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
