$ErrorActionPreference = 'Stop'
$toolsDir = "$PSScriptRoot\tools"
$vendorDir = "$PSScriptRoot\vendor"
New-Item -ItemType Directory -Force $toolsDir, $vendorDir | Out-Null
$downloads = @(
	@{ Url='https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/llvm-mingw-20261006-ucrt-x86_64.zip'; Name='llvm-mingw'; Destination=$toolsDir },
	@{ Url='https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.zip'; Name='imgui'; Destination=$vendorDir },
	@{ Url='https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.4.zip'; Name='minhook'; Destination=$vendorDir }
)
foreach ($download in $downloads) {
	$archive = Join-Path $toolsDir ($download.Name + '.zip')
	Invoke-WebRequest $download.Url -OutFile $archive
	Expand-Archive -LiteralPath $archive -DestinationPath $download.Destination -Force
}
Write-Output 'Dependencies ready. Run build.ps1.'
