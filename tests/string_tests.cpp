#include "../src/game_string.h"
#include "../src/skins.generated.h"
#include "../src/skin_category.h"
#include <cassert>
#include <iostream>

int main() {
	assert(SkinCategory("Creatures\\PC\\Female\\skin.tex") == 1);
	assert(SkinCategory("creatures/pc/MALE/skin.tex") == 0);
	assert(SkinCategory("XPack2/creatures/npc/asgard/tyr/tyr01.tex") == 2);
	// Catch SSO boundary mistakes and accidental native C++ ABI use.
	for (int length : {5, 15, 16, 17, 255}) {
		std::string text(length - 4, 'a');
		text += ".tex";
		auto source = BorrowTexture(text.c_str(), static_cast<uint32_t>(text.size()));
		const char* bytes = source.capacity < 16 ? source.storage.small : source.storage.pointer;
		assert(source.size == text.size());
		assert(source.size <= source.capacity);
		assert(std::string(bytes, source.size) == text);
		assert(bytes[source.size] == '\0');
		assert(ValidTexture(text));
	}
	assert(!ValidTexture("../outside.tex"));
	assert(!ValidTexture("C:/outside.tex"));
	assert(!ValidTexture("Creatures/PC/skin.msh"));
	assert(!ValidTexture(std::string(256, 'a') + ".tex"));
	assert(!ValidTexture(std::string("skin\0bad.tex", 12)));
	for (const auto& skin : kSkins) assert(ValidTexture(skin.path));
	std::cout << "Passed string ABI boundaries, invalid input, and all imported texture paths.\n";
}
