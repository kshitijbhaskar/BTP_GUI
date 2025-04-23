@echo off
REM Dependencies verification script for BTP_GUI application

echo ===== BTP_GUI Dependency Verification Script =====
echo.

REM Set paths
SET OSGEO4W_DIR=C:\OSGeo4W
SET OSGEO4W_BIN=%OSGEO4W_DIR%\bin
SET OSGEO4W_APPS_GDAL_DEV=%OSGEO4W_DIR%\apps\gdal-dev
SET VCPKG_DIR=C:\vcpkg
SET QT_DIR=C:\Users\kshit\Qt\6.10.0\msvc2022_64
SET BUILD_DIR=%~dp0\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug

echo Checking for critical directories...
echo.

if exist "%OSGEO4W_DIR%" (
    echo [√] OSGeo4W directory found: %OSGEO4W_DIR%
) else (
    echo [X] OSGeo4W directory not found: %OSGEO4W_DIR%
)

if exist "%OSGEO4W_APPS_GDAL_DEV%" (
    echo [√] GDAL-Dev directory found: %OSGEO4W_APPS_GDAL_DEV%
) else (
    echo [X] GDAL-Dev directory not found: %OSGEO4W_APPS_GDAL_DEV%
)

if exist "%QT_DIR%" (
    echo [√] Qt directory found: %QT_DIR%
) else (
    echo [X] Qt directory not found: %QT_DIR%
)

if exist "%VCPKG_DIR%" (
    echo [√] VCPKG directory found: %VCPKG_DIR%
) else (
    echo [X] VCPKG directory not found: %VCPKG_DIR%
)

echo.
echo Checking for GDAL library files...
echo.

if exist "%OSGEO4W_APPS_GDAL_DEV%\lib\gdal.lib" (
    echo [√] GDAL library found: %OSGEO4W_APPS_GDAL_DEV%\lib\gdal.lib
) else (
    echo [X] GDAL library not found: %OSGEO4W_APPS_GDAL_DEV%\lib\gdal.lib
)

if exist "%OSGEO4W_APPS_GDAL_DEV%\include\gdal.h" (
    echo [√] GDAL header found: %OSGEO4W_APPS_GDAL_DEV%\include\gdal.h
) else (
    echo [X] GDAL header not found: %OSGEO4W_APPS_GDAL_DEV%\include\gdal.h
)

echo.
echo Checking for GDAL DLL files...
echo.

SET FOUND_GDAL_DLL=0
if exist "%OSGEO4W_BIN%\gdal310.dll" (
    echo [√] GDAL DLL found: %OSGEO4W_BIN%\gdal310.dll
    SET FOUND_GDAL_DLL=1
)

if exist "%OSGEO4W_APPS_GDAL_DEV%\bin\gdal-dev311.dll" (
    echo [√] GDAL-Dev DLL found: %OSGEO4W_APPS_GDAL_DEV%\bin\gdal-dev311.dll
    SET FOUND_GDAL_DLL=1
)

if %FOUND_GDAL_DLL%==0 (
    echo [X] No GDAL DLLs found
)

echo.
echo Checking for dependency DLLs...
echo.

SET DEPENDENCY_DLLS=libxml2.dll spatialite.dll iconv.dll freexl.dll proj_9.dll sqlite3.dll zlib1.dll geos_c.dll
FOR %%D IN (%DEPENDENCY_DLLS%) DO (
    if exist "%OSGEO4W_BIN%\%%D" (
        echo [√] Found dependency: %OSGEO4W_BIN%\%%D
    ) else (
        echo [X] Missing dependency: %OSGEO4W_BIN%\%%D
    )
)

echo.
echo Checking for Qt and VTK modules...
echo.

if exist "%QT_DIR%\lib\cmake\Qt6Widgets" (
    echo [√] Qt6Widgets module found
) else (
    echo [X] Qt6Widgets module not found
)

if exist "%QT_DIR%\lib\cmake\Qt6LinguistTools" (
    echo [√] Qt6LinguistTools module found
) else (
    echo [X] Qt6LinguistTools module not found
)

if exist "%VCPKG_DIR%\installed\x64-windows\share\vtk" (
    echo [√] VTK package found in vcpkg
) else (
    echo [X] VTK package not found in vcpkg
)

echo.
echo ===== Recommended CMake Configuration =====
echo.
echo Use these variables in your CMake configuration:
echo.
echo -DGDAL_LIBRARY:FILEPATH="C:/OSGeo4W/apps/gdal-dev/lib/gdal.lib"
echo -DGDAL_INCLUDE_DIR:PATH="C:/OSGeo4W/apps/gdal-dev/include"
echo -DCMAKE_PREFIX_PATH:PATH="%QT_DIR%;C:/vcpkg/installed/x64-windows"
echo -DVTK_DIR:PATH="C:/vcpkg/installed/x64-windows/share/vtk"
echo -DQt6LinguistTools_DIR:PATH="%QT_DIR%/lib/cmake/Qt6LinguistTools"
echo.
echo ===== Script complete =====

pause 