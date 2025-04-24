cmake_minimum_required(VERSION 3.16)

project(BTP_GUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Basic OpenMP support (simplified for compatibility)
if(MSVC)
    # MSVC doesn't support 'collapse' clause, so we only enable basic OpenMP
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /openmp")
    message(STATUS "Added /openmp flag for MSVC (note: collapse clause not supported)")
else()
    # For GCC/Clang
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        message(STATUS "OpenMP found and enabled")
    else()
        message(WARNING "OpenMP not found. Multithreading optimizations will be disabled.")
    endif()
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

# Find GDAL
find_package(GDAL REQUIRED)

# Debug output to see what's found
message(STATUS "GDAL_INCLUDE_DIR: ${GDAL_INCLUDE_DIR}")
message(STATUS "GDAL_LIBRARY: ${GDAL_LIBRARY}")

# Strip quotes from GDAL paths if present (Windows specific issue with Qt Creator)
string(REPLACE "\"" "" GDAL_INCLUDE_DIR_CLEAN "${GDAL_INCLUDE_DIR}")
string(REPLACE "\"" "" GDAL_LIBRARY_CLEAN "${GDAL_LIBRARY}")

# Check if the library path already has a .lib extension and make sure it's preserved
if(NOT GDAL_LIBRARY_CLEAN MATCHES "[.]lib$")
    # Add .lib extension if missing, important for Windows
    set(GDAL_LIBRARY_CLEAN "${GDAL_LIBRARY_CLEAN}.lib")
endif()

# Debug after cleaning
message(STATUS "GDAL_INCLUDE_DIR_CLEAN: ${GDAL_INCLUDE_DIR_CLEAN}")
message(STATUS "GDAL_LIBRARY_CLEAN: ${GDAL_LIBRARY_CLEAN}")

set(TS_FILES BTP_GUI_en_IN.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(BTP_GUI
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        SimulationEngine.cpp
        SimulationEngine.h
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET BTP_GUI APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(BTP_GUI SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(BTP_GUI
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# Add GDAL include directories
target_include_directories(BTP_GUI PRIVATE 
    ${GDAL_INCLUDE_DIR_CLEAN}
)

target_link_libraries(BTP_GUI PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    ${GDAL_LIBRARY_CLEAN}
)

# Link with OpenMP if found (for non-MSVC compilers only)
if(NOT MSVC AND OpenMP_CXX_FOUND)
    target_link_libraries(BTP_GUI PRIVATE OpenMP::OpenMP_CXX)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.BTP_GUI)
endif()
set_target_properties(BTP_GUI PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS BTP_GUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(BTP_GUI)
endif()

# Add code to copy GDAL DLLs to the output directory
if(WIN32)
    # Find the GDAL DLL
    string(REPLACE "lib/gdal.lib" "bin/gdal.dll" GDAL_DLL "${GDAL_LIBRARY_CLEAN}")
    
    # Check if the DLL exists
    if(EXISTS "${GDAL_DLL}")
        message(STATUS "Found GDAL DLL at ${GDAL_DLL}")
        
        # Add post-build command to copy the DLL
        add_custom_command(TARGET BTP_GUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${GDAL_DLL}"
                $<TARGET_FILE_DIR:BTP_GUI>
            COMMENT "Copying GDAL DLL to output directory"
        )
    else()
        # Try alternate path
        string(REPLACE "apps/gdal-dev/lib/gdal.lib" "bin/gdal.dll" GDAL_DLL_ALT "${GDAL_LIBRARY_CLEAN}")
        if(EXISTS "${GDAL_DLL_ALT}")
            message(STATUS "Found GDAL DLL at alternate location ${GDAL_DLL_ALT}")
            
            # Add post-build command to copy the DLL
            add_custom_command(TARGET BTP_GUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${GDAL_DLL_ALT}"
                    $<TARGET_FILE_DIR:BTP_GUI>
                COMMENT "Copying GDAL DLL to output directory"
            )
        else()
            message(WARNING "GDAL DLL not found at ${GDAL_DLL} or ${GDAL_DLL_ALT}. You may need to manually copy it to the executable directory.")
        endif()
    endif()
endif()
