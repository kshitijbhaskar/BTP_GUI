cmake_minimum_required(VERSION 3.16)

project(BTP_GUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add MSVC specific flags
if(MSVC)
    add_compile_options(/Zc:__cplusplus /permissive- /EHsc)
endif()

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
qt_standard_project_setup()

# Set GDAL paths explicitly for Windows
set(GDAL_DIR "C:/OSGeo4W/apps/gdal-dev/lib/cmake/gdal")
set(GDAL_INCLUDE_DIR "C:/OSGeo4W/apps/gdal-dev/include")
set(GDAL_LIBRARY "C:/OSGeo4W/apps/gdal-dev/lib/gdal_i.lib")

# Find GDAL with explicit paths
find_package(GDAL REQUIRED)

# Add source files
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    SimulationEngine.cpp
    SimulationEngine.h
)

# Create executable
qt_add_executable(BTP_GUI
    ${PROJECT_SOURCES}
)

# Set include directories
target_include_directories(BTP_GUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GDAL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(BTP_GUI PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    ${GDAL_LIBRARY}
)

# Enable automoc, autorcc and autouic
set_target_properties(BTP_GUI PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

# Set warning level
if(MSVC)
    target_compile_options(BTP_GUI PRIVATE /W4)
else()
    target_compile_options(BTP_GUI PRIVATE -Wall -Wextra)
endif()

# Install rules
install(TARGETS BTP_GUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
