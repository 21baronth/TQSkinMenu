#include "../src/menu_options.h"
#include <cassert>
#include <iostream>

int main() {
	const auto groups = BuildSkinGroups();
	int count = 0;
	for (const auto& group : groups) {
		count += static_cast<int>(group.size());
		for (size_t i = 1; i < group.size(); ++i) {
			assert(!SkinLabelLess(kSkins[group[i]], kSkins[group[i - 1]]));
		}
	}
	assert(count == static_cast<int>(std::size(kSkins)));
	assert(SkinMatchesSearch(kSkins[0], "greek m white"));
	assert(SkinMatchesSearch(kSkins[0], "male\\malePC01_white.tex"));
	assert(!SkinMatchesSearch(kSkins[0], "femalePC01"));
	assert(SkinMatchesSearch(kSkins[0], ""));
	std::cout << "Passed alphabetical skin grouping and case-insensitive name/path search across " << count << " choices.\n";
}
