/**
 * @file main.cpp
 * @brief Entry point for the BTP GUI Hydrological Simulation Application
 * 
 * This application provides:
 * - DEM-based hydrological simulation
 * - Interactive visualization of water flow
 * - Manual and automatic outlet selection
 * - Real-time simulation control
 * 
 * Built with:
 * - Qt 6.10.0 for the GUI framework
 * - GDAL for GeoTIFF processing
 * - C++17 features
 * 
 * Command line arguments:
 * None currently supported, using default Qt initialization
 * 
 * Dependencies:
 * - GDAL libraries must be installed and accessible
 * - Qt6 runtime environment
 * - C++ runtime libraries
 * 
 * High DPI Support:
 * - Enables Qt high DPI scaling for modern displays
 * - Automatically adjusts UI elements for screen resolution
 */

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // Initialize Qt application with high DPI support
    QApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    
    // Create and show the main window
    MainWindow w;
    w.show();
    
    // Enter Qt's event loop
    return a.exec();
}
