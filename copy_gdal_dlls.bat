@echo off
REM Script to copy GDAL DLLs to the build directory

echo ===== Copy GDAL DLLs Script =====

REM Set paths
SET BUILD_DIR=%~dp0\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug
SET OSGEO4W_DIR=C:\OSGeo4W
SET OSGEO4W_BIN=%OSGEO4W_DIR%\bin
SET OSGEO4W_APPS_GDAL_DEV=%OSGEO4W_DIR%\apps\gdal-dev\bin

echo BUILD_DIR: %BUILD_DIR%
echo OSGEO4W_BIN: %OSGEO4W_BIN%
echo OSGEO4W_APPS_GDAL_DEV: %OSGEO4W_APPS_GDAL_DEV%

if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory does not exist: %BUILD_DIR%
    mkdir "%BUILD_DIR%"
    if errorlevel 1 (
        echo Failed to create build directory.
        goto :error
    )
    echo Created build directory.
)

echo.
echo Copying GDAL DLLs...

REM Copy GDAL DLLs
SET GDAL_DLL_FOUND=0
if exist "%OSGEO4W_BIN%\gdal310.dll" (
    echo Copying gdal310.dll from %OSGEO4W_BIN%
    copy /Y "%OSGEO4W_BIN%\gdal310.dll" "%BUILD_DIR%\"
    SET GDAL_DLL_FOUND=1
)

if exist "%OSGEO4W_APPS_GDAL_DEV%\gdal-dev311.dll" (
    echo Copying gdal-dev311.dll from %OSGEO4W_APPS_GDAL_DEV%
    copy /Y "%OSGEO4W_APPS_GDAL_DEV%\gdal-dev311.dll" "%BUILD_DIR%\"
    SET GDAL_DLL_FOUND=1
)

echo.
echo Copying dependency DLLs...

REM Copy dependency DLLs
SET DEPENDENCY_DLLS=libxml2.dll spatialite.dll iconv.dll freexl.dll proj_9.dll sqlite3.dll zlib1.dll geos_c.dll
for %%G in (%DEPENDENCY_DLLS%) do (
    if exist "%OSGEO4W_BIN%\%%G" (
        echo Copying %%G from %OSGEO4W_BIN%
        copy /Y "%OSGEO4W_BIN%\%%G" "%BUILD_DIR%\"
    )
)

echo.
echo Verifying copied files...
dir "%BUILD_DIR%\*.dll" | findstr /i "gdal spatialite libxml2"

if %GDAL_DLL_FOUND%==0 (
    echo WARNING: No GDAL DLLs were found to copy.
) else (
    echo GDAL DLLs copied successfully.
)

echo.
echo Script completed.
goto :end

:error
echo An error occurred during execution.

:end
pause 