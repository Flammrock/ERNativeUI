<#
.SYNOPSIS
Generates the row and right-side control crops used by the control guides.

.PARAMETER ControlsRoot
Path to `docs/assets/images/controls`. Each control's source screenshots and
generated crops are kept in its own child directory.

.EXAMPLE
.\tools\generate-control-crops.ps1 -ControlsRoot .\docs\assets\images\controls
#>

param(
    [Parameter(Mandatory = $true)]
    [Alias("Root")]
    [string]$ControlsRoot
)

$ErrorActionPreference = "Stop"

# ------------------------------------------------------------
# Check ImageMagick
# ------------------------------------------------------------
$magick = Get-Command "magick" -ErrorAction SilentlyContinue
if (-not $magick) {
    Write-Host "ImageMagick was not found in PATH." -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install ImageMagick and make sure the 'magick' command is available in PATH."
    Write-Host "Download: https://imagemagick.org/script/download.php#windows"
    Write-Host ""
    Write-Host "After installing, open a new PowerShell window and verify with:"
    Write-Host "  magick -version"
    exit 1
}

# ------------------------------------------------------------
# Validate root directory
# ------------------------------------------------------------
if (-not (Test-Path -LiteralPath $ControlsRoot -PathType Container)) {
    Write-Host "Controls directory does not exist: $ControlsRoot" -ForegroundColor Red
    exit 1
}
$ControlsRoot = (Resolve-Path -LiteralPath $ControlsRoot).Path
Write-Host "Controls directory: $ControlsRoot"
Write-Host ""

# ------------------------------------------------------------
# Controls
# ------------------------------------------------------------
$controls = @(
    "button",
    "submenu",
    "toggle",
    "slider",
    "inline-choice",
    "popup-choice",
    "text-input",
    "color-picker"
)

# ------------------------------------------------------------
# Crop definitions
# ------------------------------------------------------------
$crops = @{
    "row" = @{
        X      = 496
        Y      = 227
        Width  = 1580
        Height = 88
    }

    "control" = @{
        X      = 1131
        Y      = 227
        Width  = 765
        Height = 88
    }
}

# ------------------------------------------------------------
# Generate images
# ------------------------------------------------------------
foreach ($control in $controls) {

    $controlDirectory = Join-Path $ControlsRoot $control
    if (-not (Test-Path -LiteralPath $controlDirectory -PathType Container)) {
        Write-Warning "Control image directory does not exist: $controlDirectory"
        continue
    }

    $beforeClick = Join-Path $controlDirectory "${control}_fullscreen_before_click.png"
    $afterClick  = Join-Path $controlDirectory "${control}_fullscreen_after_click.png"

    # Prefer before_click, fallback to after_click
    if (Test-Path -LiteralPath $beforeClick -PathType Leaf) {
        $inputFile = $beforeClick
    }
    elseif (Test-Path -LiteralPath $afterClick -PathType Leaf) {
        $inputFile = $afterClick
        Write-Host "Using after_click fallback for: $control" -ForegroundColor Yellow
    }
    else {
        Write-Warning "No source image found for '$control'. Expected:"
        Write-Warning "  $beforeClick"
        Write-Warning "  $afterClick"
        continue
    }

    foreach ($cropName in $crops.Keys) {
        $crop = $crops[$cropName]

        $outputFile = Join-Path $controlDirectory "${control}_${cropName}.png"
        $geometry = "$($crop.Width)x$($crop.Height)+$($crop.X)+$($crop.Y)"

        Write-Host "Generating: $outputFile"

        & magick `
            $inputFile `
            -crop $geometry `
            +repage `
            $outputFile

        if ($LASTEXITCODE -ne 0) {
            Write-Error "ImageMagick failed while generating '$outputFile'."
        }
    }
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
