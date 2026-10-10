param([switch]$Graphics)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
function Invoke-Checked([scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE" }
}
if ($Graphics) {
    Invoke-Checked { cmake --preset windows-graphics }
    Invoke-Checked { cmake --build --preset windows-graphics --parallel }
    Invoke-Checked { ctest --preset windows-graphics --output-junit test-results.xml }
} else {
    Invoke-Checked { cmake --preset windows }
    foreach ($configuration in @("Debug", "Release")) {
        Invoke-Checked { cmake --build build/cmake/windows --config $configuration --parallel }
        Invoke-Checked { ctest --test-dir build/cmake/windows -C $configuration --output-on-failure --no-tests=error --output-junit "test-$configuration.xml" }
    }
}
