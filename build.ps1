param([string]$Compiler = "$PSScriptRoot\tools\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-clang++.exe", [switch]$GraphicsTests)
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $Compiler)) { throw 'Compiler missing. Run bootstrap.ps1 first or supply -Compiler.' }
$imgui = "$PSScriptRoot\vendor\imgui-1.91.9b"
$minhook = "$PSScriptRoot\vendor\minhook-1.3.4"
$out = "$PSScriptRoot\build"
New-Item -ItemType Directory -Force $out | Out-Null
$common = @('-O2', '-static', '-std=c++17', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX', '-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS', "-I$imgui", "-I$minhook\include")
$sources = @("$PSScriptRoot\src\mod.cpp", "$PSScriptRoot\src\input.cpp", "$PSScriptRoot\src\graphics.cpp", "$imgui\imgui.cpp", "$imgui\imgui_draw.cpp", "$imgui\imgui_tables.cpp", "$imgui\imgui_widgets.cpp", "$imgui\backends\imgui_impl_dx9.cpp", "$imgui\backends\imgui_impl_dx11.cpp", "$imgui\backends\imgui_impl_win32.cpp")
$objects = @()
foreach ($source in @('buffer.c', 'hook.c', 'trampoline.c', 'hde\hde32.c')) {
	$object = Join-Path $out ([IO.Path]::GetFileNameWithoutExtension($source) + '.o')
	& $Compiler '-x' 'c' '-O2' '-c' "$minhook\src\$source" '-o' $object
	if ($LASTEXITCODE) { throw "Compilation failed: $source" }
	$objects += $object
}
& $Compiler @common '-shared' @sources @objects '-ld3d9' '-ld3d11' '-ldxgi' '-ld3dcompiler' '-ladvapi32' '-luser32' '-lgdi32' '-ldwmapi' '-o' "$out\TQSkinMenu.asi"
if ($LASTEXITCODE) { throw 'ASI compilation failed' }
& $Compiler @common "$PSScriptRoot\tests\string_tests.cpp" '-o' "$out\string_tests.exe"
if ($LASTEXITCODE) { throw 'Test compilation failed' }
& "$out\string_tests.exe"
if ($LASTEXITCODE) { throw 'String ABI tests failed' }
& $Compiler @common "$PSScriptRoot\tests\input_tests.cpp" '-o' "$out\input_tests.exe"
if ($LASTEXITCODE) { throw 'Input test compilation failed' }
& "$out\input_tests.exe"
if ($LASTEXITCODE) { throw 'Input capture tests failed' }
& $Compiler @common "$PSScriptRoot\tests\menu_options_tests.cpp" '-o' "$out\menu_options_tests.exe"
if ($LASTEXITCODE) { throw 'Menu organization test compilation failed' }
& "$out\menu_options_tests.exe"
if ($LASTEXITCODE) { throw 'Menu organization tests failed' }
Get-FileHash "$out\TQSkinMenu.asi" -Algorithm SHA256
if ($GraphicsTests) {
	& $Compiler @common '-DTQ_SKIN_PREVIEW' "$PSScriptRoot\tests\graphics_smoke.cpp" @sources @objects '-ld3d9' '-ld3d11' '-ldxgi' '-ld3dcompiler' '-ladvapi32' '-luser32' '-lgdi32' '-ldwmapi' '-o' "$out\graphics_smoke.exe"
	if ($LASTEXITCODE) { throw 'Graphics test compilation failed' }
	foreach ($api in @('ui', 'dx9', 'dx11')) {
		& "$out\graphics_smoke.exe" $api "$out\$api.ppm"
		if ($LASTEXITCODE) { throw "$api graphics smoke test failed: $LASTEXITCODE" }
	}
}
