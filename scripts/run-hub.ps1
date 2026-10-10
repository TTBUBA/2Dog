param([ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root "build/bin/$Configuration/TwoDogHub.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build first: .\scripts\build-editor.ps1 -Configuration Debug' }
Start-Process -FilePath $exe
