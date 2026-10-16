#pragma once
#include <cstdint>
#include <string>
#include <algorithm>
#include <cctype>

constexpr uint32_t kMaleGender = 0;
constexpr uint32_t kFemaleGender = 1;
constexpr uint32_t kCharacterGenderOffset = 0xC1C;

inline const char* PlayerModelPath(uint32_t gender) {
	if (gender == kMaleGender) return "Creatures\\PC\\Male\\MalePC01.mif";
	if (gender == kFemaleGender) return "Creatures\\PC\\Female\\FemalePC01.mif";
	return nullptr;
}

inline std::string TextureForGender(const std::string& texture, uint32_t gender) {
	const char* marker = "\\Male\\";
	const char* replacement = "\\Female\\";
	const char* name = "MalePC01";
	const char* replacementName = "FemalePC01";
	if (gender == kMaleGender) {
		marker = "\\Female\\";
		replacement = "\\Male\\";
		name = "FemalePC01";
		replacementName = "MalePC01";
	}
	std::string result = texture;
	auto position = result.find(marker);
	if (position == std::string::npos) return {};
	result.replace(position, std::char_traits<char>::length(marker), replacement);
	position = result.find(name);
	if (position == std::string::npos) return {};
	result.replace(position, std::char_traits<char>::length(name), replacementName);
	return result;
}
