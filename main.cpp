#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);


#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QProcessEnvironment>
#include <QDir>
#include <QDebug>
#include <QLibrary>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include "mainwindow.h"

// Add global variable to control VTK visualization availability
bool g_enableVTKVisualization = true;

// Helper function to verify if a DLL can be loaded
bool verifyDllLoads(const QString& dllPath) {
    QLibrary lib(dllPath);
    bool success = lib.load();
    if (!success) {
        qDebug() << "Failed to load:" << dllPath << "Error:" << lib.errorString();
    } else {
        qDebug() << "Successfully loaded:" << dllPath;
        lib.unload();
    }
    return success;
}

// Helper function to check OpenGL availability and capabilities
bool checkOpenGLCapabilities() {
    // Create a default format that should work on most systems
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(2, 1);  // Very basic OpenGL version
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    
    // Create a temporary QApplication instance to ensure screen is initialized
    // This is important as QOffscreenSurface needs screen information to work properly
    int tmpArgc = 1;
    char* tmpArgv[] = { const_cast<char*>("dummy"), nullptr };
    QApplication tmpApp(tmpArgc, tmpArgv);
    
    // Create a temporary offscreen surface to test OpenGL capabilities
    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    if (!surface.isValid()) {
        qDebug() << "Failed to create valid offscreen OpenGL surface";
        return false;
    }
    
    // Create an OpenGL context
    QOpenGLContext context;
    context.setFormat(format);
    if (!context.create()) {
        qDebug() << "Failed to create OpenGL context";
        return false;
    }
    
    // Make the context current on the offscreen surface
    if (!context.makeCurrent(&surface)) {
        qDebug() << "Failed to make OpenGL context current";
        return false;
    }
    
    // Get OpenGL functions for the current context
    QOpenGLFunctions* f = context.functions();
    if (!f) {
        qDebug() << "Failed to get OpenGL functions";
        return false;
    }
    
    // Get vendor and version info
    QString vendor = reinterpret_cast<const char*>(f->glGetString(GL_VENDOR));
    QString renderer = reinterpret_cast<const char*>(f->glGetString(GL_RENDERER));
    QString version = reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
    
    qDebug() << "OpenGL Vendor:" << vendor;
    qDebug() << "OpenGL Renderer:" << renderer;
    qDebug() << "OpenGL Version:" << version;
    
    // Check if we have at least OpenGL 2.1
    QStringList versionParts = version.split(' ').first().split('.');
    if (versionParts.size() >= 2) {
        int majorVersion = versionParts[0].toInt();
        int minorVersion = versionParts[1].toInt();
        
        if (majorVersion < 2 || (majorVersion == 2 && minorVersion < 1)) {
            qDebug() << "OpenGL version too low for VTK visualization";
            return false;
        }
    }
    
    // Release context
    context.doneCurrent();
    
    // All checks passed
    qDebug() << "OpenGL capabilities check passed";
    return true;
}

void checkOpenGLSupport() {
    QOpenGLContext context;
    if (!context.create()) {
        qDebug() << "Failed to create OpenGL context!";
        return;
    }
    
    qDebug() << "OpenGL support check:";
    qDebug() << "  OpenGL context valid:" << context.isValid();
    qDebug() << "  OpenGL version:" << context.format().majorVersion() << "." << context.format().minorVersion();
    qDebug() << "  Profile:" << (context.format().profile() == QSurfaceFormat::CoreProfile ? "Core" : 
                              (context.format().profile() == QSurfaceFormat::CompatibilityProfile ? "Compatibility" : "None"));
}

