param([Parameter(Mandatory=$true)][string]$Solution,[Parameter(Mandatory=$true)][string]$Source,[switch]$CheckWorkspace)
$ErrorActionPreference='Stop'

# Only a single solution path is sent to devenv. Its command parser does not
# reliably accept CRT-escaped nested quotes in /Command File.OpenFile.
function Quote-NativeArgument([string]$Value) {
    '"' + ([regex]::Replace(([regex]::Replace($Value, '(\\*)"', '$1$1\"')), '(\\+)$', '$1$1')) + '"'
}
function Initialize-ScriptWorkspace([string]$Solution) {
    $folder=Split-Path -Parent $Solution
    Push-Location -LiteralPath $folder
    try {
        $version=(& dotnet --version 2>&1 | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $version -notmatch '^8[.]') {
            throw "The generated Visual Studio 2022 workspace requires the .NET 8 SDK (x64), alongside the existing .NET 10 SDK. Install the .NET desktop development workload and .NET 8 SDK in Visual Studio Installer, then reopen this solution. Detail: $version"
        }
        & dotnet restore $Solution --disable-parallel --ignore-failed-sources -p:NuGetAudit=false
        if ($LASTEXITCODE -ne 0) { throw 'IDE reference restore failed. See the preceding log; runtime builds are separate.' }
    } finally { Pop-Location }
}
function Initialize-DteInterop {
    if ('TwoDog.IdeRunningObjectTable' -as [type]) { return }
    Add-Type -TypeDefinition @'
using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
namespace TwoDog {
    public static class IdeRunningObjectTable {
        [DllImport("ole32.dll")]
        private static extern int CreateBindCtx(uint reserved, out IBindCtx context);
        public static object ForProcess(int processId) {
            IBindCtx context = null;
            IRunningObjectTable table = null;
            IEnumMoniker iterator = null;
            try {
                Marshal.ThrowExceptionForHR(CreateBindCtx(0, out context));
                context.GetRunningObjectTable(out table);
                table.EnumRunning(out iterator);
                var entry = new IMoniker[1];
                string suffix = ":" + processId.ToString(CultureInfo.InvariantCulture);
                while (iterator.Next(1, entry, IntPtr.Zero) == 0) {
                    try {
                        string name;
                        entry[0].GetDisplayName(context, null, out name);
                        if (name.TrimStart('!').StartsWith("VisualStudio.DTE.", StringComparison.Ordinal)
                            && name.EndsWith(suffix, StringComparison.Ordinal)) {
                            object instance;
                            table.GetObject(entry[0], out instance);
                            return instance;
                        }
                    } catch (COMException) {
                        // Busy or inaccessible instances must not block another project.
                    } catch (UnauthorizedAccessException) {
                    } finally {
                        if (entry[0] != null) Marshal.ReleaseComObject(entry[0]);
                    }
                }
                return null;
            } finally {
                if (iterator != null) Marshal.ReleaseComObject(iterator);
                if (table != null) Marshal.ReleaseComObject(table);
                if (context != null) Marshal.ReleaseComObject(context);
            }
        }
    }
}
'@
}
function Get-DteForProcess([int]$ProcessId) {
    [TwoDog.IdeRunningObjectTable]::ForProcess($ProcessId)
}
function Release-Dte($Dte) {
    if ($null -ne $Dte -and [Runtime.InteropServices.Marshal]::IsComObject($Dte)) {
        $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($Dte)
    }
}
function Try-OpenSource($Dte,[string]$Solution,[string]$Source) {
    if ($null -eq $Dte) { return $false }
    try {
        # Never change the solution of an unrelated IDE window.
        $opened=$Dte.Solution.FullName
        if (-not $opened -or [IO.Path]::GetFullPath($opened) -ine $Solution) { return $false }
        # Opening the .sln alone is insufficient: the C# project must be loaded
        # and the source must belong to it, otherwise VS opens a loose file.
        $item=$Dte.Solution.FindProjectItem($Source)
        $projectPath=Join-Path (Split-Path -Parent $Solution) 'GameScripts.csproj'
        if ($null -eq $item -or $null -eq $item.ContainingProject -or
            [IO.Path]::GetFullPath($item.ContainingProject.FullName) -ine $projectPath) {
            $script:lastDteError='GameScripts.csproj has not loaded this source. Check project load errors in Visual Studio. The generated workspace requires the .NET 8 SDK and Visual Studio C# tools.'
            return $false
        }
        # Pass the path as a COM string, not as quoted command-line text.
        $null=$Dte.ItemOperations.OpenFile($Source)
        # Keep Solution Explorer visible, with solution/project nodes expanded.
        # UI customization must not prevent opening an otherwise valid script.
        try {
            $Dte.ExecuteCommand('View.SolutionExplorer')
            $tree=$Dte.ToolWindows.SolutionExplorer
            $tree.UIHierarchyItems.Expanded=$true
            foreach ($node in $tree.UIHierarchyItems) {
                $node.UIHierarchyItems.Expanded=$true
                foreach ($projectNode in $node.UIHierarchyItems) {
                    if ($projectNode.Name -eq 'GameScripts') {
                        $projectNode.UIHierarchyItems.Expanded=$true
                    }
                }
            }
        } catch { }
        $Dte.MainWindow.Visible=$true
        $Dte.MainWindow.Activate()
        return $true
    } catch {
        $script:lastDteError=$_.Exception.Message
        return $false # VS may temporarily reject automation during solution load.
    }
}
function Invoke-VisualStudioOpen([string]$Ide,[string]$Solution,[string]$Source,[int]$TimeoutSeconds=90) {
    $script:lastDteError='Visual Studio automation is not ready.'
    foreach ($instance in @(Get-Process -Name devenv -ErrorAction SilentlyContinue)) {
        $dte=$null
        try {
            $dte=Get-DteForProcess $instance.Id
            if (Try-OpenSource $dte $Solution $Source) {
                Write-Host "Opened script in existing solution: $Solution"
                return
            }
        } catch { $script:lastDteError=$_.Exception.Message } finally { Release-Dte $dte }
    }
    $arguments=Quote-NativeArgument $Solution
    Write-Host "Opening solution: $Solution"
    Write-Host "Script: $Source"
    $process=Start-Process -FilePath $Ide -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $Solution) -PassThru
    try {
        $timer=[Diagnostics.Stopwatch]::StartNew()
        do {
            if ($process.HasExited) { throw 'Visual Studio closed before the script could be opened.' }
            $dte=$null
            try {
                $dte=Get-DteForProcess $process.Id
                if (Try-OpenSource $dte $Solution $Source) {
                    Write-Host 'Solution and script opened successfully.'
                    return
                }
            } catch { $script:lastDteError=$_.Exception.Message } finally { Release-Dte $dte }
            if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { break }
            Start-Sleep -Milliseconds 500
        } while ($true)
        throw "Visual Studio started, but the script could not be opened within $TimeoutSeconds seconds. Complete any first-run dialogs, then double-click the script again. Run editor and IDE at the same privilege level. Detail: $script:lastDteError"
    } finally { $process.Dispose() } # Leave Visual Studio running.
}
try {
    $Solution=(Resolve-Path -LiteralPath $Solution).ProviderPath
    $Source=(Resolve-Path -LiteralPath $Source).ProviderPath
    if (-not (Test-Path -LiteralPath $Solution -PathType Leaf)) { throw 'Solution file not found.' }
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { throw 'C# source file not found.' }
    if ($CheckWorkspace) { Initialize-ScriptWorkspace $Solution }
    Initialize-DteInterop
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere.exe not found. Install Visual Studio Community.' }
    $installation=& $vswhere -latest -products Microsoft.VisualStudio.Product.Community -requires Microsoft.VisualStudio.Component.Roslyn.Compiler -property installationPath
    if (-not $installation) { $installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.Roslyn.Compiler -property installationPath }
    if (-not $installation) { throw 'Visual Studio C# tools not found. Add the .NET desktop development workload in Visual Studio Installer.' }
    $ide=Join-Path $installation 'Common7/IDE/devenv.exe'
    if (-not (Test-Path -LiteralPath $ide)) { throw 'devenv.exe not found.' }
    Invoke-VisualStudioOpen $ide $Solution $Source
} catch { Write-Error $_; exit 1 }
