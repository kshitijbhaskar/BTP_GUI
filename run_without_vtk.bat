@echo off
echo ===== BTP_GUI - Running with VTK Disabled =====
echo.
echo This batch file runs the application with VTK 3D visualization disabled.
echo Use this if you encounter OpenGL-related crashes.
echo.

cd /d "%~dp0\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug"

if not exist "BTP_GUI.exe" (
    echo ERROR: Application executable not found at %CD%\BTP_GUI.exe
    goto :error
)

REM Set environment variable to force disable VTK
set BTP_GUI_DISABLE_VTK=1

REM Run with VTK disabled
echo Starting application with VTK disabled...
BTP_GUI.exe --no-vtk

if %ERRORLEVEL% NEQ 0 (
    echo Application exited with error code: %ERRORLEVEL%
) else (
    echo Application closed successfully.
)

goto :end

:error
echo An error occurred.

:end
pause 