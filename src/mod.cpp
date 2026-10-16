#include "mod.h"
#include "game_string.h"
#include "skins.generated.h"
#include "imgui.h"
#include "MinHook.h"
#include <wincrypt.h>
#include <array>
#include <mutex>
#include <algorithm>
#include <vector>
#include <cctype>
#include <atomic>
#include "skin_category.h"
#include "menu_options.h"
#include "player_model.h"

namespace {
HMODULE module;
std::mutex stateMutex;
std::atomic_bool visible{false};
bool ready = false;
int selection = 0;
int requestedSkin = -1;
int requestedGender = -1;
uint32_t currentGender = 2;
uint32_t originalGender = 2;
#ifdef TQ_SKIN_PREVIEW
ImVec4 groupRects[7];
bool groupOpen[7]{};
#endif
bool requestedRestore = false;
std::string status = "Waiting for a loaded character.";
std::string current;
std::string original;
void* lastPlayer = nullptr;
std::array<bool, 256> keys{};
using GetPlayer = void* (__thiscall*)(void*);
using GetTexture = const GameString* (__thiscall*)(void*);
using SetTexture = void (__thiscall*)(void*, GameString*);
using GetGender = uint32_t (__thiscall*)(void*);
using SetMesh = void (__thiscall*)(void*, const char*, bool);
using CreatePaperDoll = void (__thiscall*)(void*, bool);
using Update = void (__thiscall*)(void*, int);
GetPlayer getPlayer;
GetTexture getTexture;
SetTexture setTexture;
GetGender getGender;
SetMesh setMesh;
CreatePaperDoll createPaperDoll;
Update originalUpdate;

bool Pressed(int key) {
	bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
	bool pressed = down && !keys[key];
	keys[key] = down;
	return pressed;
}

const std::array<SkinMenuNode, 7>& Groups() {
	static const auto groups = BuildSkinGroups();
	return groups;
}

void DrawSkinNode(const SkinMenuNode& node, const char* filter, bool searching, bool filterChanged, int topLevelIndex = -1) {
	const int matches = CountSkinMatches(node, filter);
	if (searching && matches == 0) return;
	if (node.skinIndex >= 0) {
		if (!SkinMatchesSearch(kSkins[node.skinIndex], filter)) return;
		ImGui::PushID(node.skinIndex);
		if (ImGui::Selectable(kSkins[node.skinIndex].label, node.skinIndex == selection)) selection = node.skinIndex;
		ImGui::PopID();
		return;
	}

	if (filterChanged) ImGui::SetNextItemOpen(searching, ImGuiCond_Always);
	ImGui::PushID(node.label.c_str());
	std::string header = node.label + " (" + std::to_string(matches) + ")###skin-group";
	const bool expanded = ImGui::CollapsingHeader(header.c_str());
#ifdef TQ_SKIN_PREVIEW
	if (topLevelIndex >= 0 && topLevelIndex < 7) {
		const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
		groupRects[topLevelIndex] = ImVec4(low.x, low.y, high.x, high.y);
		groupOpen[topLevelIndex] = expanded;
	}
#endif
	if (expanded) {
		for (const auto& child : node.children) DrawSkinNode(child, filter, searching, filterChanged);
	}
	ImGui::PopID();
}

bool ReadTexture(void* player, std::string& out) {
	const GameString* value = getTexture(player);
	if (!value || value->size > 255 || value->size > value->capacity) return false;
	const char* data = value->capacity < 16 ? value->storage.small : value->storage.pointer;
	if (!data) return false;
	// The pointer belongs to the current player, accessed only on GameEngine::Update.
	out.assign(data, value->size);
	return ValidTexture(out);
}

void Apply(void* player, const std::string& path) {
	if (!ValidTexture(path)) { std::lock_guard<std::mutex> lock(stateMutex); status = "Invalid texture path; nothing changed."; return; }
	GameString source = BorrowTexture(path.c_str(), static_cast<uint32_t>(path.size()));
	setTexture(player, &source);
	std::string observed;
	const bool retained = ReadTexture(player, observed) && observed == path;
	std::lock_guard<std::mutex> lock(stateMutex);
	if (retained) {
		current = observed;
		status = "Applied. The game may save this appearance with your character.";
	} else status = "The game did not retain the requested texture.";
}

bool ChangeGender(void* player, uint32_t targetGender, const std::string& priorTexture) {
	const char* mesh = PlayerModelPath(targetGender);
	if (!mesh || !player || !getGender || !setMesh || !createPaperDoll) return false;
	const uint32_t currentGender = getGender(player);
	if (currentGender > kFemaleGender) {
		std::lock_guard<std::mutex> lock(stateMutex);
		status = "The game returned an unknown player model. No change was made.";
		return false;
	}
	if (currentGender == targetGender) {
		std::lock_guard<std::mutex> lock(stateMutex);
		status = targetGender == kFemaleGender ? "Female model is already active." : "Male model is already active.";
		return true;
	}
	// Character::GetGender reads this enum from the verified 2.10.21415 layout.
	// The supported Game.dll hash is checked before this profile can be used.
	*reinterpret_cast<uint32_t*>(static_cast<unsigned char*>(player) + kCharacterGenderOffset) = targetGender;
	setMesh(player, mesh, true);
	createPaperDoll(player, true);
	std::string matchingTexture = TextureForGender(priorTexture, targetGender);
	if (matchingTexture.empty()) matchingTexture = targetGender == kFemaleGender
		? "Creatures\\PC\\Female\\FemalePC01_White.tex"
		: "Creatures\\PC\\Male\\MalePC01_White.tex";
	Apply(player, matchingTexture);
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		status = targetGender == kFemaleGender
			? "Female model applied. Check armor and save behavior in game."
			: "Male model applied. Check armor and save behavior in game.";
	}
	return true;
}

