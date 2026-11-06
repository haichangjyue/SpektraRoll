# SPDX-License-Identifier: GPL-3.0-or-later
# Draw the SpektraRoll app icon (a spectral film reel) and write an .iconset.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\make-app-icon.ps1
#
# The generated iconset lives in ui-macos/Resources/AppIcon.iconset. On macOS
# run `iconutil -c icns ui-macos/Resources/AppIcon.iconset` to produce
# AppIcon.icns (scripts/build-macos-ui.sh does this automatically).

param(
    [string]$OutDir = (Join-Path $PSScriptRoot '..\ui-macos\Resources\AppIcon.iconset')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$size = 1024
$bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.Clear([System.Drawing.Color]::Transparent)

# --- Rounded dark background -------------------------------------------------
# Standard macOS grid: the rounded square occupies ~824 of 1024 px.
$rect = New-Object System.Drawing.RectangleF 100, 100, 824, 824
$radius = 180.0
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddArc($rect.X, $rect.Y, $radius, $radius, 180, 90)
$path.AddArc($rect.Right - $radius, $rect.Y, $radius, $radius, 270, 90)
$path.AddArc($rect.Right - $radius, $rect.Bottom - $radius, $radius, $radius, 0, 90)
$path.AddArc($rect.X, $rect.Bottom - $radius, $radius, $radius, 90, 90)
$path.CloseFigure()

$bgTop = [System.Drawing.Color]::FromArgb(255, 26, 27, 30)
$bgBottom = [System.Drawing.Color]::FromArgb(255, 9, 9, 11)
$bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $bgTop, $bgBottom, 62.0
$g.FillPath($bgBrush, $path)
$bgBrush.Dispose()

# Soft warm glow behind the reel.
$glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$glowPath.AddEllipse(172, 172, 680, 680)
$glowBrush = New-Object System.Drawing.Drawing2D.PathGradientBrush $glowPath
$glowBrush.CenterColor = [System.Drawing.Color]::FromArgb(46, 245, 184, 92)
$glowBrush.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 245, 184, 92))
$g.FillPath($glowBrush, $glowPath)
$glowBrush.Dispose()
$glowPath.Dispose()

$cx = 512.0
$cy = 512.0

# --- Spectral ring -----------------------------------------------------------
# Curated sweep: amber -> rose -> violet -> cyan -> amber.
$stops = @(
    @{ P = 0.00; C = @(245, 184, 92) },
    @{ P = 0.28; C = @(224, 96, 126) },
    @{ P = 0.55; C = @(124, 108, 240) },
    @{ P = 0.80; C = @(79, 195, 217) },
    @{ P = 1.00; C = @(245, 184, 92) }
)

function Get-SweepColor([double]$t) {
    for ($i = 0; $i -lt $stops.Count - 1; $i++) {
        $a = $stops[$i]; $b = $stops[$i + 1]
        if ($t -ge $a.P -and $t -le $b.P) {
            $f = ($t - $a.P) / ($b.P - $a.P)
            $r = [int]($a.C[0] + ($b.C[0] - $a.C[0]) * $f)
            $gr = [int]($a.C[1] + ($b.C[1] - $a.C[1]) * $f)
            $bl = [int]($a.C[2] + ($b.C[2] - $a.C[2]) * $f)
            return [System.Drawing.Color]::FromArgb(255, $r, $gr, $bl)
        }
    }
    return [System.Drawing.Color]::FromArgb(255, 245, 184, 92)
}

$ringRadius = 312.0
$ringWidth = 62.0
$segments = 240
$sweep = 360.0 / $segments
for ($i = 0; $i -lt $segments; $i++) {
    $start = -90.0 + $i * $sweep
    $col = Get-SweepColor ($i / [double]$segments)
    $pen = New-Object System.Drawing.Pen $col, $ringWidth
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawArc($pen, $cx - $ringRadius, $cy - $ringRadius, $ringRadius * 2, $ringRadius * 2,
        [float]$start, [float]($sweep + 1.2))
    $pen.Dispose()
}

# --- Reel hub and spokes -----------------------------------------------------
$cream = [System.Drawing.Color]::FromArgb(255, 243, 236, 222)
$creamDim = [System.Drawing.Color]::FromArgb(255, 214, 205, 188)

$hubRadius = 92.0
$hubBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush (
    (New-Object System.Drawing.RectangleF ($cx - $hubRadius), ($cy - $hubRadius), ($hubRadius * 2), ($hubRadius * 2))),
    $cream, $creamDim, 55.0
$g.FillEllipse($hubBrush, $cx - $hubRadius, $cy - $hubRadius, $hubRadius * 2, $hubRadius * 2)
$hubBrush.Dispose()

$spokeWidth = 58.0
$spokeInner = 120.0
$spokeOuter = 286.0
$spokePen = New-Object System.Drawing.Pen $cream, $spokeWidth
$spokePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$spokePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
foreach ($deg in @(-90.0, 30.0, 150.0)) {
    $rad = $deg * [Math]::PI / 180.0
    $x1 = $cx + [Math]::Cos($rad) * $spokeInner
    $y1 = $cy + [Math]::Sin($rad) * $spokeInner
    $x2 = $cx + [Math]::Cos($rad) * $spokeOuter
    $y2 = $cy + [Math]::Sin($rad) * $spokeOuter
    $g.DrawLine($spokePen, [float]$x1, [float]$y1, [float]$x2, [float]$y2)
}
$spokePen.Dispose()

# Small dark centre so the hub reads as a reel axle.
$axleBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 18, 18, 20))
$g.FillEllipse($axleBrush, $cx - 26, $cy - 26, 52, 52)
$axleBrush.Dispose()

$g.Dispose()

# --- Write the iconset -------------------------------------------------------
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutDir | Out-Null

$variants = @(
    @{ Name = 'icon_16x16.png'; Size = 16 },
    @{ Name = 'icon_16x16@2x.png'; Size = 32 },
    @{ Name = 'icon_32x32.png'; Size = 32 },
    @{ Name = 'icon_32x32@2x.png'; Size = 64 },
    @{ Name = 'icon_128x128.png'; Size = 128 },
    @{ Name = 'icon_128x128@2x.png'; Size = 256 },
    @{ Name = 'icon_256x256.png'; Size = 256 },
    @{ Name = 'icon_256x256@2x.png'; Size = 512 },
    @{ Name = 'icon_512x512.png'; Size = 512 },
    @{ Name = 'icon_512x512@2x.png'; Size = 1024 }
)

foreach ($v in $variants) {
    $out = New-Object System.Drawing.Bitmap $v.Size, $v.Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $og = [System.Drawing.Graphics]::FromImage($out)
    $og.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $og.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $og.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $og.Clear([System.Drawing.Color]::Transparent)
    $og.DrawImage($bmp, 0, 0, $v.Size, $v.Size)
    $og.Dispose()
    $out.Save((Join-Path $OutDir $v.Name), [System.Drawing.Imaging.ImageFormat]::Png)
    $out.Dispose()
}

$bmp.Dispose()
Write-Host "Wrote $OutDir"
