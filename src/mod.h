#pragma once
#include <windows.h>
#include <string>

void Log(const char* message);
void DrawMenu(float width, float height);
bool IsMenuVisible();
bool InstallGraphicsHooks();
bool InstallInputHooks();
