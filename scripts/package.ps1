<#
.SYNOPSIS
  Build globe in Release and stage a portable, self-contained Windows zip.

.DESCRIPTION
  Builds the Release config in the EXISTING build/ tree (so FetchContent does
  not re-download anything), then collects the executable, its runtime assets,
  the required Visual C++ runtime DLLs, and the license/readme files into
  dist/globe-<version>-win64/ and zips it.

  Run from anywhere:  pwsh scripts/package.ps1
  Requires that `cmake -S . -B build` has already been configured once.
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "build",
    [string]$Config   = "Release"
)

$ErrorActionPreference = "Stop"

# --- paths -----------------------------------------------------------------
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    throw "No configured build at '$BuildDir'. Run: cmake -S . -B $BuildDir -G `"Visual Studio 17 2022`""
}

# --- version (parsed from CMakeLists project(... VERSION x.y.z ...)) --------
$cml = Get-Content (Join-Path $repo "CMakeLists.txt") -Raw
$m = [regex]::Match($cml, 'VERSION\s+(\d+\.\d+\.\d+)')
if ($m.Success) { $version = $m.Groups[1].Value } else { $version = "0.0.0" }
$stageName = "globe-$version-win64"
Write-Host "Packaging $stageName" -ForegroundColor Cyan

# --- build Release in the existing tree (no reconfigure, no re-fetch) -------
Write-Host "Building $Config..." -ForegroundColor Cyan
cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }

$exe = Join-Path $BuildDir "$Config/globe.exe"
if (-not (Test-Path $exe)) { throw "Expected executable not found: $exe" }

# --- clean staging dir ------------------------------------------------------
$dist  = Join-Path $repo "dist"
$stage = Join-Path $dist $stageName
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

# --- executable + top-level docs -------------------------------------------
Copy-Item $exe $stage
Copy-Item (Join-Path $repo "README.md")                 $stage
Copy-Item (Join-Path $repo "LICENSE")                   $stage
Copy-Item (Join-Path $repo "THIRD-PARTY-LICENSES.txt")  $stage

# --- assets (only what the app loads at runtime) ----------------------------
$aStage = Join-Path $stage "assets"
New-Item -ItemType Directory -Path (Join-Path $aStage "shaders")  -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $aStage "textures") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $aStage "data")     -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $aStage "fonts")    -Force | Out-Null

Copy-Item (Join-Path $repo "assets/shaders/*.vert")  (Join-Path $aStage "shaders")
Copy-Item (Join-Path $repo "assets/shaders/*.frag")  (Join-Path $aStage "shaders")
# Only the images the app actually loads (skip the large .tif source textures):
# the globe's Blue Marble map and the splash photo.
Copy-Item (Join-Path $repo "assets/textures/1280px-Blue_Marble_2002.png") (Join-Path $aStage "textures")
Copy-Item (Join-Path $repo "assets/textures/startsplashscreen.jpg")       (Join-Path $aStage "textures")
Copy-Item (Join-Path $repo "assets/data/*.tle")      (Join-Path $aStage "data")
Copy-Item (Join-Path $repo "assets/data/README.md")  (Join-Path $aStage "data")
# The UI font the app actually loads (JetBrains Mono Regular, OFL) + its license.
Copy-Item (Join-Path $repo "assets/fonts/JetBrainsMono-Regular.ttf") (Join-Path $aStage "fonts")
$ofl = Join-Path $repo "assets/fonts/OFL.txt"
if (Test-Path $ofl) {
    Copy-Item $ofl (Join-Path $aStage "fonts")
} else {
    Write-Warning "assets/fonts/OFL.txt not found - add JetBrains Mono's OFL license file (required to redistribute the font)."
}

# GeoNames city gazetteer (not in git; needed for the location search). If it's
# missing the app falls back to manual lat/lon entry, so warn but don't fail.
$cities = Join-Path $repo "assets/data/cities15000.txt"
if (Test-Path $cities) {
    Copy-Item $cities (Join-Path $aStage "data")
} else {
    Write-Warning "assets/data/cities15000.txt not found - city search will be unavailable in this build. See assets/data/README.md."
}

# --- Visual C++ runtime DLLs (app-local, so no redist install needed) -------
$crtRoot = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT" -Directory -ErrorAction SilentlyContinue |
    Sort-Object FullName | Select-Object -Last 1
if (-not $crtRoot) {
    Write-Warning "VC143 CRT redist folder not found. The zip will require the user to install the VC++ Redistributable."
} else {
    $crtDlls = @(
        "vcruntime140.dll","vcruntime140_1.dll",
        "msvcp140.dll","msvcp140_1.dll","msvcp140_2.dll","msvcp140_atomic_wait.dll"
    )
    foreach ($d in $crtDlls) {
        $src = Join-Path $crtRoot.FullName $d
        if (Test-Path $src) { Copy-Item $src $stage }
    }
    Write-Host "Bundled CRT from $($crtRoot.FullName)" -ForegroundColor DarkGray
}

# --- zip --------------------------------------------------------------------
$zip = Join-Path $dist "$stageName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
$sizeMB = [math]::Round((Get-Item $zip).Length / 1MB, 1)

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "  Staged: $stage"
Write-Host "  Zip:    $zip  ($sizeMB MB)"
