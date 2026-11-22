#Requires -Version 5.1
<#
  Build OpenRGBRipplePlugin.dll for OpenRGB 1.1 (Plugin API 5, Qt 6.8.3).
  The plugin's Qt major version must match OpenRGB.exe's (1.1 ships Qt 6).

  powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1
  powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1 -NoInstall
#>
param(
    [switch]$NoInstall
)
$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

$QtVersion = "6.8.3"
$QtArch    = "msvc2022_64"

function Find-VcVars {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $vswhere = "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    }
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $vs) { $vs = & $vswhere -latest -products * -property installationPath }
        if ($vs) {
            $bat = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $bat) { return $bat }
        }
    }
    $fallback = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $fallback) { return $fallback }
    return $null
}

# Only a Qt 6 MSVC 64-bit kit counts: a Qt 5 DLL will not load in OpenRGB 1.1.
function Test-Qt6Kit([string]$dir) {
    return (Test-Path (Join-Path $dir "bin\qmake.exe")) -and
           (Test-Path (Join-Path $dir "lib\Qt6Core.lib"))
}

function Find-Qt {
    $candidates = @(
        $env:QTDIR,
        "$PSScriptRoot\.qt\$QtVersion\$QtArch",
        "C:\Qt\$QtVersion\$QtArch"
    ) | Where-Object { $_ }
    foreach ($c in $candidates) {
        if (Test-Qt6Kit $c) { return $c }
    }
    if (Test-Path "C:\Qt") {
        $hit = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^6\.\d+(\.\d+)*$' } |
            Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { Join-Path $_.FullName $QtArch } |
            Where-Object { Test-Qt6Kit $_ } |
            Select-Object -First 1
        if ($hit) { return $hit }
    }
    return $null
}

function Get-Python {
    foreach ($name in @("py", "python", "python3")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

Write-Host "== OpenRGB Ripple Plugin builder ==" -ForegroundColor Cyan
Write-Host "Tip: Use arg: -NoInstall to avoid inplace upgrades" -ForegroundColor Cyan

$vcvars = Find-VcVars
if (-not $vcvars) {
    Write-Error "MSVC x64 not found. Visual Studio Installer -> Modify -> Desktop development with C++"
}

if (-not (Test-Path ".\OpenRGB\OpenRGBPluginInterface.h")) {
    Write-Host "Cloning OpenRGB release_1.1 (headers only)..."
    git clone --branch release_1.1 --depth 1 https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB
    if ($LASTEXITCODE -ne 0) {
        Write-Error "git clone failed. Install Git, or clone OpenRGB into this folder yourself."
    }
}

$qt = Find-Qt
if (-not $qt) {
    $py = Get-Python
    if (-not $py) {
        Write-Error @"
Python 3 is required once, to download Qt $QtVersion.
Install from https://www.python.org/downloads/  (tick Add python.exe to PATH)
then re-run this script.
"@
    }

    $venvPy = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
    if (-not (Test-Path $venvPy)) {
        Write-Host "Creating a clean Python venv (your system pip is broken / missing idna)..."
        & $py -m venv "$PSScriptRoot\.venv"
        if (-not (Test-Path $venvPy)) {
            Write-Error "Could not create .venv with $py"
        }
    }

    Write-Host "Installing aqtinstall into .venv ..."
    & $venvPy -m pip install --upgrade pip
    & $venvPy -m pip install "aqtinstall>=3.1" "requests" "idna" "charset-normalizer"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "pip install aqtinstall failed inside .venv"
    }

    Write-Host "Downloading Qt $QtVersion $QtArch into .qt\  (several hundred MB, first time only)..."
    New-Item -ItemType Directory -Force -Path "$PSScriptRoot\.qt" | Out-Null
    & $venvPy -m aqt install-qt windows desktop $QtVersion "win64_$QtArch" -O "$PSScriptRoot\.qt"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "aqtinstall failed. Check network / try again."
    }

    $qt = Find-Qt
    if (-not $qt) {
        Write-Error "Qt still missing after aqtinstall. Look under $PSScriptRoot\.qt"
    }
}

Write-Host "Qt: $qt"
Write-Host "vcvars: $vcvars"

New-Item -ItemType Directory -Force -Path "$PSScriptRoot\build" | Out-Null

# Drop qmake output from earlier builds (e.g. Qt 5 objects) so nmake
# cannot link stale .obj / moc files into the Qt 6 DLL.
foreach ($stale in @("release", "debug", "Makefile", "Makefile.Release", "Makefile.Debug", ".qmake.stash")) {
    $p = Join-Path $PSScriptRoot $stale
    if (Test-Path $p) { Remove-Item -Recurse -Force $p }
}

$qmake = Join-Path $qt "bin\qmake.exe"
$cmd = @"
@echo off
call `"$vcvars`"
set PATH=$qt\bin;%PATH%
cd /d `"$PSScriptRoot`"
`"$qmake`" OpenRGBRipplePlugin.pro -spec win32-msvc "CONFIG+=release"
if errorlevel 1 exit /b 1
nmake
"@

$bat = Join-Path $env:TEMP "openrgb-ripple-build.bat"
Set-Content -Path $bat -Value $cmd -Encoding ASCII
& cmd.exe /c $bat
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed. See the nmake / qmake output above."
}

$dll = Get-ChildItem -Path "$PSScriptRoot\build","$PSScriptRoot" -Filter "OpenRGBRipplePlugin*.dll" -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch "\\\.qt\\|\\\.venv\\" } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $dll) {
    Write-Error "Build finished but OpenRGBRipplePlugin.dll was not found."
}

Write-Host ""
Write-Host "Built $($dll.FullName)" -ForegroundColor Green

if ($NoInstall) {
    Write-Host "Skipped install (-NoInstall)."
} else {
    $destDir = Join-Path $env:APPDATA "OpenRGB\plugins"
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    Copy-Item -Force $dll.FullName (Join-Path $destDir $dll.Name)
    Write-Host "Copied to $destDir\$($dll.Name)"
    Write-Host ""
    Write-Host "Close OpenRGBRipple.exe if it is running."
    Write-Host "Restart OpenRGB and open the Ripple tab."
}
