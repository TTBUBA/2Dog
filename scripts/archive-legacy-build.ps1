# Preserve old outputs (including any projects edited inside them) instead of deleting them.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$items=@()
foreach ($name in @('windows-csharp','windows','windows-graphics','headless','csharp-staging')) {
    $path=Join-Path $root "build/$name"
    if (Test-Path -LiteralPath $path) { $items+=@{Source=$path;Relative="build/$name"} }
}
foreach ($project in Get-ChildItem -LiteralPath (Join-Path $root 'managed') -Directory) {
    foreach ($kind in @('bin','obj')) {
        $path=Join-Path $project.FullName $kind
        if (Test-Path -LiteralPath $path) { $items+=@{Source=$path;Relative="managed/$($project.Name)/$kind"} }
    }
}
if ($items.Count -eq 0) { return }
$backup=Join-Path $root ('build/legacy/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
foreach ($item in $items) {
    $destination=Join-Path $backup $item.Relative
    New-Item -ItemType Directory -Force (Split-Path $destination -Parent) | Out-Null
    Move-Item -LiteralPath $item.Source -Destination $destination
}
Write-Host "Previous build outputs preserved in: $backup"
