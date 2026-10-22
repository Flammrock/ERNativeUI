[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $BuildDirectory,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+$')]
    [string] $ApiTarget,

    [Parameter(Mandatory)]
    [ValidateRange(1, 2147483647)]
    [int] $AlphaOrdinal,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9A-Fa-f]{40}$')]
    [string] $CommitSha,

    [Parameter(Mandatory)]
    [ValidatePattern('^api-\d+\.\d+$')]
    [string] $SourceBranch,

    [Parameter(Mandatory)]
    [string] $TagName,

    [Parameter(Mandatory)]
    [string] $WorkflowRunUrl,

    [Parameter(Mandatory)]
    [string] $ReleaseNotesPath
)

$ErrorActionPreference = 'Stop'

function Get-CacheValue {
    param(
        [Parameter(Mandatory)]
        [string] $CachePath,

        [Parameter(Mandatory)]
        [string] $Name
    )

    $prefix = "$Name`:"
    $matches = @(Get-Content -LiteralPath $CachePath | Where-Object {
        $_.StartsWith($prefix, [StringComparison]::Ordinal)
    })
    if ($matches.Count -ne 1) {
        throw "Expected exactly one $Name entry in $CachePath."
    }

    $separator = $matches[0].IndexOf('=')
    if ($separator -lt 0) {
        throw "Malformed $Name entry in $CachePath."
    }
    return $matches[0].Substring($separator + 1)
}

function Assert-ChildPath {
    param(
        [Parameter(Mandatory)]
        [string] $Path,

        [Parameter(Mandatory)]
        [string] $Parent,

        [Parameter(Mandatory)]
        [string] $Description
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullParent = [IO.Path]::GetFullPath($Parent).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar)
    $prefix = $fullParent + [IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must remain inside $fullParent; got $fullPath."
    }
    return $fullPath
}

function Invoke-GitText {
    param(
        [Parameter(Mandatory)]
        [string[]] $Arguments
    )

    $output = & git -C $script:projectRoot @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed: $($output -join [Environment]::NewLine)"
    }
    return ($output -join [Environment]::NewLine).Trim()
}

function Assert-ReleaseNotes {
    param(
        [Parameter(Mandatory)]
        [string] $Path,

        [Parameter(Mandatory)]
        [string[]] $RequiredValues
    )

    $text = Get-Content -LiteralPath $Path -Raw
    if ([string]::IsNullOrWhiteSpace($text)) {
        throw 'The alpha release notes are empty.'
    }
    foreach ($value in $RequiredValues) {
        if ($text.IndexOf($value, [StringComparison]::Ordinal) -lt 0) {
            throw "The alpha release notes do not contain required provenance '$value'."
        }
    }
    if ($text -notmatch '(?i)\balpha\b' -or
        $text -notmatch '(?i)(testing only|not (a )?stable|unstable)') {
        throw 'The alpha release notes need an explicit alpha/testing warning.'
    }
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedBuild = (Resolve-Path (Join-Path $projectRoot $BuildDirectory)).Path
$resolvedNotes = (Resolve-Path $ReleaseNotesPath).Path
$cachePath = Join-Path $resolvedBuild 'CMakeCache.txt'
if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
    throw "The build directory has no CMakeCache.txt: $resolvedBuild"
}

$normalizedSha = $CommitSha.ToLowerInvariant()
$shortSha = $normalizedSha.Substring(0, 12)
$expectedBranch = "api-$ApiTarget"
$expectedTag = "ERNativeUI-api-$ApiTarget-alpha.$AlphaOrdinal"
$buildIdentity = "$expectedBranch-alpha.$AlphaOrdinal+g$shortSha"
$packageName = "$expectedTag-g$shortSha-runtime-windows-x64"

if ($SourceBranch -cne $expectedBranch) {
    throw "SourceBranch must be '$expectedBranch' for API target $ApiTarget."
}
if ($TagName -cne $expectedTag) {
    throw "TagName must be '$expectedTag' for alpha ordinal $AlphaOrdinal."
}

$runUri = $null
if (-not [Uri]::TryCreate($WorkflowRunUrl, [UriKind]::Absolute, [ref] $runUri) -or
    $runUri.Scheme -ne 'https' -or
    $runUri.Host -ne 'github.com' -or
    -not $runUri.AbsolutePath.Contains('/actions/runs/')) {
    throw "WorkflowRunUrl must be an HTTPS github.com Actions run URL; got '$WorkflowRunUrl'."
}

$headSha = Invoke-GitText -Arguments @('rev-parse', 'HEAD')
if ($headSha -cne $normalizedSha) {
    throw "CommitSha does not match the checked-out commit: expected $headSha, got $normalizedSha."
}
$dirty = Invoke-GitText -Arguments @('status', '--porcelain', '--untracked-files=all')
if (-not [string]::IsNullOrEmpty($dirty)) {
    throw "Alpha packages require a clean source checkout. Uncommitted paths:`n$dirty"
}

$configuredSource = Get-CacheValue -CachePath $cachePath -Name 'CMAKE_HOME_DIRECTORY'
if ([IO.Path]::GetFullPath($configuredSource) -cne
    [IO.Path]::GetFullPath($projectRoot)) {
    throw "The build directory belongs to '$configuredSource', not '$projectRoot'."
}
$configuredIdentity = Get-CacheValue -CachePath $cachePath `
    -Name 'ERNATIVEUI_EFFECTIVE_BUILD_IDENTITY'
if ($configuredIdentity -cne $buildIdentity) {
    throw "The build identity is '$configuredIdentity'; expected '$buildIdentity'. Reconfigure and rebuild before packaging."
}

Assert-ReleaseNotes -Path $resolvedNotes -RequiredValues @(
    $expectedTag,
    $SourceBranch,
    $normalizedSha,
    $WorkflowRunUrl
)

$releaseRoot = Assert-ChildPath -Path (Join-Path $projectRoot 'dist\alpha') `
    -Parent $projectRoot -Description 'Alpha output directory'
