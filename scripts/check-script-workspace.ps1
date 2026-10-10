param(
    [Parameter(Mandatory=$true)][string]$Fixture,
    [Parameter(Mandatory=$true)][string]$Managed,
    [Parameter(Mandatory=$true)][string]$Support,
    [string]$DotNet8='dotnet',
    [string]$DotNet10='dotnet'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$Fixture=[IO.Path]::GetFullPath($Fixture)
$Managed=[IO.Path]::GetFullPath($Managed)
$Support=[IO.Path]::GetFullPath($Support)
$temp=Join-Path ([IO.Path]::GetTempPath()) ('2dog-ide-check-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $temp | Out-Null
try {
    # These ancestor files must not influence either generated build.
    [IO.File]::WriteAllText((Join-Path $temp 'global.json'),'{"sdk":{"version":"6.0.100","rollForward":"disable"}}')
    [IO.File]::WriteAllText((Join-Path $temp 'Directory.Build.props'),'<Project><Target Name="RejectInheritedProps" BeforeTargets="PrepareForBuild"><Error Text="Ancestor props leaked"/></Target></Project>')
    & $Fixture $temp $Managed $Support
    if ($LASTEXITCODE -ne 0) { throw 'Fixture generation failed' }
    $root=Join-Path $temp 'Game [IDE] & test'
    $work=Join-Path $root '.twodog'
    $source=Join-Path $root 'assets/Scripts/Nested/CompletionProbe.cs'
    [IO.File]::WriteAllText($source,@'
using TwoDog;
namespace GameScripts;
public sealed class CompletionProbe : Behaviour {
    public override void OnCreate() {
        if (Entity.TryGetComponent<Rigidbody2D>(out var body)) {
            body.Velocity = new Vec2(1, 2);
            body.AddForce(new Vec2(2, 0));
        }
        var sprite = Entity.GetComponent<SpriteRenderer>();
        if (sprite is not null) sprite.Color = new Color(1, 0, 0);
        var text = Entity.GetComponent<UIText>();
        if (text is not null) text.Text = "References available";
    }
}
'@)
    Push-Location -LiteralPath $work
    try {
        & $DotNet8 build GameScripts.sln --nologo -m:1 -p:UseSharedCompilation=false
        if ($LASTEXITCODE -ne 0) { throw 'VS2022-compatible solution failed. Install the .NET 8 SDK.' }
        $design=& $DotNet8 msbuild GameScripts.csproj -t:Compile -p:DesignTimeBuild=true -p:BuildingInsideVisualStudio=true -p:SkipCompilerExecution=true -p:ProvideCommandLineArgs=true -getItem:CscCommandLineArgs
        if ($LASTEXITCODE -ne 0 -or ($design -join "") -notmatch 'TwoDog.API.dll') { throw 'Design-time compiler does not receive the API reference' }
    } finally { Pop-Location }
    $ideAssets=Join-Path $work 'cache/ide/GameScripts/obj/project.assets.json'
    $before=(Get-FileHash -LiteralPath $ideAssets).Hash
    $runtime=Join-Path $work 'cache/scripts'
    Push-Location -LiteralPath $runtime
    try {
        & $DotNet10 build RuntimeScripts.csproj -c Debug -o build/Debug --nologo -m:1 -p:UseSharedCompilation=false
        if ($LASTEXITCODE -ne 0) { throw 'Isolated .NET 10 runtime project failed' }
    } finally { Pop-Location }
    if ((Get-FileHash -LiteralPath $ideAssets).Hash -ne $before) { throw 'Runtime build changed IDE restore assets' }
    if (-not (Test-Path -LiteralPath "$runtime/build/Debug/GameScripts.dll")) { throw 'Runtime assembly missing' }
    if (-not (Test-Path -LiteralPath "$work/EngineAPI/Sources/Components.cs")) { throw 'Typed API source missing from IDE workspace' }
    Write-Host 'SCRIPT WORKSPACE: solution build, design-time API reference and isolated runtime build passed'
} finally { Remove-Item -LiteralPath $temp -Recurse -Force }
