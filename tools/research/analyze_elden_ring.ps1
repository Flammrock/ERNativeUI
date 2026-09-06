param(
    [Parameter(Mandatory = $true)]
    [string]$GhidraHome,

    [Parameter(Mandatory = $true)]
    [string]$GameExe,

    [string]$ResearchRoot = (Join-Path $PSScriptRoot '..\..\research-work'),

    [ValidatePattern('^[1-9][0-9]*[MG]$')]
    [string]$HeadlessMaxMemory = '8G',

    [ValidateRange(1, 256)]
    [int]$MaxCpu = 12,

    [ValidateRange(300, 86400)]
    [int]$AnalysisTimeoutSeconds = 21600,

    [switch]$AllowUnknownExecutable,
    [switch]$Reimport
)

$ErrorActionPreference = 'Stop'

$expectedHash = 'D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134'
$resolvedGhidra = (Resolve-Path -LiteralPath $GhidraHome).Path
$resolvedExe = (Resolve-Path -LiteralPath $GameExe).Path
$headless = Join-Path $resolvedGhidra 'support\analyzeHeadless.bat'
if (-not (Test-Path -LiteralPath $headless -PathType Leaf)) {
    throw "Ghidra headless analyzer not found: $headless"
}

$actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedExe).Hash
if (-not $AllowUnknownExecutable -and $actualHash -ne $expectedHash) {
    throw "Unknown eldenring.exe SHA-256 $actualHash. Record the new build before using -AllowUnknownExecutable."
}

$resolvedResearch = [System.IO.Path]::GetFullPath($ResearchRoot)
$projectDirectory = Join-Path $resolvedResearch 'ghidra-project'
$exportDirectory = Join-Path $resolvedResearch 'exports'
$logDirectory = Join-Path $resolvedResearch 'logs'
$scriptDirectory = Join-Path $PSScriptRoot 'ghidra'
$knownSymbols = Join-Path $PSScriptRoot `
    '..\..\docs\research\address-map\eldenring_2.7.0.0_known_symbols.csv'
New-Item -ItemType Directory -Force -Path $projectDirectory,$exportDirectory,$logDirectory | Out-Null

$arguments = @(
    $projectDirectory,
    'ERNativeUI-EldenRing',
    '-scriptPath', $scriptDirectory,
    '-log', (Join-Path $logDirectory 'headless.log'),
    '-scriptlog', (Join-Path $logDirectory 'scripts.log'),
    '-max-cpu', $MaxCpu.ToString(),
    '-analysisTimeoutPerFile', $AnalysisTimeoutSeconds.ToString()
)

if ($Reimport) {
    $arguments += @('-import', $resolvedExe, '-overwrite')
} else {
    $arguments += @('-import', $resolvedExe)
}
$arguments += @(
    '-preScript', 'ConfigureLargePeAnalysis.java',
    '-postScript', 'ApplyKnownSymbols.java', ([System.IO.Path]::GetFullPath($knownSymbols)),
    '-postScript', 'ExportFunctionMap.java', (Join-Path $exportDirectory 'functions.csv'),
    '-postScript', 'ExportStringReferences.java', (Join-Path $exportDirectory 'string_references.csv')
)

Write-Host "Executable SHA-256: $actualHash"
Write-Host "Research directory: $resolvedResearch"
Write-Host "Ghidra headless heap: $HeadlessMaxMemory; CPUs: $MaxCpu; timeout: $AnalysisTimeoutSeconds seconds"
$previousHeadlessMemory = $env:GHIDRA_HEADLESS_MAXMEM
try {
    $env:GHIDRA_HEADLESS_MAXMEM = $HeadlessMaxMemory
    & $headless @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Ghidra analysis failed with exit code $LASTEXITCODE."
    }
} finally {
    $env:GHIDRA_HEADLESS_MAXMEM = $previousHeadlessMemory
}
