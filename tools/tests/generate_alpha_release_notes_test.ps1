[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generator = Join-Path $projectRoot 'tools\generate_alpha_release_notes.ps1'
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("ernativeui-alpha-notes-" + [guid]::NewGuid().ToString('N'))

function Invoke-GitFixture {
    param(
        [Parameter(Mandatory)]
        [string[]] $Arguments
    )

    $output = @(& git -C $fixtureRoot @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Fixture git command failed: git $($Arguments -join ' ')`n$($output -join "`n")"
    }
    return ($output -join "`n").TrimEnd()
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)]
        [string] $Text,

        [Parameter(Mandatory)]
        [string] $Expected
    )

    if (-not $Text.Contains($Expected)) {
        throw "Expected generated notes to contain: $Expected"
    }
}

try {
    New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
    Invoke-GitFixture -Arguments @('init', '--quiet') | Out-Null
    Invoke-GitFixture -Arguments @('config', 'user.name', 'ERNativeUI test') | Out-Null
    Invoke-GitFixture -Arguments @('config', 'user.email', 'test@example.invalid') | Out-Null

    Invoke-GitFixture -Arguments @('commit', '--quiet', '--allow-empty', '-m', 'chore: stable base') | Out-Null
    $base = Invoke-GitFixture -Arguments @('rev-parse', 'HEAD')

    Invoke-GitFixture -Arguments @('commit', '--quiet', '--allow-empty', '-m', 'feat(menu): add reactive rows') | Out-Null
    Invoke-GitFixture -Arguments @(
        'commit',
        '--quiet',
        '--allow-empty',
        '-m',
        'fix: change fixture contract',
        '-m',
        'BREAKING-CHANGE: fixture-only compatibility break'
    ) | Out-Null
    Invoke-GitFixture -Arguments @('commit', '--quiet', '--allow-empty', '-m', 'docs: explain alpha builds') | Out-Null
    Invoke-GitFixture -Arguments @('commit', '--quiet', '--allow-empty', '-m', 'uncategorized fixture change') | Out-Null
    $head = Invoke-GitFixture -Arguments @('rev-parse', 'HEAD')

    $outputPath = Join-Path $fixtureRoot 'output\ALPHA.md'
    & $generator `
        -ApiTarget '1.4' `
        -AlphaOrdinal 2 `
        -CommitSha $head `
        -SourceBranch 'api-1.4' `
        -TagName 'ERNativeUI-api-1.4-alpha.2' `
        -BaseRef $base `
        -Repository 'Flammrock/ERNativeUI' `
        -WorkflowRunUrl 'https://github.com/Flammrock/ERNativeUI/actions/runs/123' `
        -BuildTimestampUtc '2026-10-22T12:00:00Z' `
        -OutputPath $outputPath `
        -RepositoryRoot $fixtureRoot

    $notes = Get-Content -LiteralPath $outputPath -Raw
    Assert-Contains -Text $notes -Expected '# ERUI API 1.4 Alpha 2'
    Assert-Contains -Text $notes -Expected '### Breaking Changes'
    Assert-Contains -Text $notes -Expected 'change fixture contract'
    Assert-Contains -Text $notes -Expected '### Features'
    Assert-Contains -Text $notes -Expected '**menu:** add reactive rows'
    Assert-Contains -Text $notes -Expected '### Documentation'
    Assert-Contains -Text $notes -Expected 'explain alpha builds'
    Assert-Contains -Text $notes -Expected '### Other Changes'
    Assert-Contains -Text $notes -Expected 'uncategorized fixture change'
    Assert-Contains -Text $notes -Expected "- Commit: ``$head``"
    Assert-Contains -Text $notes -Expected 'compare/'
    Assert-Contains -Text $notes -Expected 'ERNativeUI-api-1.4-alpha.2'

    Write-Host 'Alpha release-note generator tests passed.'
} finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