void __fastcall UpdateHook(void* engine, void*, int elapsed) {
	originalUpdate(engine, elapsed);
	std::unique_lock<std::mutex> lock(stateMutex);
	DWORD foregroundPid = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
	const bool focused = foregroundPid == GetCurrentProcessId();
	bool toggle = Pressed(VK_HOME);
	if (focused && toggle) visible = !visible;
	void* player = getPlayer(engine);
	currentGender = player ? getGender(player) : 2;
	if (player != lastPlayer) {
		lastPlayer = player;
		requestedSkin = -1;
		requestedGender = -1;
		requestedRestore = false;
		original.clear();
		current.clear();
		originalGender = 2;
		ready = player && ReadTexture(player, current);
		if (ready) { original = current; originalGender = currentGender; }
		status = ready ? "Choose a skin, then click Apply skin." : "Load a character to change skins.";
	}
	if (!player) { ready = false; return; }
	if (!ready && ReadTexture(player, current)) {
		ready = true;
		original = current;
		originalGender = currentGender;
		status = "Choose a skin, then click Apply skin.";
	}
	// UI requests are consumed only here, never inside the renderer or WndProc.
	int request = requestedSkin;
	bool restore = requestedRestore;
	int genderRequest = restore ? (originalGender <= kFemaleGender ? static_cast<int>(originalGender) : -1) : requestedGender;
	requestedSkin = -1;
	requestedGender = -1;
	requestedRestore = false;
	if (!focused || !ready) return;
	std::string path;
	if (restore && !original.empty()) path = original;
	else if (request >= 0 && request < static_cast<int>(std::size(kSkins))) path = kSkins[request].path;
	if (genderRequest >= 0 && !path.empty()) {
		std::string matchingTexture = TextureForGender(path, static_cast<uint32_t>(genderRequest));
		if (!matchingTexture.empty()) path = matchingTexture;
	}
	const std::string priorTexture = current;
	lock.unlock();
	if (genderRequest >= 0 && genderRequest <= static_cast<int>(kFemaleGender)) ChangeGender(player, static_cast<uint32_t>(genderRequest), priorTexture);
	// Never hold the menu lock while game code loads textures or refreshes rendering.
	if (!path.empty()) Apply(player, path);
}

