[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $BuildDirectory,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+([-.][0-9A-Za-z.-]+)?$')]
    [string] $Version,

    [switch] $IncludeNexus
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedBuild = (Resolve-Path (Join-Path $projectRoot $BuildDirectory)).Path
$releaseRoot = Join-Path $projectRoot 'dist\release'
$installRoot = Join-Path $releaseRoot "ERNativeUI-$Version-windows-x64"

if (Test-Path -LiteralPath $releaseRoot) {
    Remove-Item -LiteralPath $releaseRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $releaseRoot | Out-Null

cmake --install $resolvedBuild --config Release --prefix $installRoot
if ($LASTEXITCODE -ne 0) {
    throw 'CMake installation failed.'
}

$sdkArchive = Join-Path $releaseRoot "ERNativeUI-$Version-windows-x64.zip"
Compress-Archive -LiteralPath $installRoot -DestinationPath $sdkArchive -CompressionLevel Optimal

if ($IncludeNexus) {
    $nexusRoot = Join-Path $releaseRoot "ERNativeUI-$Version-nexus"
    $nexusArchive = Join-Path $releaseRoot "ERNativeUI-$Version-nexus.zip"
    New-Item -ItemType Directory -Path $nexusRoot | Out-Null
    Copy-Item -LiteralPath (Join-Path $installRoot 'bin\ERNativeUI.dll') -Destination $nexusRoot
    Copy-Item -LiteralPath (Join-Path $installRoot 'bin\ERNativeUI.ini') -Destination $nexusRoot
    Copy-Item -LiteralPath (Join-Path $installRoot 'bin\locales') -Destination $nexusRoot -Recurse
    Copy-Item -LiteralPath (Join-Path $installRoot 'bin\menu') -Destination $nexusRoot -Recurse
    Copy-Item -LiteralPath (Join-Path $installRoot 'bin\examples') -Destination $nexusRoot -Recurse
    Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\NEXUS_README.md') -Destination (Join-Path $nexusRoot 'README.md')
    Copy-Item -LiteralPath (Join-Path $installRoot 'share\ERNativeUI\LICENSE.txt') -Destination $nexusRoot
    Copy-Item -LiteralPath (Join-Path $installRoot 'share\ERNativeUI\THIRD_PARTY_NOTICES.txt') -Destination $nexusRoot
    Copy-Item -LiteralPath (Join-Path $installRoot 'share\ERNativeUI\licenses') -Destination $nexusRoot -Recurse
    Compress-Archive -Path (Join-Path $nexusRoot '*') -DestinationPath $nexusArchive -CompressionLevel Optimal
    Remove-Item -LiteralPath $nexusRoot -Recurse -Force
}

Remove-Item -LiteralPath $installRoot -Recurse -Force

$checksums = Get-ChildItem -LiteralPath $releaseRoot -Filter '*.zip' |
    Sort-Object Name |
    ForEach-Object {
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $($_.Name)"
    }
Set-Content -LiteralPath (Join-Path $releaseRoot 'SHA256SUMS.txt') -Value $checksums -Encoding ascii

Write-Host "Release assets created in $releaseRoot"
