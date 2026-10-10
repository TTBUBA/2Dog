[CmdletBinding(SupportsShouldProcess)]
param([string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$Root=[IO.Path]::GetFullPath($Root)
if (-not (Test-Path -LiteralPath (Join-Path $Root 'CMakeLists.txt'))) { throw 'Choose the 2Dog source root.' }
# Only known obsolete files, unchanged from the delivered 0.6 version, are removed.
# A locally edited checklist may contain user results and is deliberately preserved.
$obsolete=@{
    'docs/V03_CHECKLIST.md' = '6973bbee0a77d6259393ba73802d1ba1d214c95eba86f7525a0beb53d4d6cabb'
    'docs/V04_CHECKLIST.md' = 'f2a4916218a2835c960acc982ed0b14e538391607fbc49b2586ace05a0c78ca7'
    'docs/V05_CHECKLIST.md' = 'f3c70ba45cc3a55a804db9452e1cbe7ba4ba08a3cdb2bb3344df212f60ea0ebf'
    'docs/V06_CHECKLIST.md' = 'cf0d30ae64171818bfc2ed29a763d57ff9337853e213bcdc902b67b2eba2be95'
    'docs/CHANGELOG.md' = '9847a4bfeafa745fddd3894cf3a06b2b0e2f47683c4266f6fd9df614dad22e04'
    'scripts/verify-v03.ps1' = '44bcf32ef7f9a25c904113f559cfc0b91fc9876ce8ea6fa46adefc343dd55600'
    'scripts/verify-v04.ps1' = '2422f3bfd4dd165b5675c92d85c2cd074a036a01b48dc864298deee005db489b'
    'scripts/verify-v05.ps1' = '2d6e571cab6610dbdf87dc536755271ea564f99615737edc19c2aa9659729a04'
    'scripts/verify-v06.ps1' = 'ca84209f0a8899f28b3836411715db62e91edbfa83862fe7f498567b8b8dd9a7'
}
$removed=0
foreach ($relative in $obsolete.Keys | Sort-Object) {
    $path=Join-Path $Root $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $obsolete[$relative]) {
        Write-Warning "Preserved locally modified file: $relative"
        continue
    }
    if ($PSCmdlet.ShouldProcess($path,'Remove obsolete 0.3-0.6 documentation/verification script')) {
        Remove-Item -LiteralPath $path
        $removed++
    }
}
Write-Host "Cleanup complete: $removed obsolete files removed. Source projects, assets, builds and reports preserved."