bool KnownGame(HMODULE game) {
	wchar_t path[MAX_PATH];
	if (!GetModuleFileNameW(game, path, MAX_PATH)) return false;
	HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	HCRYPTPROV provider = 0;
	HCRYPTHASH hash = 0;
	bool ok = CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
	BYTE buffer[65536];
	DWORD read = 0;
	while (ok) {
		if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) { ok = false; break; }
		if (!read) break;
		ok = CryptHashData(hash, buffer, read, 0);
	}
	BYTE digest[32]; DWORD length = sizeof(digest);
	ok = ok && CryptGetHashParam(hash, HP_HASHVAL, digest, &length, 0);
	const BYTE expected[] = {0x75,0x49,0x07,0xea,0xcf,0x55,0x26,0x56,0x94,0x5f,0xf9,0xea,0xf1,0x76,0x36,0x30,0xe1,0x38,0x50,0x65,0x17,0xe9,0x1b,0x69,0x8a,0xb2,0x8a,0x0c,0x31,0x86,0xaa,0x86};
	ok = ok && !memcmp(expected, digest, sizeof(expected));
	if (hash) CryptDestroyHash(hash);
	if (provider) CryptReleaseContext(provider, 0);
	CloseHandle(file);
	return ok;
}

bool KnownModelEngine(HMODULE engine, void* meshSetter) {
	if (!engine) return false;
	auto* base = reinterpret_cast<unsigned char*>(engine);
	auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
	auto* pe = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	const unsigned char signature[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51,0x56};
	return pe->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
		pe->FileHeader.TimeDateStamp == 0x62da9ca0 &&
		pe->OptionalHeader.SizeOfImage == 0x44c000 &&
		reinterpret_cast<unsigned char*>(meshSetter) == base + 0x113890 &&
		memcmp(meshSetter, signature, sizeof(signature)) == 0;
}

DWORD WINAPI Initialize(void*) {
	HMODULE game = nullptr;
	for (int i = 0; i < 120 && !game; ++i) { game = GetModuleHandleW(L"Game.dll"); if (!game) Sleep(500); }
	if (!game || !KnownGame(game)) { Log("Unsupported Game.dll. No hooks or texture changes installed."); return 0; }
	getPlayer = reinterpret_cast<GetPlayer>(GetProcAddress(game, "?GetMainPlayer@GameEngine@GAME@@QBEPAVPlayer@2@XZ"));
	getTexture = reinterpret_cast<GetTexture>(GetProcAddress(game, "?GetPlayerTexture@Player@GAME@@QAEABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ"));
	setTexture = reinterpret_cast<SetTexture>(GetProcAddress(game, "?SetPlayerTexture@Player@GAME@@QAEXAAV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z"));
	getGender = reinterpret_cast<GetGender>(GetProcAddress(game, "?GetGender@Character@GAME@@UBE?AW4Character_Gender@2@XZ"));
	HMODULE engine = GetModuleHandleW(L"Engine.dll");
	setMesh = engine ? reinterpret_cast<SetMesh>(GetProcAddress(engine, "?SetMesh@Actor@GAME@@QAEXPBD_N@Z")) : nullptr;
	createPaperDoll = reinterpret_cast<CreatePaperDoll>(GetProcAddress(game, "?CreatePaperDoll@Player@GAME@@QAEX_N@Z"));
	void* update = reinterpret_cast<void*>(GetProcAddress(game, "?Update@GameEngine@GAME@@QAEXH@Z"));
	if (!getPlayer || !getTexture || !setTexture || !getGender || !setMesh || !createPaperDoll || !update) { Log("Required game exports missing."); return 0; }
	if (!KnownModelEngine(engine, reinterpret_cast<void*>(setMesh))) { Log("Unsupported Engine.dll model function. No hooks enabled."); return 0; }
	if (MH_Initialize() != MH_OK) { Log("MinHook initialization failed."); return 0; }
	if (MH_CreateHook(update, reinterpret_cast<void*>(&UpdateHook), reinterpret_cast<void**>(&originalUpdate)) != MH_OK || !InstallInputHooks() || !InstallGraphicsHooks()) {
		Log("Hook preparation failed. No hooks enabled."); MH_Uninitialize(); return 0;
	}
	if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) { MH_DisableHook(MH_ALL_HOOKS); Log("Hook activation failed."); return 0; }
	Log("Initialized for verified Game.dll. Press Home in game.");
	return 0;
}
}

