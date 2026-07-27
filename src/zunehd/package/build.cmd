@echo off
setlocal
cd /d "%~dp0"

set "BUILD_CONFIGURATION=Release"
set "LAUNCHER_BIN=..\launcher\bin\Zune\Release"
set "NATIVE_BIN=..\native\bin\OpenZDK (ARMV4I)\Release"
call :has_required_build
if errorlevel 1 (
    set "BUILD_CONFIGURATION=Debug"
    set "LAUNCHER_BIN=..\launcher\bin\Zune\Debug"
    set "NATIVE_BIN=..\native\bin\OpenZDK (ARMV4I)\Debug"
    call :has_required_build
    if errorlevel 1 (
        echo No build, no package, sorry
        exit /b 1
    )
)

echo BUILD_CONFIGURATION = %BUILD_CONFIGURATION%
if exist data rmdir /s /q data
mkdir data\Content
copy /y "%LAUNCHER_BIN%\exploiter.exe" data\exploiter.exe >nul
if errorlevel 1 exit /b 1
copy /y "%NATIVE_BIN%\threedee.exe" data\Content\threedee.exe >nul
if errorlevel 1 exit /b 1
copy /y "%NATIVE_BIN%\fragment.nvbf" data\Content\fragment.nvbf >nul
if errorlevel 1 exit /b 1
copy /y "%NATIVE_BIN%\vertex.nvbv" data\Content\vertex.nvbv >nul
if errorlevel 1 exit /b 1

set "ARCHIVER=%~dp07zr.exe"

if exist zuco8.7z del /q zuco8.7z
if exist zuco8-installer.exe del /q zuco8-installer.exe

"%ARCHIVER%" a -t7z -mx=0 zuco8.7z application.cfg thumbnail.png data
if errorlevel 1 exit /b 1

pushd DeployKit\bin
"%ARCHIVER%" a -t7z -mx=0 ..\..\zuco8.7z deploykit.exe dengine.dll
set "ARCHIVE_RC=%ERRORLEVEL%"
popd
if not "%ARCHIVE_RC%"=="0" exit /b %ARCHIVE_RC%

copy /y /b DeployKit\sfx\extractor.sfx + DeployKit\sfx\config.txt + zuco8.7z zuco8-installer.exe
exit /b %ERRORLEVEL%

:has_required_build
if not exist "%LAUNCHER_BIN%\exploiter.exe" exit /b 1
if not exist "%NATIVE_BIN%\threedee.exe" exit /b 1
if not exist "%NATIVE_BIN%\fragment.nvbf" exit /b 1
if not exist "%NATIVE_BIN%\vertex.nvbv" exit /b 1
exit /b 0
