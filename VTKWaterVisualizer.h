#ifndef VTKWATERVISUALIZER_H
#define VTKWATERVISUALIZER_H

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
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkWarpScalar.h>
#include <vtkPoints.h>
#include <vtkCamera.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkProperty.h>

// Forward declarations
class SimulationEngine;
class VTKTerrainVisualizer;
class vtkScalarBarActor;

class VTKWaterVisualizer : public QVTKOpenGLNativeWidget
{
    Q_OBJECT

public:
    explicit VTKWaterVisualizer(QWidget *parent = nullptr);
    ~VTKWaterVisualizer();

    // Initialize with a reference to the simulation engine
    void initialize(SimulationEngine* engine, VTKTerrainVisualizer* terrainViz = nullptr);
    
    // Update water depth visualization
    void updateWaterDepth();
    
    // Set visualization parameters
    void setWaterOpacity(double opacity);
    void setWaterSurfaceSmoothing(bool enable);
    void setDepthColorScale(double scale);
    void setVerticalExaggeration(double factor);
    
    // Show/hide flow vectors
    void setShowFlowVectors(bool show);
    
    // Show/hide water surface
    void setWaterSurfaceVisible(bool visible);
    
    // Show the water as a scalar height field
    void setShowHeightField(bool enable);
    
    // Access renderer for synchronization with terrain
    vtkSmartPointer<vtkRenderer> getRenderer() const { return renderer; }
    
    // Get current water max depth
    double getMaxWaterDepth() const { return maxWaterDepth; }
    
    // Camera controls (synchronized with terrain visualizer)
    void syncCameraWithTerrain();
    void resetCamera();
    
signals:
    void waterDepthUpdated(double maxDepth);

private:
    // Reference to simulation engine
    SimulationEngine* simulationEngine;
    
    // Optional reference to terrain visualizer
    VTKTerrainVisualizer* terrainVisualizer;
    
    // Core VTK components
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkPolyData> waterPolyData;
    vtkSmartPointer<vtkActor> waterActor;
    vtkSmartPointer<vtkLookupTable> waterLUT;
    vtkSmartPointer<vtkPolyDataMapper> waterMapper;
    
    // Flow vector visualization
    vtkSmartPointer<vtkActor> flowVectorsActor;
    
    // Grid dimensions
    int gridWidth;
    int gridHeight;
    double resolution;
    
    // Water data
    vtkSmartPointer<vtkPoints> waterPoints;
    vtkSmartPointer<vtkFloatArray> waterDepthData;
    
    // Visualization parameters
    double waterOpacity;
    bool surfaceSmoothing;
    double depthColorScale;
    double verticalExaggeration;
    bool showFlowVectors;
    bool waterSurfaceVisible;
    bool showHeightField;
    
    // Stats
    double maxWaterDepth;
    double lastMaxWaterDepth;
    
    // Create initial water surface
    void createWaterSurface();
    
    // Update water surface with current simulation data
    void updateWaterSurface();
    
    // Create flow vector glyphs
    void createFlowVectors();
    
    // Set up color mapping for water depth
    void setupWaterColorMapping();
    
    // Helper to find max water depth in simulation
    double findMaxWaterDepth();
    
    // Helper method to create minimal render components in case of initialization failure
    void createMinimalRenderComponents();
};

#endif // VTKWATERVISUALIZER_H 