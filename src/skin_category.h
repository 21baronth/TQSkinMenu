#pragma once
#include <string>
#include <algorithm>
#include <cctype>

// Character PC paths belong to Male/Female; NPC and other paths stay in Extra.
inline int SkinCategory(std::string path) {
	std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	std::replace(path.begin(), path.end(), '\\', '/');
	if (path.find("/female/") != std::string::npos) return 1;
	if (path.find("/male/") != std::string::npos) return 0;
	return 2;
}
