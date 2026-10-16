#include "../src/menu_options.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

const char* SortLabel(const SkinMenuNode& node) {
	if (node.skinIndex >= 0) return kSkins[node.skinIndex].label;
	assert(!node.children.empty());
	return kSkins[node.children.front().skinIndex].label;
}

void CollectSkinIndices(const SkinMenuNode& node, std::vector<int>& indices) {
	if (node.skinIndex >= 0) {
		assert(node.children.empty());
		indices.push_back(node.skinIndex);
		return;
	}
	for (size_t i = 1; i < node.children.size(); ++i) {
		assert(!SkinTextLess(SortLabel(node.children[i]), SortLabel(node.children[i - 1])));
	}
	for (const auto& child : node.children) CollectSkinIndices(child, indices);
}

int main() {
	const auto groups = BuildSkinGroups();
	const std::array<const char*, 7> expectedSections = {"GREEK", "EGYPT", "ASIAN", "LOINCLOTH", "CHTHON", "NORTHERN", "BONUS"};
	std::vector<int> indices;
	for (size_t i = 0; i < groups.size(); ++i) {
		assert(groups[i].label == expectedSections[i]);
		assert(groups[i].children.size() == 2);
		assert(groups[i].children[0].label == "Female");
		assert(groups[i].children[1].label == "Male");
		CollectSkinIndices(groups[i], indices);
	}
	assert(indices.size() == std::size(kSkins));
	std::array<bool, std::size(kSkins)> seen{};
	for (int index : indices) {
		assert(index >= 0 && index < static_cast<int>(seen.size()));
		assert(!seen[index]);
		seen[index] = true;
	}
	for (bool found : seen) assert(found);
	assert(CountSkinMatches(groups[0], "greek ") == 32);
	assert(CountSkinMatches(groups[6], "") == 25);
	assert(CountSkinMatches(groups[6], "tyr") == 1);
	assert(CountSkinMatches(groups[6], "not-a-skin") == 0);
	assert(SkinMatchesSearch(kSkins[0], "male\\malePC01_white.tex"));
	assert(SkinMatchesSearch(kSkins[0], ""));
	assert(!SkinMatchesSearch(kSkins[0], "femalePC01"));

	const SkinMenuNode& bonusMale = groups[6].children[1];
	assert(FindSkinGroup(bonusMale, "Egyptian Guard") >= 0);
	assert(FindSkinGroup(bonusMale, "Chinese Guard") >= 0);
	std::cout << "Passed regional, gender, and nested bonus grouping with searchable, alphabetized lists across " << indices.size() << " choices.\n";
}
