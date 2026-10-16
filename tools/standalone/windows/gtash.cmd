@echo off
rem Opens Git Bash with the MSVC x86 build environment (toolset pinned to the one the port is verified with).
rem Start Claude Code from THIS shell: every build/test/soak the agent runs then inherits cl, link, ninja, cmake and the right toolset.
rem Overrides (set before running): GTA_MSVC_VER (default 14.51), GTA_GIT_BASH (default %ProgramFiles%\Git\bin\bash.exe), GTA_VSINSTALL (default: newest VS found by vswhere).
setlocal
if "%GTA_MSVC_VER%"=="" set "GTA_MSVC_VER=14.51"
if "%GTA_GIT_BASH%"=="" set "GTA_GIT_BASH=%ProgramFiles%\Git\bin\bash.exe"
set "GTA_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if "%GTA_VSINSTALL%"=="" for /f "usebackq delims=" %%i in (`"%GTA_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GTA_VSINSTALL=%%i"
if "%GTA_VSINSTALL%"=="" goto novs
call "%GTA_VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86 -vcvars_ver=%GTA_MSVC_VER%
if errorlevel 1 goto novc
cd /d "%~dp0..\..\.."
cl 2>&1 | findstr /i "Version"
"%GTA_GIT_BASH%" --login -i
exit /b 0
:novs
echo gtash: no Visual Studio with the C++ x86/x64 tools found - see docs\WINDOWS_SETUP.md
exit /b 1
:novc
echo gtash: vcvarsall failed - is toolset %GTA_MSVC_VER% installed? see docs\WINDOWS_SETUP.md
exit /b 1
