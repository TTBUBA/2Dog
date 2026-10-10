param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$Output = '',
    [string]$DotNet = 'dotnet',
    [switch]$RebuildHost
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $Output) { $Output = Join-Path $root "build/bin/$Configuration/managed" }
$Output = [IO.Path]::GetFullPath($Output)
& "$root/tools/generate-bindings.ps1" -Root $root
if ($RebuildHost -or -not (Test-Path "$Output/TwoDog.ManagedHost.dll")) {
    & $DotNet build "$root/managed/TwoDog.ManagedHost/TwoDog.ManagedHost.csproj" -c $Configuration -o $Output --nologo
    if ($LASTEXITCODE -ne 0) { throw 'C# bootstrap build failed (close the engine before rebuilding the host)' }
}
# Build to staging. Publish DLL last so the native watcher sees one completed build.
$staging = Join-Path $root "build/managed/staging/$Configuration"
& $DotNet build "$root/managed/GameScripts/GameScripts.csproj" -c $Configuration -o $staging --nologo
if ($LASTEXITCODE -ne 0) { throw 'GameScripts build failed; the running assembly has not been replaced' }
$game = Join-Path $Output 'game'
New-Item -ItemType Directory -Force $game | Out-Null
Get-ChildItem $staging -File | Where-Object { $_.Name -ne 'GameScripts.dll' } | Copy-Item -Destination $game -Force
Copy-Item "$staging/GameScripts.dll" "$game/GameScripts.dll.new" -Force
Move-Item "$game/GameScripts.dll.new" "$game/GameScripts.dll" -Force
Write-Host "C# build ready: $game/GameScripts.dll"
Write-Host 'No tests were run by this script. The running project detects the new DLL, or press F9.'