void Log(const char* message) {
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(module, path, MAX_PATH);
	std::wstring name(path);
	name += L".log";
	HANDLE file = CreateFileW(name.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
	if (file == INVALID_HANDLE_VALUE) return;
	DWORD written;
	WriteFile(file, message, static_cast<DWORD>(strlen(message)), &written, nullptr);
	WriteFile(file, "\r\n", 2, &written, nullptr);
	CloseHandle(file);
}

bool IsMenuVisible() {
	return visible.load();
}

void DrawMenu(float width, float height) {
	static char filter[128]{};
	static std::string previousFilter;
	std::lock_guard<std::mutex> lock(stateMutex);
	if (!visible) return;
	ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(std::min(720.0f, width - 48), std::min(620.0f, height - 48)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(420.0f, width), std::min(300.0f, height)), ImVec2(width, height));
	ImGui::SetNextWindowBgAlpha(0.96f);
	bool open = true;
	bool expanded = ImGui::Begin("Titan Quest | Skin wardrobe", &open, ImGuiWindowFlags_NoSavedSettings);
	if (!open) visible = false;
	if (expanded) {
		ImGui::TextUnformatted("Home: show/hide | Drag the title bar | Resize from a corner");
		ImGui::TextColored(ImVec4(1,0.76f,0.3f,1), "Gameplay input captured. The world keeps running.");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
		const uint32_t gender = currentGender;
		ImGui::Text("Player model: %s", gender == kMaleGender ? "Male" : gender == kFemaleGender ? "Female" : "Unknown");
		ImGui::SameLine();
		ImGui::BeginDisabled(!ready || gender > kFemaleGender);
		if (gender == kMaleGender && ImGui::Button("Experimental: female")) requestedGender = kFemaleGender;
		else if (gender == kFemaleGender && ImGui::Button("Experimental: male")) requestedGender = kMaleGender;
		ImGui::EndDisabled();
		ImGui::TextColored(ImVec4(1,0.76f,0.3f,1), "Back up your character first; check armor and animations after switching.");
		ImGui::TextWrapped("Current: %s", current.empty() ? "No loaded character" : current.c_str());
		ImGui::Separator();
		ImGui::Text("Skin catalogue  |  %d choices", static_cast<int>(std::size(kSkins)));
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##skin-search", "Search names or texture paths...", filter, sizeof(filter));
		const bool filterChanged = previousFilter != filter;
		if (filterChanged) previousFilter = filter;
		const bool searching = filter[0] != '\0';
		ImGui::BeginChild("Skin lists", ImVec2(0, -158), ImGuiChildFlags_Borders);
		const auto& groups = Groups();
		int totalMatches = 0;
		for (int group = 0; group < static_cast<int>(groups.size()); ++group) {
			const int matches = CountSkinMatches(groups[group], filter);
			totalMatches += matches;
			DrawSkinNode(groups[group], filter, searching, filterChanged, group);
		}
		ImGui::EndChild();
		ImGui::Text("%d matching choice%s", totalMatches, totalMatches == 1 ? "" : "s");
		ImGui::Text("Selected: %s", kSkins[selection].label);
		ImGui::TextWrapped("%s", kSkins[selection].path);
		ImGui::BeginDisabled(!ready);
		if (ImGui::Button("Apply selected skin", ImVec2(170, 0))) requestedSkin = selection;
		ImGui::SameLine();
		if (ImGui::Button("Restore starting appearance")) requestedRestore = true;
		ImGui::EndDisabled();
	}
	ImGui::End();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		module = instance;
		DisableThreadLibraryCalls(instance);
		HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
		if (thread) CloseHandle(thread);
	}
	return TRUE;
}

#ifdef TQ_SKIN_PREVIEW
ImVec4 PreviewGroupRect(int group) { return groupRects[group]; }
bool PreviewGroupOpen(int group) { return groupOpen[group]; }
void EnablePreview() {
	visible = true;
	current = kSkins[0].path;
	status = "Renderer test preview - no game or character connected.";
}
#endif
