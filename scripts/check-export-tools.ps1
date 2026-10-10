$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
# Exercise pure exporter functions without running dotnet, the player, or touching real projects.
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'export-windows.ps1'),[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw ($errors -join "`n") }
foreach ($function in $ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst]},$true)) {
    . ([scriptblock]::Create($function.Extent.Text))
}
function Require([bool]$Value,[string]$Message) { if (-not $Value) { throw $Message } }
function Reject([scriptblock]$Action) {
    $failed=$false
    try { & $Action | Out-Null } catch { $failed=$true }
    Require $failed 'Expected rejection'
}
$temp=Join-Path ([IO.Path]::GetTempPath()) ('2dog-export-test-'+[guid]::NewGuid().ToString('N'))
try {
    $Project=Join-Path $temp 'Project (test) [1]'
    $stage=Join-Path $temp 'Stage'
    New-Item -ItemType Directory -Force "$Project/assets/nested","$Project/build","$Project/.twodog","$stage/data" | Out-Null
    Require (Inside "$Project/assets/x" $Project) 'Child containment'
    Require (-not (Inside ($Project+'-other/file') $Project)) 'Sibling prefix containment'
    Require (-not (Inside (FullPath "$Project/../outside") $Project)) 'Traversal containment'
    Write-Host 'PASS export containment'
    WriteUtf8 "$Project/assets/nested/map.json" '{"hello":"world"}'
    $null=CopyAsset 'assets/nested/map.json'
    Require ((Get-Content -LiteralPath "$stage/data/assets/nested/map.json" -Raw) -eq '{"hello":"world"}') 'Asset copy failed'
    Write-Host 'PASS export nested assets with spaces and brackets'
    Reject { CopyAsset '../outside.json' }
    Reject { CopyAsset 'assets/missing.png' }
    WriteUtf8 "$Project/build/asset.png" 'x'
    Reject { CopyAsset 'build/asset.png' }
    WriteUtf8 "$Project/assets/private.cs" 'class Private {}'
    Reject { CopyAsset 'assets/private.cs' }
    Write-Host 'PASS export rejects missing, outside, generated and source assets'
    WriteUtf8 "$Project/assets/nested/Player.cs" 'class Player {}'
    WriteUtf8 "$Project/build/generated.cs" 'bad'
    WriteUtf8 "$Project/.twodog/cache.cs" 'bad'
    $scripts=@(CollectScripts $Project)
    Require ($scripts.Count -eq 2) 'Generated sources leaked into export'
    Write-Host 'PASS export source enumeration excludes caches'
    $escaped=Xml 'A (1) & [2];$.cs'
    Require ($escaped.Contains('%28') -and $escaped.Contains('&amp;') -and $escaped.Contains('%3B') -and $escaped.Contains('%24')) 'MSBuild escaping'
    $null=[xml]('<Project><Compile Include="'+$escaped+'" /></Project>')
    Write-Host 'PASS export MSBuild/XML path escaping'
    # Synthetic PE headers check the architecture gate, not executable behavior.
    $bytes=New-Object byte[] 256
    $bytes[0]=0x4D; $bytes[1]=0x5A; $bytes[0x3C]=0x80
    $bytes[0x80]=0x50; $bytes[0x81]=0x45; $bytes[0x84]=0x64; $bytes[0x85]=0x86
    [IO.File]::WriteAllBytes("$temp/x64.exe",$bytes)
    AssertX64 "$temp/x64.exe"
    $bytes[0x84]=0x4C; $bytes[0x85]=0x01
    [IO.File]::WriteAllBytes("$temp/x86.exe",$bytes)
    Reject { AssertX64 "$temp/x86.exe" }
    Reject { AssertX64 "$Project/assets/nested/map.json" }
    Write-Host 'PASS export x64 PE gate'
    $first=[IO.File]::ReadAllBytes("$Project/assets/nested/map.json")
    Require (-not ($first[0] -eq 239 -and $first[1] -eq 187)) 'JSON has BOM'
    Write-Host 'PASS export JSON UTF-8 without BOM'
    Write-Host 'EXPORT TOOLS: 7 checks passed'
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
