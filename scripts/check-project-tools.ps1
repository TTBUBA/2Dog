$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
# Parse all shipped PowerShell scripts without executing their external actions.
foreach ($file in Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1') {
    $tokens=$null; $errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
    if ($errors.Count -ne 0) { throw ($file.Name+': '+($errors -join '; ')) }
}
Write-Host 'PASS PowerShell syntax'
# Exercise path encoding. IDE workflow is tested separately below with fake DTE instances.
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'open-script.ps1'),[ref]$tokens,[ref]$errors)
$encoder=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Quote-NativeArgument'},$true)
if ($null -eq $encoder) { throw 'IDE argument encoder missing' }
. ([scriptblock]::Create($encoder.Extent.Text))
if ((Quote-NativeArgument 'C:\My Game\GameScripts.sln') -cne '"C:\My Game\GameScripts.sln"') { throw 'Solution path quoting failed' }
if ((Quote-NativeArgument 'C:\folder\') -cne '"C:\folder\\"') { throw 'Trailing slash quoting failed' }
Write-Host 'PASS IDE native argument quoting'
# Migration must preserve user-authored data in an old build and be idempotent.
$temp=Join-Path ([IO.Path]::GetTempPath()) ('2dog-migration-'+[guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Force "$temp/scripts","$temp/managed/Example/obj","$temp/build/windows-csharp/Debug/projects/MyGame" | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'archive-legacy-build.ps1') -Destination "$temp/scripts/archive-legacy-build.ps1"
    Set-Content -LiteralPath "$temp/build/windows-csharp/Debug/projects/MyGame/scene.json" -Value 'user scene'
    Set-Content -LiteralPath "$temp/managed/Example/obj/marker" -Value 'intermediate'
    & "$temp/scripts/archive-legacy-build.ps1"
    $backups=@(Get-ChildItem -LiteralPath "$temp/build/legacy" -Directory)
    if ($backups.Count -ne 1) { throw 'Expected one backup' }
    $backup=$backups[0].FullName
    if ((Get-Content -Raw -LiteralPath "$backup/build/windows-csharp/Debug/projects/MyGame/scene.json").Trim() -ne 'user scene') { throw 'Old project was not preserved' }
    if (-not (Test-Path -LiteralPath "$backup/managed/Example/obj/marker")) { throw 'Old intermediate was not archived' }
    & "$temp/scripts/archive-legacy-build.ps1"
    if (@(Get-ChildItem -LiteralPath "$temp/build/legacy" -Directory).Count -ne 1) { throw 'Migration is not idempotent' }
    Write-Host 'PASS legacy migration preserves projects and is idempotent'
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
& (Join-Path $PSScriptRoot 'check-ide-launcher.ps1')
Write-Host 'PROJECT TOOLS CHECKS: 3 passed + 5 IDE launcher checks'
