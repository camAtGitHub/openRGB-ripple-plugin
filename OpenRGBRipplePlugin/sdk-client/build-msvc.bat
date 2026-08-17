@echo off
setlocal EnableExtensions
cd /d "%~dp0"

rem --- Load the MSVC + Windows SDK environment if cl is not ready ---
where cl >nul 2>&1
if errorlevel 1 (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
  if not exist "%VSWHERE%" goto :no_vs

  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
  if not defined VSINSTALL (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSINSTALL=%%i"
  )
  if not defined VSINSTALL goto :no_workload

  if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" (
    echo Using Visual Studio at "%VSINSTALL%"
    call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
  ) else if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
    echo Using VsDevCmd at "%VSINSTALL%"
    call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul
  ) else (
    goto :no_workload
  )
)

where cl >nul 2>&1
if errorlevel 1 goto :no_workload

rem winsock2.h lives in the Windows SDK; cstdint lives in the MSVC STL.
if not exist "%INCLUDE%\winsock2.h" (
  if not exist "%WindowsSdkDir%include\%WindowsSDKVersion%um\winsock2.h" goto :no_sdk
)
if not exist "%VCToolsInstallDir%include\cstdint" (
  if not exist "%VCINSTALLDIR%Tools\MSVC" goto :no_workload
)

echo Compiling OpenRGBRipple.exe ^(x64, MSVC^)...
cl /nologo /EHsc /std:c++17 /O2 /DUNICODE /D_UNICODE /I.. ^
  OpenRGBRippleClient.cpp ..\KeyMap.cpp ^
  ws2_32.lib user32.lib shell32.lib /Fe:OpenRGBRipple.exe
if errorlevel 1 exit /b 1

echo.
echo Built %CD%\OpenRGBRipple.exe
echo Start OpenRGB with SDK Server enabled, then run OpenRGBRipple.exe
exit /b 0

:no_vs
echo.
echo Visual Studio was not found.
echo Install "Build Tools for Visual Studio" or Visual Studio, then
echo tick the workload: Desktop development with C++
echo https://visualstudio.microsoft.com/visual-cpp-build-tools/
exit /b 1

:no_workload
echo.
echo cl.exe / the C++ toolset is missing.
echo Visual Studio is installed, but not the C++ bits.
echo.
echo Fix: open Visual Studio Installer -^> Modify
echo   - Desktop development with C++
echo   - MSVC v143 ^(or v142^) x64/x86 build tools
echo   - Windows 10/11 SDK
echo Then run this script again from a normal cmd ^(it will find vcvars64^).
echo.
echo Or open "x64 Native Tools Command Prompt for VS" and run:
echo   cd /d "%~dp0"
echo   cl /EHsc /std:c++17 /O2 /I.. OpenRGBRippleClient.cpp ..\KeyMap.cpp ws2_32.lib user32.lib /Fe:OpenRGBRipple.exe
exit /b 1

:no_sdk
echo.
echo winsock2.h not found — Windows SDK is not on INCLUDE.
echo Visual Studio Installer -^> Modify -^> Windows 10/11 SDK
echo Then use "x64 Native Tools Command Prompt for VS", not a plain terminal.
exit /b 1
