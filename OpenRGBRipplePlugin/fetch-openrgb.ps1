# Clone OpenRGB 1.1 headers (Plugin API 5) next to this script.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
if (Test-Path ".\OpenRGB\OpenRGBPluginInterface.h") {
    Write-Host "OpenRGB headers already present."
    exit 0
}
git clone --branch release_1.1 --depth 1 https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB
