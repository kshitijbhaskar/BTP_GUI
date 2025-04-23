@echo off
REM Script to copy Qt plugins and DLLs to the build directory

echo ===== Copy Qt Plugins Script =====

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
echo Creating plugin directories...

REM Create necessary plugin directories
SET PLUGIN_DIRS=platforms styles imageformats iconengines

for %%D in (%PLUGIN_DIRS%) do (
    if not exist "%BUILD_DIR%\%%D" (
        echo Creating %%D directory
        mkdir "%BUILD_DIR%\%%D"
    )
)

echo.
echo Copying Qt platform plugins...

REM Copy platform plugins
if exist "%QT_DIR%\plugins\platforms\qwindows.dll" (
    echo Copying qwindows.dll to platforms directory
    copy /Y "%QT_DIR%\plugins\platforms\qwindows.dll" "%BUILD_DIR%\platforms\"
) else (
    echo ERROR: Windows platform plugin not found at %QT_DIR%\plugins\platforms\qwindows.dll
    goto :error
)

echo.
echo Copying Qt style plugins...

REM Copy style plugins
if exist "%QT_DIR%\plugins\styles\qwindowsvistastyle.dll" (
    echo Copying qwindowsvistastyle.dll to styles directory
    copy /Y "%QT_DIR%\plugins\styles\qwindowsvistastyle.dll" "%BUILD_DIR%\styles\"
)

echo.
echo Copying Qt image format plugins...

REM Copy imageformat plugins
SET IMG_PLUGINS=qgif.dll qico.dll qjpeg.dll qsvg.dll

for %%G in (%IMG_PLUGINS%) do (
    if exist "%QT_DIR%\plugins\imageformats\%%G" (
        echo Copying %%G to imageformats directory
        copy /Y "%QT_DIR%\plugins\imageformats\%%G" "%BUILD_DIR%\imageformats\"
    )
)

echo.
echo Copying Qt icon engine plugins...

REM Copy iconengine plugins
if exist "%QT_DIR%\plugins\iconengines\qsvgicon.dll" (
    echo Copying qsvgicon.dll to iconengines directory
    copy /Y "%QT_DIR%\plugins\iconengines\qsvgicon.dll" "%BUILD_DIR%\iconengines\"
)

echo.
echo Copying essential Qt DLLs...

REM List of essential Qt DLLs
SET QT_DLLS=Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6OpenGL.dll Qt6OpenGLWidgets.dll Qt6Svg.dll Qt6Network.dll Qt6Sql.dll

for %%G in (%QT_DLLS%) do (
    if exist "%QT_DIR%\bin\%%G" (
        echo Copying %%G from %QT_DIR%\bin
        copy /Y "%QT_DIR%\bin\%%G" "%BUILD_DIR%\"
    ) else (
        echo WARNING: %%G not found at %QT_DIR%\bin
    )
)

echo.
echo Creating qt.conf file for plugin paths...
echo [Paths] > "%BUILD_DIR%\qt.conf"
echo Plugins = . >> "%BUILD_DIR%\qt.conf"

echo.
echo Verifying copied files...
echo.
echo Platform plugins:
dir "%BUILD_DIR%\platforms\*.dll"
echo.
echo Style plugins:
dir "%BUILD_DIR%\styles\*.dll"
echo.
echo Image format plugins:
dir "%BUILD_DIR%\imageformats\*.dll"
echo.
echo Icon engine plugins:
dir "%BUILD_DIR%\iconengines\*.dll"
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
pause 