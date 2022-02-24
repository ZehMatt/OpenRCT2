@echo off

rem Invokes a tool within a Visual Studio prompt
rem Uses %PLATFORM% to set architecture of prompt
@echo off
setlocal enabledelayedexpansion

for /f "usebackq tokens=*" %%i in (`vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
  "%%i" %*
  exit /b !errorlevel!
)

set "vspath=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise"
if exist "%vspath%" goto found
set "vspath=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
if exist "%vspath%" goto found

:notfound
echo Visual Studio directory not found
exit /b 1

:found
if "%platform%"=="x64" (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64
) else (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" -no_logo
)

%*
exit /b %errorlevel%
