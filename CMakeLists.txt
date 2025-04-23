cmake_minimum_required(VERSION 3.21)

# Set policy to allow using VERSION in project()
cmake_policy(SET CMP0048 NEW)

project(BTP_GUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Clear and set the correct Qt paths - make sure these paths exist
set(Qt6_DIR "C:/Users/kshit/Qt/6.10.0/msvc2022_64/lib/cmake/Qt6")
set(Qt6Core_DIR "C:/Users/kshit/Qt/6.10.0/msvc2022_64/lib/cmake/Qt6Core")
set(Qt6Gui_DIR "C:/Users/kshit/Qt/6.10.0/msvc2022_64/lib/cmake/Qt6Gui")
set(Qt6Widgets_DIR "C:/Users/kshit/Qt/6.10.0/msvc2022_64/lib/cmake/Qt6Widgets")
set(Qt6LinguistTools_DIR "C:/Users/kshit/Qt/6.10.0/msvc2022_64/lib/cmake/Qt6LinguistTools")

# Set the prefix path - add the OSGeo4W path as well
list(APPEND CMAKE_PREFIX_PATH 
    "C:/Users/kshit/Qt/6.10.0/msvc2022_64"
    "C:/OSGeo4W"
    "C:/vcpkg/installed/x64-windows"
)

# Add a message to debug Qt paths
message(STATUS "Qt6_DIR: ${Qt6_DIR}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

# Find Qt packages with explicit version
find_package(Qt6 6.10.0 COMPONENTS Core Gui Widgets REQUIRED)
find_package(Qt6LinguistTools REQUIRED)

# Output Qt version information
message(STATUS "Using Qt version: ${Qt6_VERSION}")
message(STATUS "Qt libraries directory: ${Qt6_DIR}")

# Find GDAL - specify the correct paths if not found automatically
find_package(GDAL REQUIRED)

# Ensure GDAL paths are correct and stripped of quotes
if(GDAL_FOUND)
    message(STATUS "GDAL found:")
    message(STATUS "  GDAL_INCLUDE_DIR: ${GDAL_INCLUDE_DIR}")
    message(STATUS "  GDAL_LIBRARY: ${GDAL_LIBRARY}")
    
    # Strip quotes from GDAL paths
    string(REPLACE "\"" "" GDAL_INCLUDE_DIR "${GDAL_INCLUDE_DIR}")
    string(REPLACE "\"" "" GDAL_LIBRARY "${GDAL_LIBRARY}")
    
    # Check for .lib extension on Windows
    if(WIN32 AND NOT GDAL_LIBRARY MATCHES "[.]lib$")
        set(GDAL_LIBRARY "${GDAL_LIBRARY}.lib")
    endif()
else()
    # If GDAL not found automatically, set explicit paths
    set(GDAL_INCLUDE_DIR "C:/OSGeo4W/apps/gdal-dev/include")
    set(GDAL_LIBRARY "C:/OSGeo4W/apps/gdal-dev/lib/gdal.lib")
    message(STATUS "GDAL not found automatically, using explicit paths:")
    message(STATUS "  GDAL_INCLUDE_DIR: ${GDAL_INCLUDE_DIR}")
    message(STATUS "  GDAL_LIBRARY: ${GDAL_LIBRARY}")
endif()

# Explicitly add GDAL_INCLUDE_DIR to include path
include_directories(${GDAL_INCLUDE_DIR})

# Define GDAL_DYNAMIC to ensure proper DLL import/export
add_definitions(-DGDAL_DYNAMIC)

# Find VTK components
find_package(VTK REQUIRED COMPONENTS 
    CommonCore
    CommonDataModel
    CommonTransforms
    FiltersCore
    FiltersGeneral
    FiltersSources
    InteractionStyle
    InteractionWidgets
    IOImage
    IOXML
    RenderingCore
    RenderingFreeType
    RenderingGL2PSOpenGL2
    RenderingOpenGL2
    RenderingAnnotation
    GUISupportQt
)

# Check if VTK was found properly
if(NOT VTK_FOUND)
    message(FATAL_ERROR "VTK not found. Please install VTK development files.")
endif()

# Include VTK's CMake features
include(${VTK_USE_FILE})

# Debug output to see what's found
message(STATUS "VTK Libraries: ${VTK_LIBRARIES}")

set(TS_FILES BTP_GUI_en_IN.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        SimulationEngine.cpp
        SimulationEngine.h
        # Add new VTK visualization classes
        VTKTerrainVisualizer.cpp
        VTKTerrainVisualizer.h
        VTKWaterVisualizer.cpp
        VTKWaterVisualizer.h
        ${TS_FILES}
)

qt_add_executable(BTP_GUI
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)
qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

# Add GDAL include directories
target_include_directories(BTP_GUI PRIVATE ${GDAL_INCLUDE_DIR})

target_link_libraries(BTP_GUI PRIVATE 
    Qt6::Widgets
    ${GDAL_LIBRARY}
    ${VTK_LIBRARIES}
)

# Set target properties
set_target_properties(BTP_GUI PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Installation
include(GNUInstallDirs)
install(TARGETS BTP_GUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(BTP_GUI)

# Add code to copy GDAL and Qt DLLs to the output directory
if(WIN32)
    # Determine if debug or release build
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(QT_DEBUG_SUFFIX "d")
        message(STATUS "Debug build - using debug Qt libraries")
    else()
        set(QT_DEBUG_SUFFIX "")
        message(STATUS "Release build - using release Qt libraries")
    endif()
    
    # Find the GDAL DLLs
    set(GDAL_DLL_PATHS
        "C:/OSGeo4W/bin/gdal310.dll"
        "C:/OSGeo4W/apps/gdal-dev/bin/gdal-dev311.dll"
    )
    
    # Check and copy the first GDAL DLL that exists
    foreach(GDAL_DLL ${GDAL_DLL_PATHS})
        if(EXISTS "${GDAL_DLL}")
            message(STATUS "Found GDAL DLL at ${GDAL_DLL}")
            # Copy the DLL with its original name
            add_custom_command(TARGET BTP_GUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${GDAL_DLL}"
                    $<TARGET_FILE_DIR:BTP_GUI>
                COMMENT "Copying GDAL DLL to output directory"
            )
            
            # Also create a copy named gdal.dll (which the app is looking for)
            get_filename_component(GDAL_DLL_FILENAME "${GDAL_DLL}" NAME)
            add_custom_command(TARGET BTP_GUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${GDAL_DLL}"
                    "$<TARGET_FILE_DIR:BTP_GUI>/gdal.dll"
                COMMENT "Creating a copy named gdal.dll from ${GDAL_DLL_FILENAME}"
            )
            
            break()
        endif()
    endforeach()
    
    # If no GDAL DLL was found, display a warning
    if(NOT EXISTS "${GDAL_DLL}")
        message(WARNING "GDAL DLL not found at C:/OSGeo4W/apps/gdal-dev/bin/gdal.dll or C:/OSGeo4W/bin/gdal.dll. You may need to manually copy it to the executable directory.")
    endif()
    
    # Copy additional OSGeo4W DLLs that might be needed by GDAL
    set(OSGEO4W_DLLS
        "C:/OSGeo4W/bin/spatialite.dll"
        "C:/OSGeo4W/bin/libxml2.dll"
        "C:/OSGeo4W/bin/iconv.dll"
        "C:/OSGeo4W/bin/freexl.dll"
        "C:/OSGeo4W/bin/proj_9.dll"
        "C:/OSGeo4W/bin/sqlite3.dll"
        "C:/OSGeo4W/bin/zlib1.dll"
        "C:/OSGeo4W/bin/geos_c.dll"
    )
    
    foreach(DLL ${OSGEO4W_DLLS})
        if(EXISTS "${DLL}")
            message(STATUS "Found additional DLL: ${DLL}")
            add_custom_command(TARGET BTP_GUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL}"
                    $<TARGET_FILE_DIR:BTP_GUI>
                COMMENT "Copying ${DLL} to output directory"
            )
        else()
            message(WARNING "Additional DLL not found: ${DLL}")
        endif()
    endforeach()
    
    # Copy Qt platform plugins
    message(STATUS "Adding Qt platform plugins to build directory")
    
    # Create plugin directories
    set(PLUGIN_DIRS platforms styles imageformats iconengines)
    foreach(PLUGIN_DIR ${PLUGIN_DIRS})
        add_custom_command(TARGET BTP_GUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:BTP_GUI>/${PLUGIN_DIR}
            COMMENT "Creating ${PLUGIN_DIR} directory"
        )
    endforeach()
    
    # 1. Platforms plugin
    set(QT_WINDOWS_PLUGIN "C:/Users/kshit/Qt/6.10.0/msvc2022_64/plugins/platforms/qwindows${QT_DEBUG_SUFFIX}.dll")
    if(EXISTS "${QT_WINDOWS_PLUGIN}")
        add_custom_command(TARGET BTP_GUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_WINDOWS_PLUGIN}"
                $<TARGET_FILE_DIR:BTP_GUI>/platforms/qwindows${QT_DEBUG_SUFFIX}.dll
            COMMENT "Copying qwindows${QT_DEBUG_SUFFIX}.dll platform plugin"
        )
    else()
        message(WARNING "Qt Windows platform plugin not found: ${QT_WINDOWS_PLUGIN}")
        
        # Fall back to non-debug version if in debug mode and debug version not found
        if(QT_DEBUG_SUFFIX)
            set(QT_FALLBACK_PLUGIN "C:/Users/kshit/Qt/6.10.0/msvc2022_64/plugins/platforms/qwindows.dll")
            if(EXISTS "${QT_FALLBACK_PLUGIN}")
                message(STATUS "Using fallback non-debug platform plugin")
                add_custom_command(TARGET BTP_GUI POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${QT_FALLBACK_PLUGIN}"
                        $<TARGET_FILE_DIR:BTP_GUI>/platforms/qwindows.dll
                    COMMENT "Copying qwindows.dll fallback platform plugin"
                )
            else()
                message(FATAL_ERROR "No Qt Windows platform plugin found!")
            endif()
        endif()
    endif()
    
    # Copy essential Qt DLLs
    set(QT_CORE_DLLS
        Qt6Core
        Qt6Gui 
        Qt6Widgets
        Qt6OpenGL
        Qt6OpenGLWidgets
        Qt6Svg
        Qt6Network
    )
    
    foreach(QT_DLL_BASE ${QT_CORE_DLLS})
        set(QT_DLL_PATH "C:/Users/kshit/Qt/6.10.0/msvc2022_64/bin/${QT_DLL_BASE}${QT_DEBUG_SUFFIX}.dll")
        if(EXISTS "${QT_DLL_PATH}")
            add_custom_command(TARGET BTP_GUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT_DLL_PATH}"
                    $<TARGET_FILE_DIR:BTP_GUI>/${QT_DLL_BASE}${QT_DEBUG_SUFFIX}.dll
                COMMENT "Copying ${QT_DLL_BASE}${QT_DEBUG_SUFFIX}.dll to output directory"
            )
        else()
            message(WARNING "Qt DLL not found: ${QT_DLL_PATH}")
        endif()
    endforeach()
    
    # Create qt.conf file to help Qt find the plugins
    file(WRITE ${CMAKE_BINARY_DIR}/qt.conf "[Paths]\nPrefix = .\nPlugins = .\nBinaries = .\nLibraries = .\n")
    add_custom_command(TARGET BTP_GUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/qt.conf
            $<TARGET_FILE_DIR:BTP_GUI>/qt.conf
        COMMENT "Creating qt.conf file"
    )
endif()
