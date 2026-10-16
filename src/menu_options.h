#pragma once
#include "skins.generated.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

struct SkinMenuNode {
	std::string label;
	int skinIndex = -1;
	std::vector<SkinMenuNode> children;
};

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

inline bool SkinTextStartsWith(const char* text, const char* prefix) {
	if (!text || !prefix) return false;
	while (*prefix) {
		if (!*text || FoldSkinSearchChar(*text++) != FoldSkinSearchChar(*prefix++)) return false;
	}
	return true;
}

inline bool SkinMatchesSearch(const Skin& skin, const char* query) {
	return SkinTextContains(skin.label, query) || SkinTextContains(skin.path, query);
}

inline bool SkinTextLess(const char* left, const char* right) {
	while (*left && *right) {
		const char foldedLeft = FoldSkinSearchChar(*left++);
		const char foldedRight = FoldSkinSearchChar(*right++);
		if (foldedLeft != foldedRight) return foldedLeft < foldedRight;
	}
	return *left < *right;
}

inline int SkinSectionIndex(const char* label) {
	if (SkinTextStartsWith(label, "Greek ")) {
		if (SkinTextStartsWith(label, "Greek Guard")) return 6;
		return 0;
	}
	if (SkinTextStartsWith(label, "Egyptian ")) {
		if (SkinTextContains(label, "Egyptian Guard")) return 6;
		return 1;
	}
	if (SkinTextStartsWith(label, "Asian ")) return 2;
	if (SkinTextStartsWith(label, "Loincloth ")) return 3;
	if (SkinTextStartsWith(label, "Chthonic ")) return 4;
	if (SkinTextStartsWith(label, "Northern ")) return 5;
	return 6;
}

inline int SkinGenderIndex(const char* label) {
	if (!label) return -1;
	const char* firstSpace = std::strchr(label, ' ');
	if (firstSpace && (FoldSkinSearchChar(firstSpace[1]) == 'f' || FoldSkinSearchChar(firstSpace[1]) == 'm') && firstSpace[2] == ' ') {
		return FoldSkinSearchChar(firstSpace[1]) == 'f' ? 0 : 1;
	}
	const std::string value(label ? label : "");
	if (value.size() >= 2 && value[value.size() - 2] == ' ' && FoldSkinSearchChar(value.back()) == 'f') return 0;
	if (value.size() >= 2 && value[value.size() - 2] == ' ' && FoldSkinSearchChar(value.back()) == 'm') return 1;
	return -1;
}

inline int FindSkinGroup(const SkinMenuNode& parent, const char* label) {
	for (int i = 0; i < static_cast<int>(parent.children.size()); ++i) {
		if (parent.children[i].label == label) return i;
	}
	return -1;
}

inline void SortSkinMenuNodes(std::vector<SkinMenuNode>& nodes) {
	const auto key = [](const SkinMenuNode& node) -> const char* {
		if (node.skinIndex >= 0) return kSkins[node.skinIndex].label;
		if (!node.children.empty()) return kSkins[node.children.front().skinIndex].label;
		return node.label.c_str();
	};
	std::stable_sort(nodes.begin(), nodes.end(), [&](const SkinMenuNode& left, const SkinMenuNode& right) {
		return SkinTextLess(key(left), key(right));
	});
}

inline std::array<SkinMenuNode, 7> BuildSkinGroups() {
	std::array<SkinMenuNode, 7> sections = {{
		{"GREEK"}, {"EGYPT"}, {"ASIAN"}, {"LOINCLOTH"}, {"CHTHON"}, {"NORTHERN"}, {"BONUS"}
	}};
	for (auto& section : sections) {
		section.children.push_back({"Female"});
		section.children.push_back({"Male"});
	}

	for (int index = 0; index < static_cast<int>(std::size(kSkins)); ++index) {
		const Skin& skin = kSkins[index];
		const int sectionIndex = SkinSectionIndex(skin.label);
		const int genderIndex = SkinGenderIndex(skin.label);
		if (genderIndex < 0) continue;
		SkinMenuNode& gender = sections[sectionIndex].children[genderIndex];
		if (sectionIndex == 6 && genderIndex == 1 && SkinTextContains(skin.label, "Egyptian Guard")) {
			int groupIndex = FindSkinGroup(gender, "Egyptian Guard");
			if (groupIndex < 0) {
				gender.children.push_back({"Egyptian Guard"});
				groupIndex = static_cast<int>(gender.children.size()) - 1;
			}
			gender.children[groupIndex].children.push_back({skin.label, index});
		} else if (sectionIndex == 6 && genderIndex == 1 && SkinTextContains(skin.label, "M Chinese Guard")) {
			int groupIndex = FindSkinGroup(gender, "Chinese Guard");
			if (groupIndex < 0) {
				gender.children.push_back({"Chinese Guard"});
				groupIndex = static_cast<int>(gender.children.size()) - 1;
			}
			gender.children[groupIndex].children.push_back({skin.label, index});
		} else {
			gender.children.push_back({skin.label, index});
		}
	}

	for (auto& section : sections) {
		for (auto& gender : section.children) {
			for (auto& subgroup : gender.children) SortSkinMenuNodes(subgroup.children);
			SortSkinMenuNodes(gender.children);
		}
	}
	return sections;
}

inline int CountSkinMatches(const SkinMenuNode& node, const char* query) {
	if (node.skinIndex >= 0) return SkinMatchesSearch(kSkins[node.skinIndex], query) ? 1 : 0;
	int count = 0;
	for (const auto& child : node.children) count += CountSkinMatches(child, query);
	return count;
}
