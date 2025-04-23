@echo off
REM Script to run the application with the correct Qt environment

setlocal

echo ===== Run with Qt Environment =====

REM Set paths
SET BUILD_DIR=%~dp0\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug
SET QT_DIR=C:\Users\kshit\Qt\6.10.0\msvc2022_64
SET PATH=%QT_DIR%\bin;%PATH%
SET QT_PLUGIN_PATH=%QT_DIR%\plugins
SET QT_QPA_PLATFORM_PLUGIN_PATH=%QT_DIR%\plugins\platforms

echo BUILD_DIR: %BUILD_DIR%
echo QT_DIR: %QT_DIR%
echo QT_PLUGIN_PATH: %QT_PLUGIN_PATH%
echo QT_QPA_PLATFORM_PLUGIN_PATH: %QT_QPA_PLATFORM_PLUGIN_PATH%

if not exist "%BUILD_DIR%\BTP_GUI.exe" (
    echo ERROR: Application executable not found at %BUILD_DIR%\BTP_GUI.exe
    goto :error
)

echo.
echo Running application with Qt environment...
cd "%BUILD_DIR%"

echo Setting QT_DEBUG_PLUGINS=1 to get verbose plugin loading info
set QT_DEBUG_PLUGINS=1

BTP_GUI.exe
if %ERRORLEVEL% NEQ 0 (
    echo Application exited with error code: %ERRORLEVEL%
)

goto :end

:error
echo An error occurred during execution.

:end
endlocal
pause 