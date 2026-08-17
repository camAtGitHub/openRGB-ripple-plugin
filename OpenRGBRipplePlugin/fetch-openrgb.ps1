# Clone OpenRGB 1.0rc2 headers (Plugin API 4) next to this script.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
if (Test-Path ".\OpenRGB\OpenRGBPluginInterface.h") {
    Write-Host "OpenRGB headers already present."
    exit 0
}
git clone --branch release_candidate_1.0rc2 --depth 1 https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB
