@echo off
REM Deployment script for BTP_GUI application
REM This script copies all necessary dependencies to the build directory

echo ===== BTP_GUI Deployment Script =====

REM Set paths - adjust these to match your actual setup
SET BUILD_DIR=%~dp0\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug
SET OSGEO4W_DIR=C:\OSGeo4W
SET OSGEO4W_BIN=%OSGEO4W_DIR%\bin
SET OSGEO4W_APPS_GDAL_BIN=%OSGEO4W_DIR%\apps\gdal-dev\bin
SET VTK_BIN=C:\vcpkg\installed\x64-windows\debug\bin
SET QT_BIN=C:\vcpkg\installed\x64-windows\debug\bin

echo BUILD_DIR: %BUILD_DIR%
echo OSGEO4W_BIN: %OSGEO4W_BIN%
echo OSGEO4W_APPS_GDAL_BIN: %OSGEO4W_APPS_GDAL_BIN%

if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory does not exist: %BUILD_DIR%
    goto :error
)

echo.
echo Checking for GDAL DLL... (this is key for the application)

REM Check for GDAL DLL in various locations
SET GDAL_DLL_FOUND=0
if exist "%OSGEO4W_BIN%\gdal.dll" (
    echo Found GDAL DLL at %OSGEO4W_BIN%\gdal.dll
    copy /Y "%OSGEO4W_BIN%\gdal.dll" "%BUILD_DIR%\"
    SET GDAL_DLL_FOUND=1
) else if exist "%OSGEO4W_APPS_GDAL_BIN%\gdal.dll" (
    echo Found GDAL DLL at %OSGEO4W_APPS_GDAL_BIN%\gdal.dll
    copy /Y "%OSGEO4W_APPS_GDAL_BIN%\gdal.dll" "%BUILD_DIR%\"
    SET GDAL_DLL_FOUND=1
) else (
    echo WARNING: GDAL DLL not found in expected locations
)

echo.
echo Copying OSGeo4W dependencies...

REM Check each key DLL separately and copy if found
SET DLLS_TO_CHECK=libxml2.dll spatialite.dll iconv.dll freexl.dll proj_9_5.dll sqlite3.dll zlib1.dll geos_c.dll geos.dll libcurl.dll

for %%G in (%DLLS_TO_CHECK%) do (
    echo Checking for %%G...
    
    if exist "%OSGEO4W_BIN%\%%G" (
        echo Found %%G at %OSGEO4W_BIN%\%%G
        copy /Y "%OSGEO4W_BIN%\%%G" "%BUILD_DIR%\"
    ) else if exist "%OSGEO4W_APPS_GDAL_BIN%\%%G" (
        echo Found %%G at %OSGEO4W_APPS_GDAL_BIN%\%%G
        copy /Y "%OSGEO4W_APPS_GDAL_BIN%\%%G" "%BUILD_DIR%\"
    ) else (
        echo WARNING: %%G not found in expected locations
    )
)

echo.
echo Copy complete. Checking the target directory...
dir /b "%BUILD_DIR%\*.dll" | findstr /i "gdal spatialite libxml2"

echo.
if %GDAL_DLL_FOUND%==0 (
    echo WARNING: GDAL DLL was not found and copied. Application may not run correctly.
) else (
    echo All critical DLLs copied successfully.
)

echo.
echo Script completed.
echo To run the application: %BUILD_DIR%\BTP_GUI.exe
echo.
goto :end

:error
echo An error occurred during deployment.

:end
pause 