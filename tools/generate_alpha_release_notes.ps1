[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+$')]
    [string] $ApiTarget,

    [Parameter(Mandatory)]
    [ValidateRange(1, [int]::MaxValue)]
    [int] $AlphaOrdinal,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string] $CommitSha,

    [Parameter(Mandatory)]
    [ValidatePattern('^api-\d+\.\d+$')]
    [string] $SourceBranch,

    [Parameter(Mandatory)]
    [string] $TagName,

    [Parameter(Mandatory)]
    [string] $BaseRef,

    [Parameter(Mandatory)]
    [ValidatePattern('^[^/]+/[^/]+$')]
    [string] $Repository,

    [Parameter(Mandatory)]
    [ValidatePattern('^https://')]
    [string] $WorkflowRunUrl,

    [Parameter(Mandatory)]
    [string] $BuildTimestampUtc,

    [Parameter(Mandatory)]
    [string] $OutputPath,

    [string] $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$expectedBranch = "api-$ApiTarget"
if ($SourceBranch -ne $expectedBranch) {
    throw "SourceBranch must be $expectedBranch for API $ApiTarget."
}

$expectedTag = "ERNativeUI-api-$ApiTarget-alpha.$AlphaOrdinal"
if ($TagName -ne $expectedTag) {
    throw "TagName must be $expectedTag for this alpha identity."
}

function Invoke-Git {
    param(
        [Parameter(Mandatory)]
        [string[]] $Arguments
    )

    $output = @(& git -C $RepositoryRoot @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed:`n$($output -join "`n")"
    }

    return ($output -join "`n").TrimEnd()
}

function Get-CommitCategory {
    param(
        [Parameter(Mandatory)]
        [string] $Type,

        [Parameter(Mandatory)]
        [bool] $Breaking
    )

    if ($Breaking) {
        return 'Breaking Changes'
    }

    switch ($Type.ToLowerInvariant()) {
        'feat' { return 'Features' }
        'fix' { return 'Bug Fixes' }
        'perf' { return 'Performance Improvements' }
        'revert' { return 'Reverts' }
        'docs' { return 'Documentation' }
        'refactor' { return 'Code Refactoring' }
        'test' { return 'Tests' }
        'build' { return 'Build System' }
        'ci' { return 'Continuous Integration' }
        'style' { return 'Styles' }
        'chore' { return 'Miscellaneous Chores' }
        default { return 'Other Changes' }
    }
}

function Format-CommitEntry {
    param(
        [Parameter(Mandatory)]
        [string] $Description,

        [string] $Scope,

        [Parameter(Mandatory)]
        [string] $Hash,

        [Parameter(Mandatory)]
        [string] $RepositoryName
    )

    $prefix = ''
    if (-not [string]::IsNullOrWhiteSpace($Scope)) {
        $prefix = "**${Scope}:** "
    }

    $shortHash = $Hash.Substring(0, 7)
    return "- $prefix$Description ([$shortHash](https://github.com/$RepositoryName/commit/$Hash))"
}

# Fail early if either boundary is absent or does not identify a commit.
Invoke-Git -Arguments @('cat-file', '-e', "$BaseRef`^{commit}") | Out-Null
Invoke-Git -Arguments @('cat-file', '-e', "$CommitSha`^{commit}") | Out-Null
$null = & git -C $RepositoryRoot merge-base --is-ancestor $BaseRef $CommitSha
if ($LASTEXITCODE -ne 0) {
    throw "Release-note base $BaseRef is not an ancestor of $CommitSha. Merge the current stable line into the API branch instead of publishing a misleading comparison."
}

$categoryOrder = @(
    'Breaking Changes',
    'Features',
    'Bug Fixes',
    'Performance Improvements',
    'Reverts',
    'Documentation',
    'Code Refactoring',
    'Tests',
    'Build System',
    'Continuous Integration',
    'Styles',
    'Miscellaneous Chores',
    'Other Changes'
)

$entries = [ordered]@{}
foreach ($category in $categoryOrder) {
    $entries[$category] = [System.Collections.Generic.List[string]]::new()
}

