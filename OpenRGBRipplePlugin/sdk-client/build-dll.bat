@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not defined VCINSTALLDIR (
  if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  ) else if exist "%~dp0build-msvc.bat" (
    echo Run this from an x64 Native Tools prompt, or run build-msvc.bat first.
  )
)

echo Building OpenRGBRipple.dll ...
cl /nologo /LD /EHsc /std:c++17 /O2 /DUNICODE /D_UNICODE /DOPENRGB_RIPPLE_DLL /I.. ^
  OpenRGBRippleClient.cpp ..\KeyMap.cpp ^
  ws2_32.lib user32.lib shell32.lib /Fe:OpenRGBRipple.dll
if errorlevel 1 exit /b 1

echo.
echo Built %CD%\OpenRGBRipple.dll
echo.
echo This is the SDK client as a DLL. It is NOT an OpenRGB plugin.
echo OpenRGB will not load it from the plugins folder ^(that needs Qt 5.15^).
echo.
echo Run it with:
echo   rundll32 "%CD%\OpenRGBRipple.dll",Start
echo.
echo OpenRGB must already be running with SDK Server enabled.
exit /b 0
