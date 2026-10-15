#include "mod.h"
#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#include <d3d9.h>
#include <d3d11.h>
#include <mutex>

namespace {
std::recursive_mutex graphicsMutex;
HWND inputWindow = nullptr;
WNDPROC originalWindowProc = nullptr;
ImGuiContext* inputContext = nullptr;
using EndScene = HRESULT (WINAPI*)(IDirect3DDevice9*);
using Reset = HRESULT (WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using Present = HRESULT (WINAPI*)(IDXGISwapChain*, UINT, UINT);
EndScene originalEndScene;
Reset originalReset;
Present originalPresent;
ImGuiContext* context9 = nullptr;
ImGuiContext* context11 = nullptr;
IDirect3DDevice9* device9 = nullptr;
ID3D11Device* device11 = nullptr;
ID3D11DeviceContext* immediate11 = nullptr;
ULONGLONG last9 = 0, last11 = 0;

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	bool consume = false;
	LRESULT result = 0;
	{
		std::lock_guard<std::recursive_mutex> lock(graphicsMutex);
		if (inputContext) {
			ImGuiContext* previous = ImGui::GetCurrentContext();
			ImGui::SetCurrentContext(inputContext);
			// Always deliver releases/focus changes to avoid stuck drag/button state.
			result = ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
			const bool mouseMessage = (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST);
			const bool keyMessage = (message >= WM_KEYFIRST && message <= WM_KEYLAST);
			consume = IsMenuVisible() && (mouseMessage || keyMessage || (message == WM_SETCURSOR && result));
			ImGui::SetCurrentContext(previous);
		}
	}
	if (consume) return message == WM_SETCURSOR ? result : 0;
	return CallWindowProcW(originalWindowProc, window, message, wParam, lParam);
}

bool InitializeInput(HWND window, ImGuiContext* context) {
	if (inputWindow && inputWindow != window) return false;
	if (!ImGui_ImplWin32_Init(window)) return false;
	if (!inputWindow) {
		SetLastError(0);
		auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
		if (!previous) {
			ImGui_ImplWin32_Shutdown();
			Log("Window input hook failed.");
			return false;
		}
		originalWindowProc = previous;
		inputWindow = window;
	}
	inputContext = context;
	return true;
}

void NewFrame(float width, float height, ULONGLONG& last) {
	if (ImGui::GetIO().BackendPlatformUserData) ImGui_ImplWin32_NewFrame();
	auto& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(width, height);
	ULONGLONG now = GetTickCount64();
	io.DeltaTime = last && now > last ? static_cast<float>(now - last) / 1000.0f : 1.0f / 60.0f;
	last = now;
	ImGui::NewFrame();
	DrawMenu(width, height);
	ImGui::Render();
}

ImGuiContext* MakeContext(HWND window) {
	auto* context = ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().LogFilename = nullptr;
	ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
	InitializeInput(window, context);
	ImGui::StyleColorsDark();
	ImGui::GetStyle().WindowRounding = 8;
	return context;
}

HRESULT WINAPI EndSceneHook(IDirect3DDevice9* device) {
	{
		std::lock_guard<std::recursive_mutex> lock(graphicsMutex);
		ImGuiContext* previous = ImGui::GetCurrentContext();
		if (!context9) {
			D3DDEVICE_CREATION_PARAMETERS creation{};
			device->GetCreationParameters(&creation);
			context9 = MakeContext(creation.hFocusWindow);
			device9 = device;
			ImGui_ImplDX9_Init(device);
			Log("DirectX 9 overlay initialized.");
		}
		if (device == device9) {
			ImGui::SetCurrentContext(context9);
			D3DVIEWPORT9 viewport{};
			if (SUCCEEDED(device->GetViewport(&viewport)) && viewport.Width && viewport.Height) {
				ImGui_ImplDX9_NewFrame();
				NewFrame(static_cast<float>(viewport.Width), static_cast<float>(viewport.Height), last9);
				ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			}
		}
		ImGui::SetCurrentContext(previous);
	}
	return originalEndScene(device);
}

HRESULT WINAPI ResetHook(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) {
	std::lock_guard<std::recursive_mutex> lock(graphicsMutex);
	ImGuiContext* previous = ImGui::GetCurrentContext();
	if (context9 && device == device9) {
		ImGui::SetCurrentContext(context9);
		ImGui_ImplDX9_InvalidateDeviceObjects();
	}
	HRESULT result = originalReset(device, parameters);
	// NewFrame recreates resources after a successful reset.
	ImGui::SetCurrentContext(previous);
	return result;
}

HRESULT WINAPI PresentHook(IDXGISwapChain* swap, UINT interval, UINT flags) {
	if (!(flags & DXGI_PRESENT_TEST)) {
		std::lock_guard<std::recursive_mutex> lock(graphicsMutex);
		ImGuiContext* previous = ImGui::GetCurrentContext();
		ID3D11Device* device = nullptr;
		if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device)))) {
			if (!context11) {
				device11 = device;
				device11->AddRef();
				device11->GetImmediateContext(&immediate11);
				DXGI_SWAP_CHAIN_DESC desc{};
				swap->GetDesc(&desc);
				context11 = MakeContext(desc.OutputWindow);
				ImGui_ImplDX11_Init(device11, immediate11);
				Log("DirectX 11 overlay initialized.");
			}
			if (device == device11) {
				ImGui::SetCurrentContext(context11);
				ID3D11Texture2D* backbuffer = nullptr;
				if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer)))) {
					D3D11_TEXTURE2D_DESC desc{};
					backbuffer->GetDesc(&desc);
					ID3D11RenderTargetView* target = nullptr;
					if (SUCCEEDED(device->CreateRenderTargetView(backbuffer, nullptr, &target))) {
						ID3D11RenderTargetView* oldTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
						ID3D11DepthStencilView* oldDepth = nullptr;
						immediate11->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldTargets, &oldDepth);
						immediate11->OMSetRenderTargets(1, &target, nullptr);
						ImGui_ImplDX11_NewFrame();
						NewFrame(static_cast<float>(desc.Width), static_cast<float>(desc.Height), last11);
						ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
						immediate11->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldTargets, oldDepth);
						for (auto* view : oldTargets) if (view) view->Release();
						if (oldDepth) oldDepth->Release();
						target->Release();
					}
					backbuffer->Release();
				}
			}
			device->Release();
		}
		ImGui::SetCurrentContext(previous);
	}
	return originalPresent(swap, interval, flags);
}
}