$revisionList = Invoke-Git -Arguments @('rev-list', '--reverse', "$BaseRef..$CommitSha")
$commitHashes = @()
if (-not [string]::IsNullOrWhiteSpace($revisionList)) {
    $commitHashes = @($revisionList -split "`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

foreach ($hash in $commitHashes) {
    $subject = Invoke-Git -Arguments @('show', '-s', '--format=%s', $hash)
    $body = Invoke-Git -Arguments @('show', '-s', '--format=%b', $hash)
    $match = [regex]::Match(
        $subject,
        '^(?<type>[A-Za-z]+)(?:\((?<scope>[^)]+)\))?(?<breaking>!)?:\s+(?<description>.+)$'
    )

    if ($match.Success) {
        $type = $match.Groups['type'].Value
        $scope = $match.Groups['scope'].Value
        $description = $match.Groups['description'].Value
        $breaking = $match.Groups['breaking'].Success -or
            [regex]::IsMatch($body, '(?m)^BREAKING(?: |-)(?:CHANGE|CHANGES):')
        $category = Get-CommitCategory -Type $type -Breaking $breaking
    } else {
        $scope = ''
        $description = $subject
        $category = 'Other Changes'
    }

    $entry = Format-CommitEntry `
        -Description $description `
        -Scope $scope `
        -Hash $hash `
        -RepositoryName $Repository
    $entries[$category].Add($entry)
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# ERUI API $ApiTarget Alpha $AlphaOrdinal")
$lines.Add('')
$lines.Add('> [!WARNING]')
$lines.Add("> This is an incomplete API $ApiTarget test build, not a stable release. Do not redistribute it as a normal mod dependency. Use the latest stable ERNativeUI release for ordinary installations.")
$lines.Add('')
$lines.Add("## Changes since ``$BaseRef``")
$lines.Add('')

if ($commitHashes.Count -eq 0) {
    $lines.Add('- No commits are present in the selected comparison range.')
    $lines.Add('')
} else {
    foreach ($category in $categoryOrder) {
        if ($entries[$category].Count -eq 0) {
            continue
        }

        $lines.Add("### $category")
        $lines.Add('')
        foreach ($entry in $entries[$category]) {
            $lines.Add($entry)
        }
        $lines.Add('')
    }
}

$lines.Add('## Known limitations')
$lines.Add('')
$lines.Add("- API $ApiTarget is still under development and may change incompatibly before its stable release.")
$lines.Add('- This runtime-only package does not publish draft API headers or a CMake SDK. Use only the bundled same-commit examples when testing client integration.')
$lines.Add('- This automated publication proves the Windows build, test, install, and installed-consumer gates only. It does not by itself claim live-game validation.')
$lines.Add('- This alpha is not published to the stable Nexus Files page and must not be redistributed as another mod''s normal dependency.')
$lines.Add('')
$lines.Add('## Build provenance')
$lines.Add('')
$lines.Add("- API target: ``$ApiTarget``")
$lines.Add("- Alpha ordinal: ``$AlphaOrdinal``")
$lines.Add("- Source branch: ``$SourceBranch``")
$lines.Add("- Git tag: ``$TagName``")
$lines.Add("- Commit: ``$CommitSha``")
$lines.Add("- Built at: ``$BuildTimestampUtc``")
$lines.Add("- Workflow run: $WorkflowRunUrl")
$lines.Add('- Archive kind: Windows runtime-only alpha')
$lines.Add('')
$lines.Add('## Testing and rollback')
$lines.Add('')
$lines.Add('1. Keep a copy of the latest stable ERNativeUI package before installing this alpha.')
$lines.Add('2. Load exactly one ERNativeUI host and use only the bundled same-commit client examples.')
$lines.Add('3. Play offline with Easy Anti-Cheat disabled and follow the test focus documented for the corresponding roadmap milestone.')
$lines.Add("4. Include ``api-$ApiTarget-alpha.$AlphaOrdinal``, the full commit SHA, game version, DLL order, configuration, logs, and reproduction steps in reports.")
$lines.Add("5. To roll back, remove the alpha files and reinstall the [latest stable release](https://github.com/$Repository/releases/latest).")
$lines.Add('')
$lines.Add("**Full comparison:** https://github.com/$Repository/compare/$BaseRef...$TagName")

$outputCandidate = $OutputPath
if (-not [System.IO.Path]::IsPathRooted($outputCandidate)) {
    $outputCandidate = Join-Path (Get-Location) $outputCandidate
}
$resolvedOutput = [System.IO.Path]::GetFullPath($outputCandidate)
$outputDirectory = Split-Path -Parent $resolvedOutput
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

Set-Content -LiteralPath $resolvedOutput -Value $lines -Encoding utf8
Write-Host "Alpha release notes written to $resolvedOutput"
