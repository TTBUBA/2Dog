$ErrorActionPreference='Stop'
# Load only function definitions: never discover or launch a real IDE in these tests.
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'open-script.ps1'),[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw ($errors -join '; ') }
foreach ($function in $ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)) {
    . ([scriptblock]::Create($function.Extent.Text))
}
Initialize-DteInterop # Compile the actual COM interop without calling Windows APIs.
function New-FakeDte([string]$Path,[int]$Failures=0) {
    $operations=[pscustomobject]@{Source=$null;Failures=$Failures}
    $operations | Add-Member ScriptMethod OpenFile {
        param($path)
        if ($this.Failures -gt 0) { $this.Failures--; throw 'RPC_E_CALL_REJECTED (simulated busy IDE)' }
        $this.Source=$path
    }
    $window=[pscustomobject]@{Visible=$false;Activated=$false}
    $window | Add-Member ScriptMethod Activate { $this.Activated=$true }
    $solutionObject=[pscustomobject]@{FullName=$Path;Loaded=$true}
    $solutionObject | Add-Member ScriptMethod FindProjectItem {
        param($path)
        if (-not $this.Loaded) { return $null }
        [pscustomobject]@{ContainingProject=[pscustomobject]@{FullName=(Join-Path (Split-Path -Parent $this.FullName) 'GameScripts.csproj')}}
    }
    [pscustomobject]@{Solution=$solutionObject;ItemOperations=$operations;MainWindow=$window}
}
function Reset-Fakes {
    $script:instances=@(); $script:dtes=@{}; $script:launches=0; $script:polls=0; $script:delay=0
    $script:process=[pscustomobject]@{Id=202;HasExited=$false;Disposed=$false}
    $script:process | Add-Member ScriptMethod Dispose { $this.Disposed=$true }
}
function Get-Process { param($Name,$ErrorAction); $script:instances }
function Get-DteForProcess([int]$ProcessId) {
    if ($ProcessId -eq 202) {
        $script:polls++
        if ($script:polls -le $script:delay) { return $null }
    }
    $script:dtes[$ProcessId]
}
function Start-Process {
    param($FilePath,$ArgumentList,$WorkingDirectory,[switch]$PassThru)
    $script:launches++
    if ($FilePath -ne 'fake-devenv.exe') { throw 'Wrong IDE executable' }
    if ($ArgumentList -cne (Quote-NativeArgument $script:solution)) { throw 'Only the solution may be passed on the command line' }
    if ($WorkingDirectory -cne (Split-Path -Parent $script:solution)) { throw 'Wrong working directory' }
    if (-not $PassThru) { throw 'Missing process handle' }
    $script:process
}
function Start-Sleep { param($Milliseconds) } # No wait needed for simulated startup.
$script:solution=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) "2Dog My Game [demo] & test/.twodog/GameScripts.sln"))
$source=Join-Path (Split-Path -Parent $script:solution) ("../assets/Folder A [1] & test/Player"+[char]0x00e8+".cs")

Reset-Fakes
$script:instances=@([pscustomobject]@{Id=101})
$matching=New-FakeDte $script:solution
$script:dtes[101]=$matching
Invoke-VisualStudioOpen 'fake-devenv.exe' $script:solution $source
if ($script:launches -ne 0 -or $matching.ItemOperations.Source -cne $source -or -not $matching.MainWindow.Activated) { throw 'Existing solution reuse failed' }
Write-Host 'PASS IDE reuse preserves the complete source path'

Reset-Fakes
$script:instances=@([pscustomobject]@{Id=101})
$other=New-FakeDte ($script:solution+'.other')
$script:dtes[101]=$other
$created=New-FakeDte $script:solution 1
$script:dtes[202]=$created; $script:delay=2
Invoke-VisualStudioOpen 'fake-devenv.exe' $script:solution $source
if ($script:launches -ne 1 -or $other.ItemOperations.Source -or $created.ItemOperations.Source -cne $source -or $script:polls -ne 4 -or -not $script:process.Disposed) { throw 'Cold IDE launch / busy retry / project isolation failed' }
Write-Host 'PASS IDE cold start, delayed automation and unrelated solution isolation'

Reset-Fakes
$script:process.HasExited=$true
$failure=''
try { Invoke-VisualStudioOpen 'fake-devenv.exe' $script:solution $source } catch { $failure=$_.Exception.Message }
if ($failure -notlike '*closed before*' -or -not $script:process.Disposed) { throw 'Early IDE exit was not reported' }
Write-Host 'PASS IDE startup failure is reported'

Reset-Fakes
$failure=''
try { Invoke-VisualStudioOpen 'fake-devenv.exe' $script:solution $source -TimeoutSeconds 0 } catch { $failure=$_.Exception.Message }
if ($failure -notlike '*could not be opened within*' -or -not $script:process.Disposed) { throw 'IDE timeout was not reported' }
Write-Host 'PASS IDE timeout is bounded and leaves the IDE running'
$unloaded=New-FakeDte $script:solution
$unloaded.Solution.Loaded=$false
if (Try-OpenSource $unloaded $script:solution $source) { throw 'Unloaded project was reported as success' }
if ($unloaded.ItemOperations.Source -or $script:lastDteError -notlike '*project load errors*') { throw 'Unloaded project opened a loose file without diagnostics' }
Write-Host 'PASS unloaded C# project cannot silently open a loose file'
Write-Host 'IDE LAUNCHER CHECKS: 5 passed (simulated DTE; Windows GUI still requires local checks)' 
