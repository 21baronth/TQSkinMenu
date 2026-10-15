#include "mod.h"
#include "input_capture.h"
#include "MinHook.h"
#include <cstring>

namespace {
using ProcessInput = void (__thiscall*)(void*);
using DisableInput = void (__thiscall*)(void*, bool);
ProcessInput originalProcessInput;
DisableInput disableInput;
void** engineGlobal;
InputCaptureGate gate;

bool ControlsHeld() {
	for (int key = 1; key < 256; ++key) {
		if (GetAsyncKeyState(key) & 0x8000) return true;
	}
	return false;
}

void __fastcall ProcessInputHook(void* engine, void*) {
	const bool open = IsMenuVisible();
	if (!gate.Capture(open, !open && ControlsHeld()) || *engineGlobal != engine) {
		originalProcessInput(engine);
		return;
	}
	// Verified against DisableInput's exact machine code below. Preserve the
	// preexisting flag: a cutscene or another game system may have disabled it.
	bool wasDisabled = *(static_cast<unsigned char*>(engine) + 0x0c) != 0;
	ProcessWithInputBlocked(wasDisabled,
		[engine](bool value) { disableInput(engine, value); },
		[engine] { originalProcessInput(engine); });
}
}

bool InstallInputHooks() {
	HMODULE engine = GetModuleHandleW(L"Engine.dll");
	if (!engine) return false;
	auto* base = reinterpret_cast<unsigned char*>(engine);
	auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
	auto* pe = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	if (pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 || pe->FileHeader.TimeDateStamp != 0x62da9ca0 || pe->OptionalHeader.SizeOfImage != 0x44c000) {
		Log("Unrecognized Engine.dll input layout. Mod not activated.");
		return false;
	}
	auto* process = reinterpret_cast<void*>(GetProcAddress(engine, "?ProcessUserInput@Engine@GAME@@QAEXXZ"));
	disableInput = reinterpret_cast<DisableInput>(GetProcAddress(engine, "?DisableInput@Engine@GAME@@QAEX_N@Z"));
	engineGlobal = reinterpret_cast<void**>(GetProcAddress(engine, "?gEngine@GAME@@3PAVEngine@1@A"));
	const unsigned char setter[] = {0x8a,0x44,0x24,0x04,0x88,0x41,0x0c,0xc2,0x04,0x00};
	const unsigned char prologue[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x6a,0xff};
	if (process != base + 0x141ef0 || reinterpret_cast<void*>(disableInput) != base + 0x145d90 || reinterpret_cast<void*>(engineGlobal) != base + 0x3743f0 ||
		memcmp(reinterpret_cast<void*>(disableInput), setter, sizeof(setter)) || memcmp(process, prologue, sizeof(prologue))) {
		Log("Engine input signatures differ. Mod not activated.");
		return false;
	}
	if (MH_CreateHook(process, reinterpret_cast<void*>(&ProcessInputHook), reinterpret_cast<void**>(&originalProcessInput)) != MH_OK) return false;
	Log("Native input capture prepared: gameplay input blocked while wardrobe is open.");
	return true;
}
