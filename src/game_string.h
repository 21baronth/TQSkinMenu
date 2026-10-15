#pragma once
#include <cstdint>
#include <cstring>
#include <string>

// MSVC x86 release std::string layout, verified in this Game.dll's getter/setter.
// Never pass the compiler's native std::string across the game ABI.
struct GameString {
	union { char small[16]; const char* pointer; } storage{};
	uint32_t size = 0;
	uint32_t capacity = 15;
};
static_assert(sizeof(GameString) == 24);

inline bool ValidTexture(const std::string& path) {
	if (path.size() < 5 || path.size() > 255 || path.find("..") != std::string::npos) return false;
	for (unsigned char c : path) if (c < 32 || c > 126 || c == ':') return false;
	std::string suffix = path.substr(path.size() - 4);
	for (char& c : suffix) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	return suffix == ".tex";
}

// Borrowed, read-only source; SetPlayerTexture copies into game-owned storage.
// Buffer must remain alive until the setter returns. No cross-heap ownership.
inline GameString BorrowTexture(const char* buffer, uint32_t size) {
	GameString result;
	if (size < 16) {
		std::memcpy(result.storage.small, buffer, size);
	} else {
		result.storage.pointer = buffer;
		result.capacity = size;
	}
	result.size = size;
	return result;
}
