param([string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$entries = @()
foreach ($line in Get-Content "$Root/include/twodog/scripting/Api.def") {
    if ($line -match '^TD_API\((\w+),\s*(\d+),\s*"([^"]*)",\s*"([^"]*)"\)') {
        $entries += [pscustomobject]@{Name=$Matches[1];Code=$Matches[2];Request=$Matches[3];Result=$Matches[4]}
    }
}
if ($entries.Count -eq 0) { throw 'No API declarations found' }
# Generated .g.cs files must opt in explicitly even when the project enables nullable.
$lines = @('// Generated from include/twodog/scripting/Api.def. Do not edit.', '#nullable enable', 'using System.Text.Json.Nodes;', 'namespace TwoDog;', 'public static class RawApi', '{', '    public const int Version = 1;')
foreach ($entry in $entries) {
    $lines += "    // Request: $($entry.Request) -> $($entry.Result)"
    $lines += "    public static JsonNode? $($entry.Name)(JsonObject request) => Native.Call($($entry.Code), request);"
}
$lines += '}'
[IO.File]::WriteAllText("$Root/managed/TwoDog.API/RawApi.g.cs", ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Host "Generated $($entries.Count) native API bindings."
