@echo off
setlocal
cd /d "%~dp0.."
if not exist build-tests mkdir build-tests
where cl >nul 2>nul
if not errorlevel 1 goto :compile
if exist "C:\BuildTools\Common7\Tools\VsDevCmd.bat" (
    call "C:\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
) else (
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%I\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
)
if errorlevel 1 exit /b 1
:compile
cl /nologo /W4 /WX /std:c11 /Isrc tests\switch_pro_test.c src\switch_pro.c /Fo:build-tests\ /Fe:build-tests\switch_pro_test.exe
if errorlevel 1 exit /b 1
build-tests\switch_pro_test.exe
