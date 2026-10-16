#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <cstdio>
#include <vector>
#include <cassert>
#include "imgui.h"
#include "imgui_internal.h"
#include "MinHook.h"
#include "../src/mod.h"

void EnablePreview();
ImVec4 PreviewGroupRect(int group);
bool PreviewGroupOpen(int group);

int TestInteraction() {
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.DisplaySize = ImVec2(960, 720);
	io.DeltaTime = 1.0f / 60;
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	unsigned char* pixels; int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	EnablePreview();
	auto frame = [&](float x, float y, bool down) {
		io.AddMousePosEvent(x, y);
		io.AddMouseButtonEvent(0, down);
		ImGui::NewFrame(); DrawMenu(960, 720); ImGui::Render();
	};
	frame(-100, -100, false); frame(-100, -100, false);
	for (int group = 0; group < 7; ++group) {
		assert(!PreviewGroupOpen(group));
		auto box = PreviewGroupRect(group);
		for (int toggle = 0; toggle < 2; ++toggle) {
			frame(box.x + 20, box.y + 8, false);
			frame(box.x + 20, box.y + 8, true);
			frame(box.x + 20, box.y + 8, false);
			assert(PreviewGroupOpen(group) == (toggle == 0));
		}
	}
	auto* window = ImGui::FindWindowByName("Titan Quest | Skin wardrobe");
	ImVec2 before = window->Pos;
	frame(before.x + 160, before.y + 10, false);
	frame(before.x + 160, before.y + 10, true);
	frame(before.x + 210, before.y + 40, true);
	frame(before.x + 210, before.y + 40, false);
	assert(window->Pos.x > before.x + 20 && window->Pos.y > before.y + 10);
	ImVec2 oldSize = window->Size;
	float cornerX = window->Pos.x + window->Size.x - 3;
	float cornerY = window->Pos.y + window->Size.y - 3;
	frame(cornerX, cornerY, false);
	frame(cornerX, cornerY, true);
	frame(cornerX - 80, cornerY - 60, true);
	frame(cornerX - 80, cornerY - 60, false);
	assert(window->Size.x < oldSize.x - 40 && window->Size.y < oldSize.y - 30);
	ImGui::DestroyContext();
	puts("Passed mouse dragging, resizing, and all seven collapsible skin sections.");
	return 0;
}

bool Save(const char* path, const unsigned char* bytes, unsigned pitch, bool bgra) {
	FILE* file = fopen(path, "wb");
	if (!file) return false;
	fprintf(file, "P6\n960 720\n255\n");
	unsigned bright = 0;
	for (unsigned y = 0; y < 720; ++y) for (unsigned x = 0; x < 960; ++x) {
		const unsigned char* pixel = bytes + y * pitch + x * 4;
		unsigned char rgb[] = {pixel[bgra ? 2 : 0], pixel[1], pixel[bgra ? 0 : 2]};
		fwrite(rgb, 1, 3, file);
		if (rgb[0] > 120 && rgb[1] > 120) ++bright;
	}
	fclose(file);
	printf("%s: %u bright pixels\n", path, bright);
	return bright > 1000;
}

int main(int argc, char** argv) {
	if (argc < 3) return 2;
	if (!strcmp(argv[1], "ui")) return TestInteraction();
	WNDCLASSW klass{};
	klass.lpfnWndProc = DefWindowProcW;
	klass.hInstance = GetModuleHandleW(nullptr);
	klass.lpszClassName = L"TQSkinSmoke";
	RegisterClassW(&klass);
	HWND window = CreateWindowW(klass.lpszClassName, L"Renderer test", WS_OVERLAPPEDWINDOW, 0, 0, 960, 720, nullptr, nullptr, klass.hInstance, nullptr);
	if (!window) return 3;
	EnablePreview();
	if (MH_Initialize() != MH_OK || !InstallGraphicsHooks() || MH_EnableHook(MH_ALL_HOOKS) != MH_OK) return 4;
	bool ok = false;
	if (!strcmp(argv[1], "dx9")) {
		IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
		D3DPRESENT_PARAMETERS parameters{};
		parameters.Windowed = TRUE;
		parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
		parameters.BackBufferWidth = 960; parameters.BackBufferHeight = 720;
		parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
		parameters.hDeviceWindow = window;
		IDirect3DDevice9* device = nullptr;
		if (!d3d || FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device))) return 5;
		device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff101820, 1, 0);
		device->BeginScene(); device->EndScene();
		IDirect3DSurface9 *target = nullptr, *copy = nullptr;
		device->GetRenderTarget(0, &target);
		device->CreateOffscreenPlainSurface(960, 720, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &copy, nullptr);
		if (copy && SUCCEEDED(device->GetRenderTargetData(target, copy))) {
			D3DLOCKED_RECT locked{};
			if (SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
				ok = Save(argv[2], static_cast<unsigned char*>(locked.pBits), locked.Pitch, true);
				copy->UnlockRect();
			}
		}
		if (copy) copy->Release(); if (target) target->Release();
		// Exercise lost-device resource invalidation and recovery as well.
		if (FAILED(device->Reset(&parameters))) ok = false;
		device->BeginScene(); device->EndScene();
		device->Release(); d3d->Release();
	} else {
		DXGI_SWAP_CHAIN_DESC desc{};
		desc.BufferCount = 1; desc.BufferDesc.Width = 960; desc.BufferDesc.Height = 720;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.OutputWindow = window; desc.SampleDesc.Count = 1; desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		IDXGISwapChain* swap = nullptr;
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
		if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &swap, &device, nullptr, &context))) return 6;
		ID3D11Texture2D* buffer = nullptr;
		swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&buffer));
		ID3D11RenderTargetView* target = nullptr;
		device->CreateRenderTargetView(buffer, nullptr, &target);
		float color[] = {0.04f,0.06f,0.08f,1}; context->ClearRenderTargetView(target, color);
		swap->Present(0, 0);
		D3D11_TEXTURE2D_DESC staging{}; buffer->GetDesc(&staging);
		staging.Usage = D3D11_USAGE_STAGING; staging.BindFlags = 0; staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ; staging.MiscFlags = 0;
		ID3D11Texture2D* copy = nullptr;
		if (SUCCEEDED(device->CreateTexture2D(&staging, nullptr, &copy))) {
			context->CopyResource(copy, buffer);
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (SUCCEEDED(context->Map(copy, 0, D3D11_MAP_READ, 0, &mapped))) {
				ok = Save(argv[2], static_cast<unsigned char*>(mapped.pData), mapped.RowPitch, false);
				context->Unmap(copy, 0);
			}
			copy->Release();
		}
		target->Release(); buffer->Release();
		if (FAILED(swap->ResizeBuffers(0, 960, 720, DXGI_FORMAT_UNKNOWN, 0))) ok = false;
		swap->Present(0, 0);
		context->Release(); device->Release(); swap->Release();
	}
	MH_DisableHook(MH_ALL_HOOKS);
	DestroyWindow(window);
	return ok ? 0 : 7;
}