$stagingRoot = Assert-ChildPath -Path (Join-Path $releaseRoot '.install') `
    -Parent $releaseRoot -Description 'Alpha install staging directory'
$packageRoot = Assert-ChildPath -Path (Join-Path $releaseRoot $packageName) `
    -Parent $releaseRoot -Description 'Alpha package staging directory'
$archivePath = Assert-ChildPath -Path (Join-Path $releaseRoot "$packageName.zip") `
    -Parent $releaseRoot -Description 'Alpha archive'

if (Test-Path -LiteralPath $releaseRoot) {
    Remove-Item -LiteralPath $releaseRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $releaseRoot | Out-Null

try {
    cmake --install $resolvedBuild --config Release --prefix $stagingRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'CMake installation failed.'
    }

    $installedBin = Join-Path $stagingRoot 'bin'
    $installedShare = Join-Path $stagingRoot 'share\ERNativeUI'
    $requiredPayload = @(
        (Join-Path $installedBin 'ERNativeUI.dll'),
        (Join-Path $installedBin 'ERNativeUI.ini'),
        (Join-Path $installedBin 'locales'),
        (Join-Path $installedBin 'examples'),
        (Join-Path $installedShare 'LICENSE.txt'),
        (Join-Path $installedShare 'THIRD_PARTY_NOTICES.txt'),
        (Join-Path $installedShare 'licenses')
    )
    foreach ($requiredPath in $requiredPayload) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "Required alpha payload was not installed: $requiredPath"
        }
    }

    $forbiddenPayload = @(
        (Join-Path $installedBin 'ERNativeUIGfxPatcher.exe'),
        (Join-Path $installedBin 'menu'),
        (Join-Path $installedShare 'tools\gfx_patcher')
    )
    foreach ($forbiddenPath in $forbiddenPayload) {
        if (Test-Path -LiteralPath $forbiddenPath) {
            throw "Developer-only GFX payload was installed: $forbiddenPath"
        }
    }

    $dllPath = Join-Path $installedBin 'ERNativeUI.dll'
    $dllText = [Text.Encoding]::ASCII.GetString(
        [IO.File]::ReadAllBytes($dllPath))
    if ($dllText.IndexOf($buildIdentity, [StringComparison]::Ordinal) -lt 0) {
        throw "ERNativeUI.dll does not contain build identity '$buildIdentity'. Rebuild the configured tree before packaging."
    }

    New-Item -ItemType Directory -Path $packageRoot | Out-Null
    Copy-Item -LiteralPath $installedBin -Destination $packageRoot -Recurse

    $packageShare = Join-Path $packageRoot 'share\ERNativeUI'
    New-Item -ItemType Directory -Path $packageShare -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $installedShare 'LICENSE.txt') `
        -Destination $packageShare
    Copy-Item -LiteralPath (Join-Path $installedShare 'THIRD_PARTY_NOTICES.txt') `
        -Destination $packageShare
    Copy-Item -LiteralPath (Join-Path $installedShare 'licenses') `
        -Destination $packageShare -Recurse

    $packagedNotes = Join-Path $packageRoot 'ALPHA.md'
    Copy-Item -LiteralPath $resolvedNotes -Destination $packagedNotes
    $sourceNotesHash = (Get-FileHash -LiteralPath $resolvedNotes `
        -Algorithm SHA256).Hash
    $packagedNotesHash = (Get-FileHash -LiteralPath $packagedNotes `
        -Algorithm SHA256).Hash
    if ($sourceNotesHash -cne $packagedNotesHash) {
        throw 'The packaged ALPHA.md differs from the release notes input.'
    }

    $provenance = @(
        "build_identity=$buildIdentity",
        "api_target=$ApiTarget",
        "alpha_ordinal=$AlphaOrdinal",
        "source_branch=$SourceBranch",
        "commit_sha=$normalizedSha",
        "tag_name=$TagName",
        "workflow_run_url=$WorkflowRunUrl",
        'package_kind=runtime-only'
    )
    Set-Content -LiteralPath (Join-Path $packageRoot 'BUILD-PROVENANCE.txt') `
        -Value $provenance -Encoding ascii

    $payloadChecksums = Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            $hash = (Get-FileHash -LiteralPath $_.FullName `
                -Algorithm SHA256).Hash.ToLowerInvariant()
            $relative = $_.FullName.Substring($packageRoot.Length).TrimStart(
                [IO.Path]::DirectorySeparatorChar,
                [IO.Path]::AltDirectorySeparatorChar).Replace('\', '/')
            "$hash  $relative"
        }
    Set-Content -LiteralPath (Join-Path $packageRoot 'SHA256SUMS.payload.txt') `
        -Value $payloadChecksums -Encoding ascii

    Compress-Archive -LiteralPath $packageRoot -DestinationPath $archivePath `
        -CompressionLevel Optimal

    $archiveHash = (Get-FileHash -LiteralPath $archivePath `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath (Join-Path $releaseRoot 'SHA256SUMS.txt') `
        -Value "$archiveHash  $([IO.Path]::GetFileName($archivePath))" `
        -Encoding ascii
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $packageRoot) {
        Remove-Item -LiteralPath $packageRoot -Recurse -Force
    }
}

Write-Host "Alpha runtime assets created in $releaseRoot"
