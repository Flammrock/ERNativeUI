param(
    [Parameter(Mandatory = $true)]
    [string]$GhidraHome,

    [Parameter(Mandatory = $true)]
    [ValidateSet('Anchors', 'CallGraph', 'FunctionContext', 'Decompile', 'Vtable', 'RttiClasses', 'RttiHierarchy', 'Callsites')]
    [string]$Query,

    [Parameter(Mandatory = $true)]
    [string[]]$ScriptArgument,

    [string]$ResearchRoot = (Join-Path $PSScriptRoot '..\..\research-work'),
    [string]$ProjectDirectory,
    [string]$ProjectName = 'ERNativeUI-EldenRing',
    [string]$ProgramName = 'eldenring.exe'
)

$ErrorActionPreference = 'Stop'

if ($ScriptArgument.Count -eq 0) {
    throw 'At least one script argument is required; the first one is always the output path.'
}

$scripts = @{
    Anchors         = 'FindUiAnchors.java'
    CallGraph       = 'ExportCallGraph.java'
    FunctionContext = 'ExportFunctionContext.java'
    Decompile       = 'ExportDecompiledFunctions.java'
    Vtable          = 'ExportVtable.java'
    RttiClasses     = 'ExportRttiClassInventory.java'
    RttiHierarchy   = 'ExportRttiHierarchy.java'
    Callsites       = 'ExportNativeCallsites.java'
}

$resolvedGhidra = (Resolve-Path -LiteralPath $GhidraHome).Path
$headless = Join-Path $resolvedGhidra 'support\analyzeHeadless.bat'
if (-not (Test-Path -LiteralPath $headless -PathType Leaf)) {
    throw "Ghidra headless analyzer not found: $headless"
}

$resolvedResearch = [System.IO.Path]::GetFullPath($ResearchRoot)
$resolvedProjectDirectory = if ([string]::IsNullOrWhiteSpace($ProjectDirectory)) {
    Join-Path $resolvedResearch 'ghidra-project'
} else {
    [System.IO.Path]::GetFullPath($ProjectDirectory)
}
$projectFile = Join-Path $resolvedProjectDirectory ($ProjectName + '.gpr')
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Ghidra project not found: $projectFile. Complete the whole-program import first."
}

$queryArguments = @($ScriptArgument)
$queryArguments[0] = [System.IO.Path]::GetFullPath($queryArguments[0])
$scriptDirectory = Join-Path $PSScriptRoot 'ghidra'
$logDirectory = Join-Path $resolvedResearch 'logs\queries'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'

$arguments = @(
    $resolvedProjectDirectory,
    $ProjectName,
    '-scriptPath', $scriptDirectory,
    '-postScript', $scripts[$Query]
) + $queryArguments + @(
    '-log', (Join-Path $logDirectory "$timestamp-$Query-headless.log"),
    '-scriptlog', (Join-Path $logDirectory "$timestamp-$Query-script.log"),
    '-readOnly',
    '-noanalysis',
    '-process', $ProgramName
)

# analyzeHeadless.bat forwards its arguments through a nested CALL. A literal
# percent sign would otherwise be re-parsed as a cmd.exe environment-variable
# delimiter (for example, the two `%s` tokens in Scaleform format strings can
# consume everything between them). Doubling each percent preserves the
# caller's literal value through that second batch expansion.
$arguments = @($arguments | ForEach-Object { $_.Replace('%', '%%') })

Write-Host "Query: $Query"
Write-Host "Program: $ProgramName"
Write-Host "Output: $($queryArguments[0])"
& $headless @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Ghidra query failed with exit code $LASTEXITCODE."
}
