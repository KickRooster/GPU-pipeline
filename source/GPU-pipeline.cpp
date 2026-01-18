// Dear ImGui: standalone example application for DirectX 12

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// ImGuizmo 需要的定义
#define IMGUI_DEFINE_MATH_OPERATORS

#include "misc/Base.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include "dx12/PipelineInterface.h"
#include "level/Level.h"
#include "actor/Camera.h"
#include "actor/StaticMesh.h"
#include "actor/CullingVisualCamera.h"
#include "misc/FileTool.h"
#include "../thirdpatry/ImGuizmo/ImGuizmo.h"
#include "../thirdpatry/imGuIZMO.quat/imguizmo_quat/imGuIZMOquat.h"

using namespace std;
using namespace DirectX;

unsigned long FenceLastSignaledValue = 0;
bool SwapChainOccluded = false;
UIState State;
StaticMesh* SelectedActor = nullptr;
ImGuizmo::OPERATION GizmoOperation = ImGuizmo::TRANSLATE;

void RefreshInput(UIState& OutState)
{
	OutState.WDown = ImGui::IsKeyDown(ImGuiKey_W);
	OutState.SDown = ImGui::IsKeyDown(ImGuiKey_S);
	OutState.ADown = ImGui::IsKeyDown(ImGuiKey_A);
	OutState.DDown = ImGui::IsKeyDown(ImGuiKey_D);
	OutState.QDown = ImGui::IsKeyDown(ImGuiKey_Q);
	OutState.EDown = ImGui::IsKeyDown(ImGuiKey_E);
	OutState.RDown = ImGui::IsKeyDown(ImGuiKey_R);
	
	if (State.MoveSpeed == 0)
	{
		State.MoveSpeed = 0.008f;
	}
	
	const float MouseWheel = ImGui::GetIO().MouseWheel;
	if (MouseWheel > 0.0f)
	{
		State.MoveSpeed *= 1.2f;
	}
	else if (MouseWheel < 0.0f)
	{
		State.MoveSpeed *= 0.8f;
	}

	State.LeftButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	State.RightButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

	State.DeltaX = ImGui::GetIO().MouseDelta.x;
	State.DeltaY = ImGui::GetIO().MouseDelta.y;
	State.RotateSpeed = 0.000087f;
}

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

	//	Culling Visual Camera Actor.
	StaticMesh* CullingCameraInstance = Level::GetInstance().InstantiateCullingVisualCamera();
	
	// Camera Actor.
	Camera* CameraInstance = Level::GetInstance().InstantiateCamera();
	PipelineInterface::GetInstance().CreateConstantBuffer(CameraInstance);

	//	SkyLight Actor.
	Level::GetInstance().InstantiateSkyLight();
	
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
		
		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		
		RefreshInput(State);
		
		ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDocking;
		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(Viewport->WorkPos);
		ImGui::SetNextWindowSize(Viewport->WorkSize);
		ImGui::SetNextWindowViewport(Viewport->ID);
		WindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		
		ImGui::Begin("DockSpace", nullptr, WindowFlags);
		ImGuiID DockspaceID = ImGui::GetID("MainDockSpace");
		ImGui::DockSpace(DockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		ImGui::End();
		
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::BeginMenu("Import"))
				{
					if (ImGui::MenuItem("Mesh"))
					{
						//Level::GetInstance().InstantiateStaticMeshes(FileTool::GetInstance().GetMeshFullPath("sponza.obj"));
						//Level::GetInstance().InstantiateStaticMeshes(FileTool::GetInstance().GetMeshFullPath("Cerberus_LP.FBX"));
						Level::GetInstance().InstantiateStaticMeshes(FileTool::GetInstance().GetMeshFullPath("duck.fbx"));
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
		
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

		ImGui::Begin("Outliner");
		{
			vector<StaticMesh*> AllActors = Level::GetInstance().GetStaticMeshes();

			for (unsigned int I = 0; I < AllActors.size(); ++I)
			{
				bool IsSelected = (SelectedActor == AllActors[I]);
				
				string UniqueLabel = AllActors[I]->Name + "##" + to_string(I);
				if (ImGui::Selectable(UniqueLabel.c_str(), IsSelected))
				{
					SelectedActor = AllActors[I];
				}
			}
		}
		ImGui::End();

		ImGui::Begin("Details");
		{
			if (SelectedActor != nullptr)
			{
				if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Text("Location"); ImGui::SameLine(100);
					float Position[3] = { SelectedActor->Transform.Position.x, SelectedActor->Transform.Position.y, SelectedActor->Transform.Position.z };
					if (ImGui::DragFloat3("##Location", Position, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
					{
						SelectedActor->Transform.Position = { Position[0], Position[1], Position[2] };
					}
					
					ImGui::Text("Rotation"); ImGui::SameLine(100);
					XMVECTOR QuatVector = XMLoadFloat4(&SelectedActor->Transform.Rotation);
					XMMATRIX RotationMatrix = XMMatrixRotationQuaternion(QuatVector);
					XMFLOAT4X4 RotMatrix4x4;
					XMStoreFloat4x4(&RotMatrix4x4, RotationMatrix);
					
					float Pitch = asinf(-RotMatrix4x4._32);
					float Yaw = atan2f(RotMatrix4x4._31, RotMatrix4x4._33);
					float Roll = atan2f(RotMatrix4x4._12, RotMatrix4x4._22);
					
					float RotationDegrees[3] = { 
						XMConvertToDegrees(Pitch), 
						XMConvertToDegrees(Yaw), 
						XMConvertToDegrees(Roll) 
					};
					
					if (ImGui::DragFloat3("##Rotation", RotationDegrees, 1.0f, -180.0f, 180.0f, "%.1f°"))
					{
						float PitchRad = XMConvertToRadians(RotationDegrees[0]);
						float YawRad = XMConvertToRadians(RotationDegrees[1]);
						float RollRad = XMConvertToRadians(RotationDegrees[2]);
						
						XMMATRIX NewRotationMatrix = XMMatrixRotationRollPitchYaw(PitchRad, YawRad, RollRad);
						XMVECTOR NewQuaternion = XMQuaternionRotationMatrix(NewRotationMatrix);
						XMStoreFloat4(&SelectedActor->Transform.Rotation, NewQuaternion);
					}
					
					ImGui::Text("Scale"); ImGui::SameLine(100);
					float Scale[3] = { SelectedActor->Transform.Scale.x, SelectedActor->Transform.Scale.y, SelectedActor->Transform.Scale.z };
					if (ImGui::DragFloat3("##Scale", Scale, 0.01f, 0.001f, FLT_MAX, "%.3f"))
					{
						SelectedActor->Transform.Scale = { Scale[0], Scale[1], Scale[2] };
					}
				}
				
				CullingVisualCamera* CullingCamera = dynamic_cast<CullingVisualCamera*>(SelectedActor);
				if (CullingCamera != nullptr)
				{
					if (ImGui::CollapsingHeader("Culling Visual Camera Actor", ImGuiTreeNodeFlags_DefaultOpen))
					{
						bool IsCullingCamera = (CameraInstance->GetCullingCamera() == CullingCamera);
						
						if (ImGui::Checkbox("UseAsCullingCamera", &IsCullingCamera))
						{
							if (Level::GetInstance().GetCameras().size() > 0)
							{
								if (IsCullingCamera)
								{
									CameraInstance->SetCullingCamera(CullingCamera);
								}
								else
								{
									CameraInstance->SetCullingCamera(nullptr);
								}
							}
						}
						
						ImGui::Separator();
						
						float FovY = CullingCamera->FovY;
						ImGui::Text("Field of View"); ImGui::SameLine(100);
						bool FovChanged = ImGui::DragFloat("##FovY", &FovY, 1.0f, 10.0f, 179.0f, "%.1f°");
						
						ImGui::Text("Aspect Ratio"); ImGui::SameLine(100);
						
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
						char AspectRatioText[32];
						sprintf_s(AspectRatioText, "%.2f", CullingCamera->AspectRatio);
						ImGui::Button(AspectRatioText, ImVec2(ImGui::CalcItemWidth(), 0));
						ImGui::PopStyleVar();
						
						float NearPlane = CullingCamera->NearPlane;
						ImGui::Text("Near Plane"); ImGui::SameLine(100);
						bool NearChanged = ImGui::DragFloat("##NearPlane", &NearPlane, 0.01f, 0.001f, 100.0f, "%.3f");
						
						float FarPlane = CullingCamera->FarPlane;
						ImGui::Text("Far Plane"); ImGui::SameLine(100);
						bool FarChanged = ImGui::DragFloat("##FarPlane", &FarPlane, 1.0f, 1.0f, 10000.0f, "%.1f");
						
						if (FovChanged || NearChanged || FarChanged)
						{
							CullingCamera->FovY = FovY;
							CullingCamera->NearPlane = NearPlane;
							CullingCamera->FarPlane = FarPlane;
						}
					}
				}
			}
		}
		ImGui::End();
		
		ImGui::Begin("Tools");
		{
			if (ImGui::BeginTabBar("ToolsTabBar"))
			{
				if (ImGui::BeginTabItem("Shaders"))
				{
					if (ImGui::Button("Recompile Shaders"))
					{
						ErrorCode Result = PipelineInterface::GetInstance().RecompileShaders();
						if (Result == ErrorCode::OK)
						{
							OutputDebugStringA("Shaders recompiled successfully!\n");
						}
						else
						{
							OutputDebugStringA("Shader recompilation failed!\n");
						}
					}
					
					ImGui::EndTabItem();
				}
				
				if (ImGui::BeginTabItem("X"))
				{
					ImGui::Text("X tab content goes here");
					
					ImGui::EndTabItem();
				}
				
				ImGui::EndTabBar();
			}
		}
		ImGui::End();
		
		ImGui::Begin("Content Browser");
		{
			static std::vector<std::string> TextureFiles;
			static bool FirstTime = true;
			static int SelectedTextureIndex = -1;
			
			if (FirstTime)
			{
				TextureFiles = FileTool::GetInstance().GetTextureFiles();
				FirstTime = false;
			}
			
			if (ImGui::BeginMenuBar())
			{
				ImGui::Text("Textures: %zu files", TextureFiles.size());
				ImGui::EndMenuBar();
			}
			
			float ThumbnailSize = 80.0f;
			float ItemSpacing = 8.0f;
			ImVec2 ContentSize = ImGui::GetContentRegionAvail();
			int ItemsPerRow = static_cast<int>(ContentSize.x / (ThumbnailSize + ItemSpacing));
			if (ItemsPerRow < 1)
			{
				ItemsPerRow = 1;
			}
			
			for (size_t I = 0; I < TextureFiles.size(); ++I)
			{
				if (I > 0 && I % ItemsPerRow != 0)
				{
					ImGui::SameLine(0, ItemSpacing);
				}
				
				std::string FileName = TextureFiles[I];
				size_t DotPos = FileName.find_last_of('.');
				if (DotPos != std::string::npos)
				{
					FileName = FileName.substr(0, DotPos);
				}
				
				std::string UniqueID = "Texture" + std::to_string(I);
				
				bool IsSelected = (SelectedTextureIndex == static_cast<int>(I));
				
				if (ImGui::Selectable(UniqueID.c_str(), IsSelected, ImGuiSelectableFlags_None, ImVec2(ThumbnailSize, ThumbnailSize + 25)))
				{
					SelectedTextureIndex = static_cast<int>(I);
				}
				
				ImVec2 ItemPos = ImGui::GetItemRectMin();
				
				ImGui::GetWindowDrawList()->AddRectFilled(
					ItemPos,
					ImVec2(ItemPos.x + ThumbnailSize, ItemPos.y + ThumbnailSize),
					IsSelected ? IM_COL32(100, 150, 200, 255) : IM_COL32(120, 120, 120, 255),
					3.0f
				);
				
				ImVec2 TextPos = ImVec2(ItemPos.x, ItemPos.y + ThumbnailSize + 2);
				
				std::string DisplayName = FileName;
				ImVec2 TextSize = ImGui::CalcTextSize(DisplayName.c_str());
				
				if (TextSize.x > ThumbnailSize)
				{
					float CharWidth = TextSize.x / DisplayName.length();
					int MaxChars = static_cast<int>(ThumbnailSize / CharWidth) - 2;
					
					if (MaxChars > 3)
					{
						DisplayName = DisplayName.substr(0, MaxChars) + "..";
					}
					else
					{
						DisplayName = "...";
					}
				}
				
				ImGui::GetWindowDrawList()->AddText(
					TextPos,
					IsSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255),
					DisplayName.c_str()
				);
			}
		}
		ImGui::End();
		
		ImGui::Begin("Scene View");
		{
			float DeltaTime = 1000.0f / io.Framerate;
			CameraInstance->ResponseToUI(State, DeltaTime); 
			ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
			PipelineInterface::GetInstance().UpdateViewport(FrameContextIndex, ViewportSize);
			CameraInstance->ViewportWidth = ViewportSize.x;
			CameraInstance->ViewportHeight = ViewportSize.y;
			CameraInstance->AspectRatio = ViewportSize.x / ViewportSize.y;
			static_cast<CullingVisualCamera*>(CullingCameraInstance)->AspectRatio = CameraInstance->AspectRatio;

			Level::GetInstance().Update(DeltaTime, FrameContextIndex);
			PipelineInterface::GetInstance().RenderLevel(FrameContextIndex, &Level::GetInstance());
			
			// Post-processing pass: ACES tone mapping (RenderTarget → TransitionTexture → RenderTarget)
			PipelineInterface::GetInstance().RenderPostProcessCompute(FrameContextIndex);
			
			ImGui::Image(static_cast<ImTextureID>(PipelineInterface::GetInstance().GetRenderTargetSRVGPUHandle(FrameContextIndex).ptr), ViewportSize);
			
			XMMATRIX ViewMatrix = CameraInstance->GetViewMatrix();
			XMMATRIX ViewMatrixInv = XMMatrixInverse(nullptr, ViewMatrix);
			XMMATRIX ProjectionMatrix = CameraInstance->GetProjectionMatrix();
			XMFLOAT4X4 ViewMatrix4x4;
			XMFLOAT4X4 ViewMatrixInv4x;
			XMFLOAT4X4 ProjectionMatrix4x4;
			XMStoreFloat4x4(&ViewMatrix4x4, ViewMatrix);
			XMStoreFloat4x4(&ViewMatrixInv4x, ViewMatrixInv);
			XMStoreFloat4x4(&ProjectionMatrix4x4, ProjectionMatrix);
			
			const float AxisIndicatorSize = min(ViewportSize.x, ViewportSize.y) * 0.2f;
			ImVec2 WindowPos = ImGui::GetWindowPos();
			ImVec2 ContentMin = ImGui::GetWindowContentRegionMin();
			ImVec2 AxisIndicatorPos = ImVec2(
				WindowPos.x + ContentMin.x + ViewportSize.x - AxisIndicatorSize,
				WindowPos.y + ContentMin.y
			);
			
			ImGuizmo::BeginFrame();
			ImGuizmo::ViewManipulate(&ViewMatrixInv4x.m[0][0], 10.0f, AxisIndicatorPos, 
				ImVec2(AxisIndicatorSize, AxisIndicatorSize), 0x00000000);
			
			if (SelectedActor != nullptr)
			{
				//if (ImGui::IsWindowFocused())
				{
					if (State.WDown && !State.RightButtonDown)
					{
						GizmoOperation = ImGuizmo::TRANSLATE;
					}
					else if (State.EDown && !State.RightButtonDown)
					{
						GizmoOperation = ImGuizmo::ROTATE;
					}
					else if (State.RDown && !State.RightButtonDown)
					{
						GizmoOperation = ImGuizmo::SCALE;
					}
				}
				
				ImVec2 ImagePos = ImVec2(WindowPos.x + ContentMin.x, WindowPos.y + ContentMin.y);
				ImVec2 ImageSize = ViewportSize;
				
				ImGuizmo::SetRect(ImagePos.x, ImagePos.y, ImageSize.x, ImageSize.y);
				ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
				ImGuizmo::AllowAxisFlip(false);
				
				XMFLOAT4X4 ObjectMatrix4x4;
				XMStoreFloat4x4(&ObjectMatrix4x4, SelectedActor->GetWorldMatrix());
				
				if (ImGuizmo::Manipulate(&ViewMatrix4x4.m[0][0], &ProjectionMatrix4x4.m[0][0], 
					GizmoOperation, ImGuizmo::LOCAL, &ObjectMatrix4x4.m[0][0]))
				{
					XMMATRIX TranslatedObjectMatrix = XMLoadFloat4x4(&ObjectMatrix4x4);
					
					XMVECTOR Scale;
					XMVECTOR Rotation;
					XMVECTOR Translation;
					
					if (XMMatrixDecompose(&Scale, &Rotation, &Translation, TranslatedObjectMatrix))
					{
						XMStoreFloat3(&SelectedActor->Transform.Position, Translation);
						XMStoreFloat4(&SelectedActor->Transform.Rotation, Rotation);
						XMStoreFloat3(&SelectedActor->Transform.Scale, Scale);
					}
				}
			}
		}
		ImGui::End();
		
		// Rendering
		ImGui::Render();
		PipelineInterface::GetInstance().InsertIMGUIRenderTargetBarrier(D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
		
		// Render Dear ImGui graphics
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		PipelineInterface::GetInstance().ClearIMGUIRenderTargetView(clear_color_with_alpha, 0, nullptr);
		PipelineInterface::GetInstance().OMSetIMGUIRenderTargets(1, false, nullptr);
		//	Has been call during Level:Render()
		//PipelineInterface::GetInstance().SetSRVDescriptorHeaps(FrameContextIndex, 1);
	
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), PipelineInterface::GetInstance().GetCommandList());
		
		PipelineInterface::GetInstance().InsertIMGUIRenderTargetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		PipelineInterface::GetInstance().GetCommandList()->Close();
		PipelineInterface::GetInstance().ExecuteCommandLists();
		
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
