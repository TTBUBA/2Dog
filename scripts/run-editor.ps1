param([ValidateSet('Debug','Release')][string]$Configuration='Debug',[string]$Project='',[switch]$NativeOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root "build/bin/$Configuration/TwoDogEditor.exe"
if (-not (Test-Path $exe)) { throw 'Build first: scripts/build-editor.ps1' }
if (-not $Project) { $Project=Join-Path $root 'samples/EditorDemo' }
$arguments=@('--project',$Project)
if ($NativeOnly) { $arguments+='--no-scripts' }
& $exe @arguments
if ($LASTEXITCODE -ne 0) { throw "Editor exited with code $LASTEXITCODE" }
