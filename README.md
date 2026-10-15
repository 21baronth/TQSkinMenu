# TQ Skin Menu — experimental ASI for Titan Quest Anniversary Edition

A draggable, resizable in-game wardrobe, containing 217 unique texture choices imported from `characterCreationKit6.CT`. Cheat Engine is not required. Only the **Player Texture** dropdown was imported; no CT scripts, attributes, currency, or other cheats are executed.

## Credits and source attribution

The texture catalogue is derived from **Character Creation Kit v6** by **Endymion**. This project imports only the texture names and paths from its Player Texture dropdown; it does not include the CT file or execute its scripts. See the original [Titan Quest Fans thread](https://titanquestfans.net/index.php?topic=1877.0) and [Nexus Mods page](https://www.nexusmods.com/titanquestanniversaryedition/mods/88). The Nexus permission notes allow modified releases provided Endymion is credited as the original creator.

## Install

1. Close Titan Quest. Back up the character save you intend to test: appearance changes can be saved by the game.
2. Extract the packaged `TQSkinMenu.zip`. Copy **TQSkinMenu.asi** and **winmm.dll** from its `TQSkinMenu` folder beside **TQ.exe**. For the installation inspected here, this is `D:\SteamLibrary\steamapps\common\Titan Quest Anniversary Edition`.
3. If that folder already contains `winmm.dll`, keep the existing file. If it is already an ASI loader, add only `TQSkinMenu.asi`; otherwise resolve the loader conflict before installing. Do not overwrite another mod's proxy DLL.
4. Start the game normally, load a character, and press **Home**.

The supplied `winmm.dll` is the **32-bit Ultimate ASI Loader**, downloaded from [its upstream release](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases). Titan Quest's inspected executable imports WINMM directly. The `.asi` is also 32-bit. No game binaries or save files are patched by the installer: installation is a manual copy of these two new files.

## Controls

| Control | Action |
| --- | --- |
| Home | Open / close wardrobe |
| Title bar | Drag to move; double-click to collapse the whole window |
| Window corner / border | Drag to resize |
| Search field | Filter by skin name or texture path |
| Male / Female / Extra headers | Expand or collapse each alphabetized texture list; counts show the current matches |
| Texture row | Select a texture |
| Apply skin button | Apply selected texture |
| Restore starting appearance button | Restore the appearance captured when this character loaded |
| X in the title bar | Close wardrobe |

Lists start collapsed and scroll inside the window. Search opens the matching groups and filters names and paths case-insensitively; clear the search to return to the full catalogue. The groups show match counts and sort skins alphabetically by display name. The footer keeps the selected skin and its resource path visible while browsing. Position, size, and expanded lists are remembered during the session. Male/Female use the texture's PC resource folder; NPC textures (including Tyr) and other paths are under Extra. Tab no longer changes categories.

The wardrobe captures gameplay mouse/keyboard input for its entire open duration. Windows events still reach ImGui, while Titan Quest's native input dispatcher runs with its input-disabled flag temporarily set. Device events continue to drain, and gameplay input resumes after the closing click/key is released. The original disable state is restored after each input pass, preserving cutscene/game-system restrictions. The world is **not paused**; use the wardrobe somewhere safe. F8 is left to Titan Quest's native HUD toggle.

## Compatibility and limits

- Targets the installed **32-bit AE 2.10.21415 Game.dll** inspected on 2026-10-15 (Steam installation reported build 19471007). DirectX 9 and DirectX 11 overlay backends are included.
- Exact supported Game.dll SHA-256: `754907EACF552656945FF9EAF1763630E138506517E91B698AB28A0C3186AA86`.
- An unknown or modified Game.dll is rejected **before hooks are enabled**. Supporting another build requires checking its ABI and updating the compatibility profile, not disabling the check.
- These are texture swaps, not body/mesh/gender replacements. A female or NPC texture on a male player mesh can look incorrect. The table includes expansion textures; resource availability and visual fit have not been checked for every entry.
- The game setter also derives the corresponding bump texture name and marks the appearance dirty. The menu uses this function so the game owns its string allocation and refresh behavior.
- Restore is a session operation. After quitting and reloading, the newly saved appearance becomes the session's starting appearance. Uninstalling the mod does not revert saved appearances.
- Do not unload the ASI while the game is running. Device reset and swap-chain resize are supported; full graphics-device replacement may require restarting the game.

## Verification status

**Experimental build.** The user opened the original menu in Titan Quest and selected Tyr; the reported missing HUD was traced to the original F8 shortcut. The mouse interface and native input capture still need live in-game verification.

Completed checks:

- Inspected exported getter, setter, main-player lookup, and update function in the installed game binary; verified the x86 MSVC string layout and calling convention.
- Compiled the x86 ASI with statically linked C++ runtime.
- Passed tests for string inline/heap boundaries, invalid paths, and all 217 imported paths.
- Passed three CT parser tests, including ignoring scripts and deduplicating paths.
- Rendered the actual menu through hooked DX9 EndScene and DX11 Present in standalone test programs, with framebuffer readback and visual inspection.
- Exercised DX9 Reset and DX11 ResizeBuffers in those programs.
- Passed simulated mouse tests for title-bar dragging, corner resizing, and independently expanding/collapsing all three lists.
- Passed alphabetical grouping and case-insensitive catalogue search tests across all 217 choices.
- Passed input-capture tests for closing-button holds, release draining, and restoring both previously enabled and disabled input states (including exception cleanup).

Still needs an in-game check: Home opening, selecting/applying a texture, visible refresh, restore, switching characters, and save/reload behavior. Start with a normal PC texture matching the character's mesh. Renderer smoke tests do not prove the game integration works.

## Troubleshooting / uninstall

The mod appends diagnostics beside itself as `TQSkinMenu.asi.log`. If Home does nothing, look for `Initialized for verified Game.dll` and a DirectX overlay initialization message. `Unsupported Game.dll` means this package intentionally did not activate. No log usually means the ASI loader did not load the plugin or the folder was not writable.

To uninstall, close the game and delete `TQSkinMenu.asi` and its log. Remove the bundled `winmm.dll` only if you installed it for this mod and no other ASI mods need it. Restore appearance in game before uninstalling if desired.

## Build from source

In PowerShell, run `bootstrap.ps1` once to download the pinned LLVM-MinGW, Dear ImGui, and MinHook dependencies into this folder, then run:

```powershell
.\build.ps1
.\build.ps1 -GraphicsTests
python -m unittest discover -s tests -p 'test_*.py'
```

The build produces `build\TQSkinMenu.asi`. Graphics tests need a working Windows graphics driver and create hidden temporary windows; their menu framebuffers are written to `build\dx9.ppm` and `build\dx11.ppm`.

The imported catalogue is checked in, so the original CT is not needed to build. To re-import:

```powershell
python .\import_table.py 'C:\path\characterCreationKit6.CT'
```

The native mod is separate from the TQVaultAE .NET solution. C++ uses tab indentation and K&R braces; the repository's C#-specific namespace and DI rules do not apply.

Implementation: MinHook intercepts `GAME::GameEngine::Update(int)` and processes requests after the original update, on the game thread. It obtains the current player through `GetMainPlayer`, reads `GetPlayerTexture`, and calls `SetPlayerTexture` with a borrowed MSVC-layout string. The setter copies the bytes into game-managed storage. The mod never writes to the CT's hard-coded memory offsets or manually edits string capacity/length in the player.

Dependencies: [Dear ImGui 1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b), [MinHook 1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4), [LLVM-MinGW 20261006](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006), and [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader). Dependency licenses accompany the binary package. Texture names/paths originate from the user's supplied CT; game assets and binaries are not redistributed.

Native input capture verifies Engine.dll timestamp, image size, export locations, and the DisableInput/ProcessUserInput instruction signatures before enabling hooks. It scopes DisableInput to ProcessUserInput, rather than bypassing polling or leaving a persistent engine flag set. Unsupported input layouts prevent activation and are recorded in the log.
