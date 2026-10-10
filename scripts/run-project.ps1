param([ValidateSet('Debug','Release')][string]$Configuration='Debug',[switch]$NativeOnly,[string]$Project='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root "build/bin/$Configuration/TwoDogRuntime.exe"
if (-not (Test-Path $exe)) { throw 'Build the windows-csharp preset first (scripts/build-editor.ps1 or README commands).' }
if (-not $Project) { $Project=Join-Path $root 'samples/CSharpDemo' }
$arguments=@('--project',$Project)
if ($NativeOnly) { $arguments+='--no-scripts' }
& $exe @arguments
if ($LASTEXITCODE -ne 0) { throw "Runtime exited with code $LASTEXITCODE" }