int main(int argc, char *argv[])
{
    try {
        // Enable high DPI support
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
        
        // Check for command-line arguments to disable VTK
        bool forceDisableVTK = false;
        for (int i = 1; i < argc; i++) {
            QString arg(argv[i]);
            if (arg == "--no-vtk" || arg == "--disable-vtk") {
                forceDisableVTK = true;
                qDebug() << "VTK visualization disabled by command-line argument";
                break;
            }
        }
        
        // Also check environment variable
        QByteArray envVar = qgetenv("BTP_GUI_DISABLE_VTK");
        if (!envVar.isEmpty() && envVar != "0") {
            forceDisableVTK = true;
            qDebug() << "VTK visualization disabled by environment variable";
        }
        
        // Force load libxml2.dll before anything else
        QLibrary xmlLib("C:\\OSGeo4W\\bin\\libxml2.dll");
        if (!xmlLib.load()) {
            qDebug() << "Failed to preload libxml2.dll:" << xmlLib.errorString();
            
            // Try alternative path
            xmlLib.setFileName("C:\\OSGeo4W\\apps\\gdal\\bin\\libxml2.dll");
            if (!xmlLib.load()) {
                qDebug() << "Failed to preload alternate libxml2.dll:" << xmlLib.errorString();
            } else {
                qDebug() << "Successfully preloaded libxml2.dll from alternate path";
            }
        } else {
            qDebug() << "Successfully preloaded libxml2.dll";
        }
        
        // Add OSGeo4W bin directory to PATH to ensure all GDAL dependencies are found
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString path = env.value("PATH");
        
        // Check multiple possible OSGeo4W paths
        QStringList osgeoPathsToTry = {
            "C:\\OSGeo4W\\bin",
            "C:\\OSGeo4W\\apps\\gdal-dev\\bin",
            "C:\\OSGeo4W\\apps\\gdal\\bin",
            "C:\\OSGeo4W\\apps\\bin"
        };
        
        qDebug() << "Checking for OSGeo4W paths...";
        
        for (const QString& osgeoPath : osgeoPathsToTry) {
            if (QDir(osgeoPath).exists()) {
                qDebug() << "Found valid OSGeo4W path:" << osgeoPath;
                
                // Only add if not already in path
                if (!path.contains(osgeoPath, Qt::CaseInsensitive)) {
                    path = osgeoPath + ";" + path;
                    qDebug() << "Added to PATH:" << osgeoPath;
                } else {
                    qDebug() << "Already in PATH:" << osgeoPath;
                }
            } else {
                qDebug() << "Path does not exist:" << osgeoPath;
            }
        }
        
        // Set the updated PATH
        qputenv("PATH", path.toLocal8Bit());
        
        // Verify critical DLLs can be loaded
        QStringList criticalDlls = {
            "libxml2.dll",
            "spatialite.dll",
            "gdal.dll",
            "freexl.dll",
            "sqlite3.dll"
        };
        
        qDebug() << "Verifying DLL loading:";
        for (const QString& dll : criticalDlls) {
            verifyDllLoads(dll);
        }
        
        // Set OpenGL format based on assumed VTK availability
        // We'll validate actual capabilities after QApplication is created
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(3, 2);
        format.setProfile(QSurfaceFormat::CompatibilityProfile);
        format.setDepthBufferSize(24);
        format.setSamples(4); // Enable multisampling for smoother rendering
        QSurfaceFormat::setDefaultFormat(format);
        
        // Enable OpenGL debug logging
        qputenv("QT_OPENGL_DEBUG", "1");
        qputenv("QSG_INFO", "1");
        qputenv("VTK_DEBUG_LEAKS", "1");
        
        // Set application info
        QApplication::setApplicationName("BTP Drainage Simulator");
        QApplication::setOrganizationName("IIT Kharagpur");
        QApplication::setApplicationVersion("1.0.0");
        
        // Create the QApplication before checking OpenGL capabilities
        QApplication app(argc, argv);
        
        // Now check OpenGL capabilities with a valid QApplication instance
        // We'll use a modified version that doesn't create its own QApplication
        g_enableVTKVisualization = !forceDisableVTK; // Default based on command line
        
        // Only check OpenGL if VTK isn't already disabled
        if (g_enableVTKVisualization) {
            // Create a context directly with the application running
            QOpenGLContext context;
            if (context.create()) {
                QOffscreenSurface surface;
                surface.setFormat(context.format());
                surface.create();
                
                if (context.makeCurrent(&surface)) {
                    QOpenGLFunctions* f = context.functions();
                    if (f) {
                        QString version = reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
                        qDebug() << "OpenGL Version:" << version;
                        
                        // Check minimal requirements
                        QStringList versionParts = version.split(' ').first().split('.');
                        if (versionParts.size() >= 2) {
                            int majorVersion = versionParts[0].toInt();
                            int minorVersion = versionParts[1].toInt();
                            
                            if (majorVersion < 2 || (majorVersion == 2 && minorVersion < 1)) {
                                qDebug() << "OpenGL version too low for VTK visualization";
                                g_enableVTKVisualization = false;
                            }
                        }
                    } else {
                        qDebug() << "Could not get OpenGL functions";
                        g_enableVTKVisualization = false;
                    }
                    context.doneCurrent();
                } else {
                    qDebug() << "Could not make OpenGL context current";
                    g_enableVTKVisualization = false;
                }
            } else {
                qDebug() << "Could not create OpenGL context";
                g_enableVTKVisualization = false;
            }
        }
        
        if (!g_enableVTKVisualization) {
            qDebug() << "WARNING: VTK 3D visualization will be disabled due to insufficient OpenGL support";
        } else {
            // Preload critical VTK DLLs if VTK is enabled
            qDebug() << "Preloading VTK DLLs...";
            QStringList vtkDlls = {
                "vtkCommonCore-9.3d.dll",
                "vtkRenderingCore-9.3d.dll",
                "vtkRenderingOpenGL2-9.3d.dll",
                "vtkGUISupportQt-9.3d.dll"
            };
            
            QString buildDir = "C:\\Users\\kshit\\Documents\\BTP GUI\\BTP_GUI\\build\\Desktop_Qt_6_10_0_MSVC2022_64bit-Debug";
            for (const QString& dll : vtkDlls) {
                QString dllPath = buildDir + "\\" + dll;
                verifyDllLoads(dllPath);
            }
        }
        
        // Create the main window, passing the VTK availability flag
        MainWindow window(g_enableVTKVisualization);
        window.show();
        
        return app.exec();
    } catch (const std::exception& e) {
        qDebug() << "Exception in main:" << e.what();
        return 1;
    } catch (...) {
        qDebug() << "Unknown exception in main";
        return 1;
    }
}
