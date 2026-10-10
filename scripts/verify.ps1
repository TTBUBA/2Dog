param([ValidateSet('Debug','Release','Both')][string]$Configuration='Both',[switch]$SkipGraphics)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
$report=Join-Path $root ("reports/local-"+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $report | Out-Null
Start-Transcript -Path "$report/session.txt" | Out-Null
try {
    & "$PSScriptRoot/check-project-tools.ps1"
    & "$PSScriptRoot/check-export-tools.ps1"
    & "$PSScriptRoot/archive-legacy-build.ps1"
    & cmake --preset windows-csharp
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    $configs=if ($Configuration -eq 'Both') { @('Debug','Release') } else { @($Configuration) }
    $failures=@()
    foreach ($config in $configs) {
        & cmake --build build/cmake/windows-csharp --config $config --parallel 2
        if ($LASTEXITCODE -ne 0) { throw "Build $config failed" }
        $arguments=@('--test-dir','build/cmake/windows-csharp','-C',$config,'--output-on-failure','--no-tests=error','--output-junit',"$report/native-$config.xml")
        if ($SkipGraphics) { $arguments+=@('-LE','graphics') }
        & ctest @arguments
        if ($LASTEXITCODE -ne 0) { $failures+="Native $config" }
        & dotnet build managed/TestScripts/TestScripts.csproj -c $config -o "build/tests/$config/managed" --nologo
        if ($LASTEXITCODE -ne 0) { $failures+="Managed fixtures $config"; continue }
        & dotnet run --project managed/HostChecks/HostChecks.csproj -c $config -- "$root/build/tests/$config/managed/TestScripts.dll"
        if ($LASTEXITCODE -ne 0) { $failures+="Managed checks $config" }
        & dotnet run --project managed/ComponentChecks/ComponentChecks.csproj -c $config
        if ($LASTEXITCODE -ne 0) { $failures+="Typed component checks $config" }
        try {
            & "$PSScriptRoot/check-script-workspace.ps1" -Fixture "$root/build/tests/$config/TwoDogWorkspaceFixture.exe" -Managed "$root/build/bin/$config/managed" -Support "$root/build/bin/$config/EditorSupport"
        } catch { $failures+="IDE workspace $config"; Write-Warning $_ }

    }
    if ($failures.Count -gt 0) { throw ("Failed checks: "+($failures -join ', ')) }
    Write-Host 'Automatic checks passed. Continue with docs/TEST_EDITOR.md.'
} finally { Stop-Transcript | Out-Null; Pop-Location; Write-Host "Local report: $report" }
