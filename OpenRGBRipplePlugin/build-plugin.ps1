#Requires -Version 5.1
<#
  Build OpenRGBRipplePlugin.dll for OpenRGB 1.0rc3 (Plugin API 4, Qt 5.15).

  powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1
#>
$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

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

function Find-Qt {
    $candidates = @(
        $env:QTDIR,
        "$PSScriptRoot\.qt\5.15.2\msvc2019_64",
        "C:\Qt\5.15.2\msvc2019_64",
        "C:\Qt\5.15.0\msvc2019_64"
    ) | Where-Object { $_ }
    foreach ($c in $candidates) {
        if (Test-Path (Join-Path $c "bin\qmake.exe")) { return $c }
    }
    if (Test-Path "C:\Qt") {
        $hit = Get-ChildItem "C:\Qt" -Recurse -Filter qmake.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.Directory.Name -eq "bin" } |
            Select-Object -First 1
        if ($hit) { return $hit.Directory.Parent.FullName }
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

$vcvars = Find-VcVars
if (-not $vcvars) {
    Write-Error "MSVC x64 not found. Visual Studio Installer -> Modify -> Desktop development with C++"
}

if (-not (Test-Path ".\OpenRGB\OpenRGBPluginInterface.h")) {
    Write-Host "Cloning OpenRGB release_candidate_1.0rc2 (headers only)..."
    git clone --branch release_candidate_1.0rc2 --depth 1 https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB
    if ($LASTEXITCODE -ne 0) {
        Write-Error "git clone failed. Install Git, or clone OpenRGB into this folder yourself."
    }
}

$qt = Find-Qt
if (-not $qt) {
    $py = Get-Python
    if (-not $py) {
        Write-Error @"
Python 3 is required once, to download Qt 5.15.
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

    Write-Host "Downloading Qt 5.15.2 msvc2019_64 into .qt\  (~200 MB, first time only)..."
    New-Item -ItemType Directory -Force -Path "$PSScriptRoot\.qt" | Out-Null
    & $venvPy -m aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -O "$PSScriptRoot\.qt"
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

$destDir = Join-Path $env:APPDATA "OpenRGB\plugins"
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
Copy-Item -Force $dll.FullName (Join-Path $destDir $dll.Name)

Write-Host ""
Write-Host "Built $($dll.FullName)" -ForegroundColor Green
Write-Host "Copied to $destDir\$($dll.Name)"
Write-Host ""
Write-Host "Close OpenRGBRipple.exe if it is running."
Write-Host "Restart OpenRGB and open the Ripple tab."
