[CmdletBinding()]
param(
    [string]$Project = '',
    [string]$Output = '',
    [string]$Name = 'Game',
    [string]$StartScene = '',
    [string]$RuntimeDirectory = '',
    [string]$DotNet = 'dotnet',
    [string]$Request = '',
    [switch]$Run
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$stage = $null
$work = $null
$transcript = $false
function FullPath([string]$Path) { [IO.Path]::GetFullPath($Path) }
function Inside([string]$Path, [string]$Parent) {
    $separators = [char[]]@([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $normalizedPath = [IO.Path]::GetFullPath($Path).TrimEnd($separators)
    $normalizedParent = [IO.Path]::GetFullPath($Parent).TrimEnd($separators)
    $prefix = $normalizedParent + [IO.Path]::DirectorySeparatorChar
    return ($normalizedPath.Equals($normalizedParent, [StringComparison]::OrdinalIgnoreCase) -or
        $normalizedPath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase))
}
function NoLinks([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    while ($null -ne $item) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Symbolic links/junctions are not supported: $Path" }
        $item = if ($item -is [IO.DirectoryInfo]) { $item.Parent } else { $item.Directory }
    }
}
function WriteUtf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
function Xml([string]$Text) {
    # MSBuild item escaping must happen before XML escaping.
    foreach ($c in @('%', ';', '$', '@', "'", '(', ')', '?', '*')) {
        $Text = $Text.Replace($c, ('%{0:X2}' -f [int][char]$c))
    }
    [Security.SecurityElement]::Escape($Text)
}
function AssertX64([string]$File) {
    $stream = [IO.File]::OpenRead($File)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not a Windows executable: $File" }
        $stream.Position = 0x3C
        $offset = $reader.ReadInt32()
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw "Windows x64 required: $File" }
    } finally { $reader.Dispose() }
}
function CopyAsset([string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative)) { throw "Asset path must be relative: $Relative" }
    $source = FullPath (Join-Path $Project $Relative)
    if (-not (Inside $source $Project) -or $source -eq $Project) { throw "Asset outside project: $Relative" }
    $parts = $Relative.Replace('\','/').Split('/')
    if (@($parts | Where-Object { $_ -in @('.twodog','.git','.vs','bin','obj','build') }).Count) { throw "Asset in generated directory: $Relative" }
    if ([IO.Path]::GetExtension($source) -in @('.cs','.csproj','.sln','.exe','.dll','.ps1','.pdb')) { throw "Source or executable registered as asset: $Relative" }
    NoLinks $source
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing asset: $Relative" }
    $relativePath = $source.Substring($Project.Length).TrimStart('\','/')
    $target = Join-Path "$stage/data" $relativePath
    New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($target)) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
    return $source
}
function CollectScripts([string]$Directory) {
    foreach ($entry in Get-ChildItem -LiteralPath $Directory -Force) {
        if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { continue }
        if ($entry.PSIsContainer) {
            if ($entry.Name -notin @('.twodog','.git','.vs','build','bin','obj')) { CollectScripts $entry.FullName }
        } elseif ($entry.Extension -eq '.cs') { $entry.FullName }
    }
}
try {
    if ($Request) {
        $r = Get-Content -LiteralPath $Request -Raw -Encoding UTF8 | ConvertFrom-Json
        $Project = [string]$r.project; $Output = [string]$r.output; $Name = [string]$r.name
        $StartScene = [string]$r.scene; $RuntimeDirectory = [string]$r.runtime; $Run = [bool]$r.run
    }
    if ($env:OS -ne 'Windows_NT') { throw 'Windows x64 export must run on Windows.' }
    if (-not $Project -or -not $Output) { throw 'Specify -Project and -Output (a NEW destination folder).' }
    $Project = (FullPath $Project).TrimEnd('\','/')
    $Output = (FullPath $Output).TrimEnd('\','/')
    if ($Name -notmatch '^[A-Za-z0-9][A-Za-z0-9 _-]{0,63}$' -or $Name -match '^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$' -or $Name.EndsWith(' ')) {
        throw 'Name: use 1-64 letters A-Z, digits, spaces, underscores or hyphens; avoid Windows reserved names.'
    }
    if (Inside $Output $Project) { throw 'Choose an output folder OUTSIDE the source project.' }
    if (Test-Path -LiteralPath $Output) { throw 'Destination already exists. Choose a new folder; existing builds are never overwritten.' }
    $parent = Split-Path $Output -Parent
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) { throw 'The output parent folder must already exist.' }
    NoLinks $Project; NoLinks $parent
    $settings = Get-Content -LiteralPath "$Project/project.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($settings.version -ne 1) { throw 'Unsupported project version.' }
    if (-not $StartScene) { $StartScene = [string]$settings.startScene }
    if (@($settings.assets | Where-Object { $_.id -eq $StartScene -and $_.type -eq 'scene' }).Count -ne 1) { throw 'Start scene is not a registered scene asset.' }
    if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'build/export-templates/windows-x64/Release' }
    $RuntimeDirectory = FullPath $RuntimeDirectory
    if ((Inside $Output $RuntimeDirectory) -or (Inside $RuntimeDirectory $Output)) { throw 'Output must be separate from the engine runtime.' }
    $template = Get-Content -LiteralPath "$RuntimeDirectory/runtime-template.json" -Raw | ConvertFrom-Json
    if ($template.format -ne 1 -or $template.configuration -ne 'Release' -or $template.architecture -ne 'x64' -or -not $template.csharp -or -not $template.staticCRT) {
        throw 'Build the current Windows C# runtime in Release before exporting.'
    }
    $engineMarker = Join-Path $RuntimeDirectory 'engine-root.txt'
    if (Test-Path -LiteralPath $engineMarker) {
        $engineSource = (Get-Content -LiteralPath $engineMarker -Raw).Trim()
        if ($engineSource -and (Inside $Output $engineSource)) { throw 'Output must be OUTSIDE the engine source tree.' }
    }
    AssertX64 "$RuntimeDirectory/TwoDogRuntime.exe"
    $dotnetExe = (Get-Command $DotNet -CommandType Application).Source
    AssertX64 $dotnetExe
    $dotnetRoot = Split-Path $dotnetExe -Parent
    $runtime = Get-ChildItem -LiteralPath "$dotnetRoot/shared/Microsoft.NETCore.App" -Directory |
        Where-Object { $_.Name -match '^10\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    if (-not $runtime) { throw 'A stable .NET 10 x64 runtime is required on the build PC.' }
    $fxr = "$dotnetRoot/host/fxr/$($runtime.Name)"
    if (-not (Test-Path -LiteralPath "$fxr/hostfxr.dll")) { throw 'Matching .NET hostfxr is missing.' }
    AssertX64 "$fxr/hostfxr.dll"
    AssertX64 "$($runtime.FullName)/coreclr.dll"
    $exportCache = Join-Path $Project '.twodog/cache/exports'
    $exportLogs = Join-Path $Project '.twodog/logs/exports'
    New-Item -ItemType Directory -Force $exportCache,$exportLogs | Out-Null
    NoLinks $exportCache; NoLinks $exportLogs
    $job = [guid]::NewGuid().ToString('N')
    $work = Join-Path $exportCache $job
    New-Item -ItemType Directory $work | Out-Null
    Start-Transcript -LiteralPath "$exportLogs/build-$job.log" | Out-Null
    $transcript = $true
    Write-Host "Exporting $Name | Windows x64 | Release | .NET $($runtime.Name)"
    # Same volume as destination: final rename publishes only a completed package.
    $stage = Join-Path $parent ('.2dog-export-' + $job)
    New-Item -ItemType Directory $stage | Out-Null
    New-Item -ItemType Directory "$stage/data","$stage/managed/game","$stage/managed/native","$stage/licenses","$stage/dotnet/host/fxr","$stage/dotnet/shared/Microsoft.NETCore.App" | Out-Null
    $ids = @{}
    foreach ($asset in $settings.assets) {
        if (-not $asset.id -or $ids.ContainsKey([string]$asset.id)) { throw "Duplicate/empty asset ID: $($asset.id)" }
        $ids[[string]$asset.id] = $true
        $source = CopyAsset ([string]$asset.path)
        if ($asset.type -eq 'tilemap') {
            $map = Get-Content -LiteralPath $source -Raw -Encoding UTF8 | ConvertFrom-Json
            foreach ($tileset in $map.tilesets) {
                $directory = Split-Path $source -Parent
                if ($tileset.PSObject.Properties['source']) {
                    # Containment is checked before deriving the project-relative path.
                    $tsPath = FullPath (Join-Path $directory $tileset.source)
                    if (-not (Inside $tsPath $Project)) { throw 'Tileset outside project.' }
                    $tsPath = CopyAsset ($tsPath.Substring($Project.Length).TrimStart('\','/'))
                    $directory = Split-Path $tsPath -Parent
                    $tileset = Get-Content -LiteralPath $tsPath -Raw -Encoding UTF8 | ConvertFrom-Json
                }
                $image = FullPath (Join-Path $directory $tileset.image)
                if (-not (Inside $image $Project)) { throw 'Tileset image outside project.' }
                $null = CopyAsset ($image.Substring($Project.Length).TrimStart('\','/'))
            }
        }
    }
    $settings.startScene = $StartScene
    WriteUtf8 "$stage/data/project.json" ($settings | ConvertTo-Json -Depth 100)
    Copy-Item -LiteralPath "$RuntimeDirectory/TwoDogRuntime.exe" -Destination "$stage/$Name.exe"
    foreach ($file in @('TwoDog.ManagedHost.dll','TwoDog.ManagedHost.deps.json','TwoDog.ManagedHost.runtimeconfig.json','TwoDog.API.dll')) {
        Copy-Item -LiteralPath "$RuntimeDirectory/managed/$file" -Destination "$stage/managed/$file"
    }
    Copy-Item -LiteralPath "$RuntimeDirectory/managed/native/nethost.dll" -Destination "$stage/managed/native/nethost.dll"
    # Compile a fresh Release assembly against the exact API shipped with the player.
    # Never reuse the editor's last valid DLL after a compilation failure.
    $items = [Collections.Generic.List[string]]::new()
    $sources = @(CollectScripts $Project)
    $includeDemo = -not $settings.PSObject.Properties['includeDemoScripts'] -or [bool]$settings.includeDemoScripts
    if ($includeDemo) {
        $builtin = Join-Path $RuntimeDirectory 'Builtin'
        if (-not (Test-Path -LiteralPath $builtin)) { $builtin = Join-Path $RuntimeDirectory 'EditorSupport/Defaults' }
        $sources += @(CollectScripts $builtin)
    }
    $index = 0
    foreach ($source in $sources) {
        $copy = "$work/Source$index.cs"; $index++
        Copy-Item -LiteralPath $source -Destination $copy
        $items.Add('<Compile Include="' + (Xml $copy) + '" />')
    }
    $api = Xml "$stage/managed/TwoDog.API.dll"
    $projectXml = '<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net10.0</TargetFramework><AssemblyName>GameScripts</AssemblyName><ImplicitUsings>enable</ImplicitUsings><Nullable>enable</Nullable><EnableDefaultItems>false</EnableDefaultItems><ImportDirectoryBuildProps>false</ImportDirectoryBuildProps><ImportDirectoryBuildTargets>false</ImportDirectoryBuildTargets><DebugType>none</DebugType><LangVersion>12.0</LangVersion></PropertyGroup><ItemGroup>' + ($items -join "`n") + '<Reference Include="TwoDog.API"><HintPath>' + $api + '</HintPath><Private>false</Private></Reference></ItemGroup></Project>'
    WriteUtf8 "$work/GameScripts.csproj" $projectXml
    WriteUtf8 "$work/global.json" '{"sdk":{"version":"10.0.100","rollForward":"latestFeature","allowPrerelease":false}}'
    Push-Location -LiteralPath $work
    try {
        & $dotnetExe build "$work/GameScripts.csproj" -c Release -o "$stage/managed/game" --nologo -p:ImportDirectoryBuildProps=false -p:ImportDirectoryBuildTargets=false
    } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw 'C# compilation failed. No game was exported; see the log.' }
    Copy-Item -LiteralPath $runtime.FullName -Destination "$stage/dotnet/shared/Microsoft.NETCore.App" -Recurse
    Copy-Item -LiteralPath $fxr -Destination "$stage/dotnet/host/fxr" -Recurse
    Copy-Item -LiteralPath $dotnetExe -Destination "$stage/dotnet/dotnet.exe"
    foreach ($notice in @('LICENSE.txt','ThirdPartyNotices.txt')) {
        Copy-Item -LiteralPath "$dotnetRoot/$notice" -Destination "$stage/dotnet/$notice"
    }
    Copy-Item -Path "$RuntimeDirectory/ExportLicenses/*" -Destination "$stage/licenses" -Recurse
    if (@(Get-ChildItem -LiteralPath "$stage/licenses" -File).Count -lt 6) { throw 'Native dependency notices are missing; rebuild the runtime.' }
    WriteUtf8 "$stage/build-settings.json" (@{version=1; name=$settings.name; engine='0.7.3'; platform='win-x64'; runtime=$runtime.Name} | ConvertTo-Json)
    WriteUtf8 "$stage/README.txt" "Launch $Name.exe. Keep all folders together. Windows x64 with OpenGL 4.1 drivers required. .NET is bundled. This is a portable build, not an installer. Keep this folder writable if the game saves scenes."
    $manifest = @(Get-ChildItem -LiteralPath $stage -File -Recurse | ForEach-Object {
        @{ path=$_.FullName.Substring($stage.Length + 1).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    WriteUtf8 "$stage/manifest.json" ($manifest | ConvertTo-Json -Depth 5)
    [IO.Directory]::Move($stage, $Output)
    $stage = $null
    Write-Host "BUILD SUCCEEDED: $Output"
    if ($Run) { Start-Process -FilePath "$Output/$Name.exe" -WorkingDirectory $Output | Out-Null }
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
} finally {
    if ($transcript) { Stop-Transcript | Out-Null }
    if ($stage -and (Test-Path -LiteralPath $stage)) { Remove-Item -LiteralPath $stage -Recurse -Force }
    if ($work -and (Test-Path -LiteralPath $work)) { Remove-Item -LiteralPath $work -Recurse -Force }
}
