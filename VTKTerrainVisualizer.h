#ifndef VTKTERRAINVISUALIZER_H
#define VTKTERRAINVISUALIZER_H

#include <QVTKOpenGLNativeWidget.h>
#include <QWidget>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkLookupTable.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageData.h>
#include <vtkTexture.h>
#include <vtkPlaneSource.h>
#include <vtkDelaunay2D.h>
#include <vtkCamera.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkInteractorStyleTerrain.h>
#include <vtkPoints.h>
#include <vtkProperty.h>

// Forward declarations
class SimulationEngine;
class vtkCubeAxesActor;
class vtkScalarBarActor;

class VTKTerrainVisualizer : public QVTKOpenGLNativeWidget
{
    Q_OBJECT

public:
    explicit VTKTerrainVisualizer(QWidget *parent = nullptr);
    ~VTKTerrainVisualizer();

    // Initialize terrain with DEM data from the simulation engine
    void initializeTerrain(SimulationEngine* engine);
    
    // Update terrain with new data
    void updateTerrain();
    
    // Set the visibility of the grid
    void setGridVisible(bool visible);
    
    // Set grid interval (spacing between gridlines)
    void setGridInterval(int interval);
    
    // Toggle rulers/coordinate display
    void setRulersVisible(bool visible);
    
    // Add outlet points to the visualization
    void updateOutlets(const QVector<QPoint>& manualOutletCells, const QVector<QPoint>& autoOutletCells);
    
    // Handle mouse interactions for outlet selection
    QPoint screenToTerrainCoordinates(const QPoint& screenPos);
    
    // Get the underlying VTK renderer
    vtkSmartPointer<vtkRenderer> getRenderer() const { return renderer; }
    
    // Camera controls
    void resetCamera();
    void zoomIn();
    void zoomOut();
    void rotateView(double azimuth, double elevation);
    
    // Toggle between 2D and 3D view
    void set3DViewEnabled(bool enabled);
    bool is3DViewEnabled() const { return view3DEnabled; }
    
signals:
    void terrainClicked(QPoint position);

protected:
    // Mouse event handling for terrain interaction
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // Reference to simulation engine
    SimulationEngine* simulationEngine;
    
    // Core VTK components
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkPolyData> terrainPolyData;
    vtkSmartPointer<vtkPoints> terrainPoints;
    vtkSmartPointer<vtkFloatArray> elevationData;
    vtkSmartPointer<vtkActor> terrainActor;
    vtkSmartPointer<vtkLookupTable> elevationLUT;
    vtkSmartPointer<vtkPolyDataMapper> terrainMapper;
    
    // Actor for grid lines
    vtkSmartPointer<vtkActor> gridActor;
    
    // Actors for outlet points
    vtkSmartPointer<vtkActor> manualOutletsActor;
    vtkSmartPointer<vtkActor> autoOutletsActor;
    
    // Actors for axis/ruler display
    vtkSmartPointer<vtkActor> rulersActor;
    
    // Data ranges
    double elevationMin;
    double elevationMax;
    int gridWidth;
    int gridHeight;
    double resolution;
    
    // View settings
    bool gridVisible;
    bool rulersVisible;
    int gridInterval;
    bool view3DEnabled;
    
    // Mouse interaction variables
    bool dragging;
    QPoint lastMousePos;
    
    // Initialize VTK pipeline
    void setupRenderer();
    
    // Create terrain mesh from DEM data
    void createTerrainMesh();
    
    // Create and update grid lines
    void createGridLines();
    
    // Create rulers/coordinates
    void createRulers();
    
    // Create color mapping for terrain elevation
    void setupElevationMapping();
    
    // Update outlet point actors
    void createOutletPoints(vtkSmartPointer<vtkActor>& actor, const QVector<QPoint>& points, const QColor& color);
    
    // Helper method to create minimal render components in case of initialization failure
    void createMinimalRenderComponents();
};

#endif // VTKTERRAINVISUALIZER_H 