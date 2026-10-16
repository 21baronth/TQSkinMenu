#include "../src/player_model.h"
#include <cassert>

int main() {
	assert(PlayerModelPath(kMaleGender) != nullptr);
	assert(std::string(PlayerModelPath(kMaleGender)) == "Creatures\\PC\\Male\\MalePC01.mif");
	assert(std::string(PlayerModelPath(kFemaleGender)) == "Creatures\\PC\\Female\\FemalePC01.mif");
	assert(PlayerModelPath(2) == nullptr);
	assert(TextureForGender("Creatures\\PC\\Male\\MalePC01_White.tex", kFemaleGender) == "Creatures\\PC\\Female\\FemalePC01_White.tex");
	assert(TextureForGender("Creatures\\PC\\Female\\FemalePC01_Tan.tex", kMaleGender) == "Creatures\\PC\\Male\\MalePC01_Tan.tex");
	assert(TextureForGender("Creatures\\NPC\\Tyr\\Tyr.tex", kFemaleGender).empty());
}
