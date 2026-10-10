param([ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    & "$PSScriptRoot/archive-legacy-build.ps1"
    & cmake --preset windows-csharp
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    & cmake --build build/cmake/windows-csharp --config $Configuration --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "Editor build $Configuration failed" }
    Write-Host 'Build complete. No tests run. Start with scripts/run-hub.ps1 or scripts/run-editor.ps1.'
} finally { Pop-Location }