bool InstallGraphicsHooks() {
	// Obtain COM method addresses from temporary devices, without patching game files.
	WNDCLASSW klass{};
	klass.lpfnWndProc = DefWindowProcW;
	klass.hInstance = GetModuleHandleW(nullptr);
	klass.lpszClassName = L"TQSkinMenuBootstrap";
	if (!RegisterClassW(&klass)) return false;
	HWND window = CreateWindowW(klass.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 64, 64, nullptr, nullptr, klass.hInstance, nullptr);
	if (!window) { UnregisterClassW(klass.lpszClassName, klass.hInstance); return false; }
	bool hooked9 = false, hooked11 = false;
	IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d) {
		D3DPRESENT_PARAMETERS parameters{};
		parameters.Windowed = TRUE;
		parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
		parameters.hDeviceWindow = window;
		IDirect3DDevice9* device = nullptr;
		if (SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device))) {
			auto** vtable = *reinterpret_cast<void***>(device);
			hooked9 = MH_CreateHook(vtable[42], reinterpret_cast<void*>(&EndSceneHook), reinterpret_cast<void**>(&originalEndScene)) == MH_OK;
			if (hooked9 && MH_CreateHook(vtable[16], reinterpret_cast<void*>(&ResetHook), reinterpret_cast<void**>(&originalReset)) != MH_OK) {
				MH_RemoveHook(vtable[42]); hooked9 = false;
			}
			device->Release();
		}
		d3d->Release();
	}
	DXGI_SWAP_CHAIN_DESC desc{};
	desc.BufferCount = 1;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = window;
	desc.SampleDesc.Count = 1;
	desc.Windowed = TRUE;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	IDXGISwapChain* swap = nullptr;
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &swap, &device, nullptr, &context))) {
		auto** vtable = *reinterpret_cast<void***>(swap);
		hooked11 = MH_CreateHook(vtable[8], reinterpret_cast<void*>(&PresentHook), reinterpret_cast<void**>(&originalPresent)) == MH_OK;
		context->Release(); device->Release(); swap->Release();
	}
	DestroyWindow(window);
	UnregisterClassW(klass.lpszClassName, klass.hInstance);
	Log(hooked9 ? "DX9 hooks prepared." : "DX9 unavailable.");
	Log(hooked11 ? "DX11 hook prepared." : "DX11 unavailable.");
	return hooked9 || hooked11;
}
