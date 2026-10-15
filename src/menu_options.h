#pragma once
#include "skin_category.h"
#include "skins.generated.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

inline char FoldSkinSearchChar(char value) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

inline bool SkinTextContains(const char* text, const char* query) {
	if (!text || !query) return false;
	if (!*query) return true;
	for (const char* start = text; *start; ++start) {
		const char* value = start;
		const char* term = query;
		while (*value && *term && FoldSkinSearchChar(*value) == FoldSkinSearchChar(*term)) {
			++value;
			++term;
		}
		if (!*term) return true;
	}
	return false;
}

inline bool SkinMatchesSearch(const Skin& skin, const char* query) {
	return SkinTextContains(skin.label, query) || SkinTextContains(skin.path, query);
}

inline bool SkinLabelLess(const Skin& left, const Skin& right) {
	const char* a = left.label;
	const char* b = right.label;
	while (*a && *b) {
		const char foldedA = FoldSkinSearchChar(*a++);
		const char foldedB = FoldSkinSearchChar(*b++);
		if (foldedA != foldedB) return foldedA < foldedB;
	}
	if (*a != *b) return *a < *b;
	return std::string(left.path) < std::string(right.path);
}

inline std::array<std::vector<int>, 3> BuildSkinGroups() {
	std::array<std::vector<int>, 3> groups;
	for (int i = 0; i < static_cast<int>(std::size(kSkins)); ++i) {
		groups[SkinCategory(kSkins[i].path)].push_back(i);
	}
	for (auto& group : groups) {
		std::sort(group.begin(), group.end(), [](int left, int right) { return SkinLabelLess(kSkins[left], kSkins[right]); });
	}
	return groups;
}
