@echo off
echo =============================================
echo BTP_GUI Environment Setup and Fixing Script
echo =============================================

set BUILD_DIR=C:\Users\kshit\Documents\BTP GUI\BTP_GUI\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug
set OSGEO4W_DIR=C:\OSGeo4W
set QT_DIR=C:\Users\kshit\Qt\6.10.0\msvc2022_64

echo.
echo Step 1: Checking and fixing GDAL DLL...
echo -------------------------------------

REM Check if gdal310.dll exists in OSGeo4W directory
if exist "%OSGEO4W_DIR%\bin\gdal310.dll" (
    echo Found GDAL DLL at %OSGEO4W_DIR%\bin\gdal310.dll
    
    REM Copy it to the build directory if not already there
    if not exist "%BUILD_DIR%\gdal310.dll" (
        echo Copying gdal310.dll to build directory...
        copy "%OSGEO4W_DIR%\bin\gdal310.dll" "%BUILD_DIR%\gdal310.dll"
    ) else (
        echo gdal310.dll already exists in build directory.
    )
    
    REM Create a copy named gdal.dll
    echo Creating a copy named gdal.dll...
    copy "%OSGEO4W_DIR%\bin\gdal310.dll" "%BUILD_DIR%\gdal.dll"
    echo GDAL DLL should now be accessible as both gdal310.dll and gdal.dll
) else (
    echo ERROR: GDAL DLL not found at %OSGEO4W_DIR%\bin\gdal310.dll
)

echo.
echo Step 2: Fixing Qt plugins and DLLs...
echo ----------------------------------

REM Remove existing Qt DLLs and plugins to avoid conflicts
echo Cleaning up existing Qt files...
if exist "%BUILD_DIR%\platforms" (
    rmdir /s /q "%BUILD_DIR%\platforms"
)
if exist "%BUILD_DIR%\styles" (
    rmdir /s /q "%BUILD_DIR%\styles"
)
if exist "%BUILD_DIR%\imageformats" (
    rmdir /s /q "%BUILD_DIR%\imageformats"
)
if exist "%BUILD_DIR%\iconengines" (
    rmdir /s /q "%BUILD_DIR%\iconengines"
)

REM Create plugin directories
echo Creating plugin directories...
mkdir "%BUILD_DIR%\platforms"
mkdir "%BUILD_DIR%\styles"
mkdir "%BUILD_DIR%\imageformats"
mkdir "%BUILD_DIR%\iconengines"

REM Copy the debug Qt platform plugin
echo Copying Qt debug platform plugin...
if exist "%QT_DIR%\plugins\platforms\qwindowsd.dll" (
    copy "%QT_DIR%\plugins\platforms\qwindowsd.dll" "%BUILD_DIR%\platforms\qwindowsd.dll"
    echo Copied qwindowsd.dll
) else (
    echo WARNING: Debug plugin qwindowsd.dll not found! 
    if exist "%QT_DIR%\plugins\platforms\qwindows.dll" (
        copy "%QT_DIR%\plugins\platforms\qwindows.dll" "%BUILD_DIR%\platforms\qwindows.dll"
        echo Using release plugin qwindows.dll instead
    ) else (
        echo ERROR: No platform plugin found!
    )
)

REM Copy essential Qt debug DLLs
echo Copying essential Qt debug DLLs...
set QT_DLLS=Qt6Cored.dll Qt6Guid.dll Qt6Widgetsd.dll Qt6OpenGLd.dll Qt6OpenGLWidgetsd.dll Qt6Svgd.dll Qt6Networkd.dll

for %%D in (%QT_DLLS%) do (
    if exist "%QT_DIR%\bin\%%D" (
        if not exist "%BUILD_DIR%\%%D" (
            copy "%QT_DIR%\bin\%%D" "%BUILD_DIR%\%%D"
            echo Copied %%D
        ) else (
            echo %%D already exists
        )
    ) else (
        echo WARNING: %%D not found at %QT_DIR%\bin\%%D
    )
)

REM Create qt.conf file
echo Creating qt.conf file...
echo [Paths] > "%BUILD_DIR%\qt.conf"
echo Prefix = . >> "%BUILD_DIR%\qt.conf"
echo Plugins = . >> "%BUILD_DIR%\qt.conf"
echo Binaries = . >> "%BUILD_DIR%\qt.conf"
echo Libraries = . >> "%BUILD_DIR%\qt.conf"

echo.
echo Step 3: Verification...
echo --------------------

REM Verify that GDAL DLLs exist
if exist "%BUILD_DIR%\gdal.dll" (
    echo ✓ gdal.dll is present
) else (
    echo ✗ gdal.dll is missing!
)

if exist "%BUILD_DIR%\gdal310.dll" (
    echo ✓ gdal310.dll is present
) else (
    echo ✗ gdal310.dll is missing!
)

REM Verify Qt platform plugin
if exist "%BUILD_DIR%\platforms\qwindowsd.dll" (
    echo ✓ Debug platform plugin (qwindowsd.dll) is present
) else (
    if exist "%BUILD_DIR%\platforms\qwindows.dll" (
        echo ✓ Release platform plugin (qwindows.dll) is present
    ) else (
        echo ✗ No Qt platform plugin found!
    )
)

echo.
echo Setup complete! Try running the application now.
echo If there are still issues, check the console output for more details.
echo =============================================

pause 