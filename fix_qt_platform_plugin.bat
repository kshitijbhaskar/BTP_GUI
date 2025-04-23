@echo off
REM Script to fix Qt platform plugin issue by ensuring debug DLLs and plugins are used

setlocal enabledelayedexpansion

echo ===== Fix Qt Platform Plugin Script =====

REM Set paths
SET BUILD_DIR=%~dp0\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug
SET QT_DIR=C:\Users\kshit\Qt\6.10.0\msvc2022_64

echo BUILD_DIR: %BUILD_DIR%
echo QT_DIR: %QT_DIR%

if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory does not exist: %BUILD_DIR%
    goto :error
)

echo.
echo First, clean up existing Qt DLLs and plugins...

REM Delete existing Qt DLLs to avoid conflicts
del /Q "%BUILD_DIR%\Qt6*.dll"

REM Delete existing plugin directories to ensure clean state
if exist "%BUILD_DIR%\platforms" (
    rmdir /S /Q "%BUILD_DIR%\platforms"
)
if exist "%BUILD_DIR%\styles" (
    rmdir /S /Q "%BUILD_DIR%\styles"
)
if exist "%BUILD_DIR%\imageformats" (
    rmdir /S /Q "%BUILD_DIR%\imageformats"
)
if exist "%BUILD_DIR%\iconengines" (
    rmdir /S /Q "%BUILD_DIR%\iconengines"
)

echo.
echo Creating plugin directories...

REM Create necessary plugin directories
SET PLUGIN_DIRS=platforms styles imageformats iconengines

for %%D in (%PLUGIN_DIRS%) do (
    echo Creating %%D directory
    mkdir "%BUILD_DIR%\%%D"
)

echo.
echo Using debug configuration - searching for debug Qt DLLs and plugins...

REM Copy Qt debug DLLs (with 'd' suffix)
SET QT_DEBUG_DLLS=Qt6Cored.dll Qt6Guid.dll Qt6Widgetsd.dll Qt6OpenGLd.dll Qt6OpenGLWidgetsd.dll Qt6Svgd.dll Qt6Networkd.dll

for %%G in (%QT_DEBUG_DLLS%) do (
    if exist "%QT_DIR%\bin\%%G" (
        echo Copying %%G from %QT_DIR%\bin
        copy /Y "%QT_DIR%\bin\%%G" "%BUILD_DIR%\"
    ) else (
        echo WARNING: %%G not found at %QT_DIR%\bin
    )
)

echo.
echo Copying Qt debug platform plugins...

REM Copy platform plugins with debug suffix
if exist "%QT_DIR%\plugins\platforms\qwindowsd.dll" (
    echo Copying qwindowsd.dll to platforms directory
    copy /Y "%QT_DIR%\plugins\platforms\qwindowsd.dll" "%BUILD_DIR%\platforms\"
) else (
    echo WARNING: Debug Windows platform plugin not found, trying release version...
    if exist "%QT_DIR%\plugins\platforms\qwindows.dll" (
        echo Copying qwindows.dll to platforms directory
        copy /Y "%QT_DIR%\plugins\platforms\qwindows.dll" "%BUILD_DIR%\platforms\"
    ) else (
        echo ERROR: No Windows platform plugin found!
        goto :error
    )
)

echo.
echo Copying Qt image format plugins...

REM Copy imageformat plugins with debug suffix where possible
SET IMG_PLUGINS=qgifd.dll qicod.dll qjpegd.dll qsvgd.dll

for %%G in (%IMG_PLUGINS%) do (
    if exist "%QT_DIR%\plugins\imageformats\%%G" (
        echo Copying %%G to imageformats directory
        copy /Y "%QT_DIR%\plugins\imageformats\%%G" "%BUILD_DIR%\imageformats\"
    ) else (
        echo WARNING: Debug version of %%G not found, trying release version...
        set RELEASE_PLUGIN=%%~nG
        set RELEASE_PLUGIN=!RELEASE_PLUGIN:~0,-1!.dll
        if exist "%QT_DIR%\plugins\imageformats\!RELEASE_PLUGIN!" (
            echo Copying !RELEASE_PLUGIN! to imageformats directory
            copy /Y "%QT_DIR%\plugins\imageformats\!RELEASE_PLUGIN!" "%BUILD_DIR%\imageformats\"
        )
    )
)

echo.
echo Creating qt.conf file with explicit plugin paths...
echo [Paths] > "%BUILD_DIR%\qt.conf"
echo Prefix = . >> "%BUILD_DIR%\qt.conf"
echo Plugins = . >> "%BUILD_DIR%\qt.conf"
echo Binaries = . >> "%BUILD_DIR%\qt.conf"
echo Libraries = . >> "%BUILD_DIR%\qt.conf"

echo.
echo Verifying Qt environment variables...
echo Setting QT_DEBUG_PLUGINS environment variable to help with debugging...
setx QT_DEBUG_PLUGINS "1"
echo Setting QT_PLUGIN_PATH environment variable...
setx QT_PLUGIN_PATH "%BUILD_DIR%"

echo.
echo Verifying copied files...
echo.
echo Platform plugins:
dir "%BUILD_DIR%\platforms\*.dll"
echo.
echo Qt DLLs:
dir "%BUILD_DIR%\Qt6*.dll"
echo.
echo qt.conf file:
type "%BUILD_DIR%\qt.conf"

echo.
echo Script completed.
goto :end

:error
echo An error occurred during execution.

:end
endlocal
pause 