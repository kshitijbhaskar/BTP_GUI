#include "VTKWaterVisualizer.h"
#include "VTKTerrainVisualizer.h"
#include "SimulationEngine.h"
#include <QDebug>

// Additional VTK headers
#include <vtkCellArray.h>
#include <vtkGlyph3D.h>
#include <vtkMath.h>
#include <vtkArrowSource.h>
#include <vtkMaskPoints.h>
#include <vtkSphereSource.h>
#include <vtkWarpScalar.h>
#include <vtkScalarBarActor.h>
#include <vtkAppendPolyData.h>
#include <vtkVectorNorm.h>
#include <vtkAssignAttribute.h>
#include <vtkTextProperty.h>
#include <vtkHedgeHog.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkColorTransferFunction.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmoothPolyDataFilter.h>

VTKWaterVisualizer::VTKWaterVisualizer(QWidget *parent)
    : QVTKOpenGLNativeWidget(parent),
      simulationEngine(nullptr),
      terrainVisualizer(nullptr),
      gridWidth(0),
      gridHeight(0),
      resolution(1.0),
      waterOpacity(0.8),
      surfaceSmoothing(true),
      depthColorScale(1.0),
      verticalExaggeration(1.0),
      showFlowVectors(false),
      waterSurfaceVisible(true),
      showHeightField(false),
      maxWaterDepth(0.0),
      lastMaxWaterDepth(0.0)
{
    try {
        qDebug() << "VTKWaterVisualizer constructor - initialization started";
        
        // Set surface format for safer OpenGL initialization
        QSurfaceFormat format = QVTKOpenGLNativeWidget::defaultFormat();
        format.setProfile(QSurfaceFormat::CompatibilityProfile);
        format.setVersion(2, 1);  // Lower minimum OpenGL version for better compatibility
        format.setSamples(0);  // Disable multisampling initially
        format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        setFormat(format);
        
        // Create a temporary QWidget to test if rendering is available
        QWidget testWidget;
        testWidget.setAttribute(Qt::WA_NativeWindow);
        if (!testWidget.winId()) {
            qDebug() << "WARNING: Could not create native window ID for testing";
        }
        
        qDebug() << "VTKWaterVisualizer - Creating renderer";
        // Create a renderer with a black background
        renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->SetBackground(0.1, 0.2, 0.4); // Dark blue background
        
        // Set up the render window
        if (renderWindow()) {
            qDebug() << "VTKWaterVisualizer - Setting up render window";
            renderWindow()->AddRenderer(renderer);
            
            // Create an interactor style for 3D navigation
            if (renderWindow()->GetInteractor()) {
                vtkSmartPointer<vtkInteractorStyleTrackballCamera> style = 
                    vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
                renderWindow()->GetInteractor()->SetInteractorStyle(style);
            }
            else {
                qDebug() << "Warning: Interactor is null in VTKWaterVisualizer constructor";
            }
        }
        else {
            qDebug() << "Warning: RenderWindow is null in VTKWaterVisualizer constructor";
        }
        
        qDebug() << "VTKWaterVisualizer - Creating components";
        // Initialize water visualization components
        waterPoints = vtkSmartPointer<vtkPoints>::New();
        waterDepthData = vtkSmartPointer<vtkFloatArray>::New();
        waterDepthData->SetName("WaterDepth");
        
        waterPolyData = vtkSmartPointer<vtkPolyData>::New();
        waterMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        waterActor = vtkSmartPointer<vtkActor>::New();
        
        // Create lookup table for water depth colors
        waterLUT = vtkSmartPointer<vtkLookupTable>::New();
        
        // Initialize flow vectors actor
        flowVectorsActor = vtkSmartPointer<vtkActor>::New();
        
        qDebug() << "VTKWaterVisualizer - Creating color legend";
        // Add a scalar bar (color legend)
        vtkSmartPointer<vtkScalarBarActor> scalarBar = 
            vtkSmartPointer<vtkScalarBarActor>::New();
        scalarBar->SetLookupTable(waterLUT);
        scalarBar->SetTitle("Water Depth (m)");
        scalarBar->SetNumberOfLabels(5);
        scalarBar->SetPosition(0.05, 0.1);
        scalarBar->SetWidth(0.1);
        scalarBar->SetHeight(0.8);
        renderer->AddActor2D(scalarBar);
        
        qDebug() << "VTKWaterVisualizer constructor - completed successfully";
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in VTKWaterVisualizer constructor:" << e.what();
        // Try to create minimal render structures to avoid crashes
        createMinimalRenderComponents();
    }
    catch (...) {
        qDebug() << "Unknown exception in VTKWaterVisualizer constructor";
        createMinimalRenderComponents();
    }
}

// Helper method to create minimal render components in case of initialization failure
void VTKWaterVisualizer::createMinimalRenderComponents()
{
    try {
        qDebug() << "Creating minimal render components to avoid crashes";
        renderer = vtkSmartPointer<vtkRenderer>::New();
        waterPoints = vtkSmartPointer<vtkPoints>::New();
        waterDepthData = vtkSmartPointer<vtkFloatArray>::New();
        waterPolyData = vtkSmartPointer<vtkPolyData>::New();
        waterMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        waterActor = vtkSmartPointer<vtkActor>::New();
        waterLUT = vtkSmartPointer<vtkLookupTable>::New();
        flowVectorsActor = vtkSmartPointer<vtkActor>::New();
    }
    catch (...) {
        qDebug() << "Failed to create even minimal VTK renderer components";
    }
}

VTKWaterVisualizer::~VTKWaterVisualizer()
{
    // VTK smart pointers handle cleanup automatically
}

void VTKWaterVisualizer::initialize(SimulationEngine* engine, VTKTerrainVisualizer* terrainViz)
{
    simulationEngine = engine;
    terrainVisualizer = terrainViz;
    
    if (!simulationEngine) {
        qDebug() << "Error: SimulationEngine is null";
        return;
    }
    
    // Get grid dimensions from the engine
    gridWidth = engine->getGridWidth();
    gridHeight = engine->getGridHeight();
    resolution = engine->getCellResolution();
    
    qDebug() << "Initializing VTK water visualizer with dimensions:" << gridWidth << "x" << gridHeight
             << "and resolution:" << resolution;
    
    // Create the initial water surface
    createWaterSurface();
    
    // Create flow vectors if enabled
    if (showFlowVectors) {
        createFlowVectors();
    }
    
    // If we have a terrain visualizer, set up shared camera
    if (terrainVisualizer) {
        syncCameraWithTerrain();
    } else {
        // Otherwise set up a default camera view
        resetCamera();
    }
    
    // Force a render
    renderWindow()->Render();
}

void VTKWaterVisualizer::createWaterSurface()
{
    if (!simulationEngine || gridWidth <= 0 || gridHeight <= 0) {
        qDebug() << "Cannot create water surface: invalid dimensions or no engine";
        return;
    }
    
    // Clear existing data
    waterPoints->Reset();
    waterDepthData->Reset();
    waterDepthData->SetNumberOfComponents(1);
    
    // Find max water depth for scaling
    maxWaterDepth = findMaxWaterDepth();
    lastMaxWaterDepth = maxWaterDepth;
    
    qDebug() << "Creating water surface with max depth:" << maxWaterDepth;
    
    // Get the no-data value for DEM cells
    double noDataValue = -999999.0;
    
    // Create points for water surface
    for (int i = 0; i < gridHeight; i++) {
        for (int j = 0; j < gridWidth; j++) {
            // Get water depth at this cell
            // This would be replaced by actual data access: simulationEngine->getWaterDepth(i, j)
            double depth = 0.0; // Initially zero depth
            
            // Skip no-data cells 
            // This would be replaced by actual data access: simulationEngine->getDEMValue(i, j)
            double elevation = 50.0 + 25.0 * sin(i*0.1) * cos(j*0.1); // Placeholder
            bool isValidCell = (elevation > noDataValue + 1);
            
            if (!isValidCell) {
                depth = 0.0;
            }
            
            // Add point with coordinates (x, y, z) where z is water surface
            double x = j * resolution;
            double y = i * resolution;
            double z = elevation + depth * verticalExaggeration;
            
            waterPoints->InsertNextPoint(x, y, z);
            waterDepthData->InsertNextValue(depth);
        }
    }
    
    // Attach points and data to poly data
    waterPolyData->SetPoints(waterPoints);
    waterPolyData->GetPointData()->SetScalars(waterDepthData);
    
    // Create triangles to form the surface
    vtkSmartPointer<vtkCellArray> triangles = 
        vtkSmartPointer<vtkCellArray>::New();
    
    for (int i = 0; i < gridHeight - 1; i++) {
        for (int j = 0; j < gridWidth - 1; j++) {
            // Get the four corner vertices of this grid cell
            vtkIdType v1 = i * gridWidth + j;
            vtkIdType v2 = i * gridWidth + (j + 1);
            vtkIdType v3 = (i + 1) * gridWidth + j;
            vtkIdType v4 = (i + 1) * gridWidth + (j + 1);
            
            // Create two triangles for each grid cell
            vtkSmartPointer<vtkTriangle> triangle1 = 
                vtkSmartPointer<vtkTriangle>::New();
            triangle1->GetPointIds()->SetId(0, v1);
            triangle1->GetPointIds()->SetId(1, v2);
            triangle1->GetPointIds()->SetId(2, v3);
            
            vtkSmartPointer<vtkTriangle> triangle2 = 
                vtkSmartPointer<vtkTriangle>::New();
            triangle2->GetPointIds()->SetId(0, v2);
            triangle2->GetPointIds()->SetId(1, v4);
            triangle2->GetPointIds()->SetId(2, v3);
            
            triangles->InsertNextCell(triangle1);
            triangles->InsertNextCell(triangle2);
        }
    }
    
    waterPolyData->SetPolys(triangles);
    
    // Apply smoothing if enabled
    if (surfaceSmoothing) {
        vtkSmartPointer<vtkSmoothPolyDataFilter> smoothFilter = 
            vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
        smoothFilter->SetInputData(waterPolyData);
        smoothFilter->SetNumberOfIterations(2);
        smoothFilter->SetRelaxationFactor(0.2);
        smoothFilter->FeatureEdgeSmoothingOff();
        smoothFilter->BoundarySmoothingOn();
        smoothFilter->Update();
        
        waterPolyData = smoothFilter->GetOutput();
    }
    
    // Generate normals for better lighting
    vtkSmartPointer<vtkPolyDataNormals> normalsGenerator = 
        vtkSmartPointer<vtkPolyDataNormals>::New();
    normalsGenerator->SetInputData(waterPolyData);
    normalsGenerator->ComputePointNormalsOn();
    normalsGenerator->ComputeCellNormalsOff();
    normalsGenerator->SplittingOff();
    normalsGenerator->Update();
    
    waterPolyData = normalsGenerator->GetOutput();
    
    // Set up coloring
    setupWaterColorMapping();
    
    // Create mapper and actor
    waterMapper->SetInputData(waterPolyData);
    waterMapper->SetLookupTable(waterLUT);
    waterMapper->SetScalarRange(0.0, maxWaterDepth > 0 ? maxWaterDepth : 0.1);
    
    // Set up water surface appearance
    waterActor->SetMapper(waterMapper);
    waterActor->GetProperty()->SetOpacity(waterOpacity);
    waterActor->GetProperty()->SetAmbient(0.4);
    waterActor->GetProperty()->SetDiffuse(0.8);
    waterActor->GetProperty()->SetSpecular(0.8);
    waterActor->GetProperty()->SetSpecularPower(30);
    waterActor->SetVisibility(waterSurfaceVisible);
    
    // Add to renderer
    renderer->AddActor(waterActor);
    
    // Emit signal with updated max depth
    emit waterDepthUpdated(maxWaterDepth);
}

void VTKWaterVisualizer::setupWaterColorMapping()
{
    // Create a blue-to-cyan color gradient for water depth
    waterLUT->SetNumberOfTableValues(256);
    waterLUT->SetHueRange(0.58, 0.54);  // Blue to cyan
    waterLUT->SetSaturationRange(1.0, 0.5);  // Fully saturated to less saturated
    waterLUT->SetValueRange(0.5, 1.0);  // Darker to lighter
    waterLUT->SetAlphaRange(0.7, 0.9);  // More transparent for shallow, more opaque for deep
    waterLUT->SetTableRange(0.0, maxWaterDepth > 0 ? maxWaterDepth : 0.1);
    waterLUT->Build();
}

void VTKWaterVisualizer::updateWaterDepth()
{
    if (!simulationEngine || !waterDepthData || gridWidth <= 0 || gridHeight <= 0) {
        return;
    }
    
    // Find new max water depth for color scaling
    maxWaterDepth = findMaxWaterDepth();
    
    // Only update the color scale if the max depth has changed significantly
    if (std::abs(maxWaterDepth - lastMaxWaterDepth) > 0.01 * lastMaxWaterDepth) {
        waterMapper->SetScalarRange(0.0, maxWaterDepth > 0 ? maxWaterDepth : 0.1);
        setupWaterColorMapping();
        lastMaxWaterDepth = maxWaterDepth;
        
        // Emit signal with updated max depth
        emit waterDepthUpdated(maxWaterDepth);
    }
    
    // Update the water surface geometry
    updateWaterSurface();
    
    // Update flow vectors if enabled
    if (showFlowVectors) {
        createFlowVectors();
    }
    
    // Render the scene
    renderWindow()->Render();
}

void VTKWaterVisualizer::updateWaterSurface()
{
    // Access the data arrays for update
    waterDepthData = vtkFloatArray::SafeDownCast(waterPolyData->GetPointData()->GetScalars());
    
    if (!waterDepthData) {
        qDebug() << "Error: Water depth data array is null";
        return;
    }
    
    // Get the no-data value for DEM cells
    double noDataValue = -999999.0;
    
    // Update point positions and depth values
    for (int i = 0; i < gridHeight; i++) {
        for (int j = 0; j < gridWidth; j++) {
            // Get water depth at this cell
            // This would be replaced by actual data access: simulationEngine->getWaterDepth(i, j)
            double depth = 0.05 * (1 + sin(i*0.2 + j*0.1)) * (1 + cos(i*0.05 - j*0.15)); // Placeholder
            
            // Skip no-data cells 
            // This would be replaced by actual data access: simulationEngine->getDEMValue(i, j)
            double elevation = 50.0 + 25.0 * sin(i*0.1) * cos(j*0.1); // Placeholder
            bool isValidCell = (elevation > noDataValue + 1);
            
            if (!isValidCell) {
                depth = 0.0;
            }
            
            // Calculate point index
            vtkIdType pointId = i * gridWidth + j;
            
            // Update water depth scalar value
            waterDepthData->SetValue(pointId, depth);
            
            // Update point position for the water surface
            double x = j * resolution;
            double y = i * resolution;
            double z = elevation + depth * verticalExaggeration;
            
            waterPoints->SetPoint(pointId, x, y, z);
        }
    }
    
    // Notify VTK that the data has been updated
    waterPoints->Modified();
    waterDepthData->Modified();
    waterPolyData->Modified();
    
    // Apply smoothing if enabled
    if (surfaceSmoothing) {
        vtkSmartPointer<vtkSmoothPolyDataFilter> smoothFilter = 
            vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
        smoothFilter->SetInputData(waterPolyData);
        smoothFilter->SetNumberOfIterations(2);
        smoothFilter->SetRelaxationFactor(0.2);
        smoothFilter->FeatureEdgeSmoothingOff();
        smoothFilter->BoundarySmoothingOn();
        smoothFilter->Update();
        
        // Update the poly data
        waterPolyData->DeepCopy(smoothFilter->GetOutput());
    }
}

void VTKWaterVisualizer::createFlowVectors()
{
    // Remove existing flow vectors
    renderer->RemoveActor(flowVectorsActor);
    
    if (!showFlowVectors) {
        return;
    }
    
    // Create flow vector field data (placeholder)
    vtkSmartPointer<vtkFloatArray> flowVectors = 
        vtkSmartPointer<vtkFloatArray>::New();
    flowVectors->SetName("FlowVectors");
    flowVectors->SetNumberOfComponents(3);
    
    // Subsample the grid for better visualization
    int subsampleFactor = 4;  // Only show arrows every N cells
    
    // Create subsample points
    vtkSmartPointer<vtkPoints> subPoints = 
        vtkSmartPointer<vtkPoints>::New();
    
    // Create polydata for subsampled points
    vtkSmartPointer<vtkPolyData> subPolyData = 
        vtkSmartPointer<vtkPolyData>::New();
    
    // Sample points and vectors at a lower resolution
    for (int i = 0; i < gridHeight; i += subsampleFactor) {
        for (int j = 0; j < gridWidth; j += subsampleFactor) {
            // Get water depth at this cell (placeholder)
            double depth = 0.05 * (1 + sin(i*0.2 + j*0.1)) * (1 + cos(i*0.05 - j*0.15)); 
            
            // Skip cells with minimal water
            if (depth < 0.01) continue;
            
            // Create a random-ish flow vector (placeholder)
            double vx = 0.2 * cos(i*0.2 + j*0.15);
            double vy = 0.2 * sin(i*0.1 + j*0.2);
            double vz = 0.0;  // No vertical component
            
            // Scale vector by water depth (deeper water flows faster)
            float scale = 0.2 + 0.8 * (depth / maxWaterDepth);
            vx *= scale;
            vy *= scale;
            
            // Add the point coordinates (adjust height slightly above water)
            double x = j * resolution;
            double y = i * resolution;
            double z = 50.0 + depth * verticalExaggeration + 0.05;  // Placeholder elevation + depth
            
            subPoints->InsertNextPoint(x, y, z);
            flowVectors->InsertNextTuple3(vx, vy, vz);
        }
    }
    
    // Set up the poly data with points and vector data
    subPolyData->SetPoints(subPoints);
    subPolyData->GetPointData()->SetVectors(flowVectors);
    
    // Create an arrow glyph for the vectors
    vtkSmartPointer<vtkArrowSource> arrowSource = 
        vtkSmartPointer<vtkArrowSource>::New();
    arrowSource->SetTipResolution(16);
    arrowSource->SetShaftResolution(16);
    arrowSource->SetTipRadius(0.15);
    arrowSource->SetShaftRadius(0.05);
    
    // Glyph the vectors
    vtkSmartPointer<vtkGlyph3D> glyph = 
        vtkSmartPointer<vtkGlyph3D>::New();
    glyph->SetSourceConnection(arrowSource->GetOutputPort());
    glyph->SetInputData(subPolyData);
    glyph->SetVectorModeToUseVector();
    glyph->SetScaleFactor(resolution * 2.0);  // Scale arrows based on cell size
    glyph->OrientOn();
    glyph->Update();
    
    // Create mapper and actor for the flow vectors
    vtkSmartPointer<vtkPolyDataMapper> flowMapper = 
        vtkSmartPointer<vtkPolyDataMapper>::New();
    flowMapper->SetInputConnection(glyph->GetOutputPort());
    
    flowVectorsActor = vtkSmartPointer<vtkActor>::New();
    flowVectorsActor->SetMapper(flowMapper);
    flowVectorsActor->GetProperty()->SetColor(1.0, 1.0, 0.3);  // Yellowish
    flowVectorsActor->GetProperty()->SetOpacity(0.8);
    
    // Add to renderer
    renderer->AddActor(flowVectorsActor);
}

double VTKWaterVisualizer::findMaxWaterDepth()
{
    double maxDepth = 0.0;
    
    // Placeholder implementation - in a real app, this would query the simulation engine
    for (int i = 0; i < gridHeight; i++) {
        for (int j = 0; j < gridWidth; j++) {
            // This would be replaced by actual data access: simulationEngine->getWaterDepth(i, j)
            double depth = 0.05 * (1 + sin(i*0.2 + j*0.1)) * (1 + cos(i*0.05 - j*0.15)); // Placeholder
            maxDepth = std::max(maxDepth, depth);
        }
    }
    
    // Ensure a minimum max depth to avoid division by zero
    if (maxDepth < 0.001) {
        maxDepth = 0.001;
    }
    
    return maxDepth;
}

void VTKWaterVisualizer::setWaterOpacity(double opacity)
{
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;
    
    waterOpacity = opacity;
    
    if (waterActor) {
        waterActor->GetProperty()->SetOpacity(waterOpacity);
        renderWindow()->Render();
    }
}

void VTKWaterVisualizer::setWaterSurfaceSmoothing(bool enable)
{
    if (surfaceSmoothing == enable) return;
    
    surfaceSmoothing = enable;
    
    // Recreate the water surface with smoothing
    updateWaterSurface();
    renderWindow()->Render();
}

void VTKWaterVisualizer::setDepthColorScale(double scale)
{
    if (scale <= 0.0) scale = 0.1;
    
    depthColorScale = scale;
    
    // Update the color mapping
    setupWaterColorMapping();
    renderWindow()->Render();
}

void VTKWaterVisualizer::setVerticalExaggeration(double factor)
{
    if (factor < 0.1) factor = 0.1;
    if (factor > 10.0) factor = 10.0;
    
    verticalExaggeration = factor;
    
    // Update the water surface geometry
    updateWaterSurface();
    renderWindow()->Render();
}

void VTKWaterVisualizer::setShowFlowVectors(bool show)
{
    if (showFlowVectors == show) return;
    
    showFlowVectors = show;
    
    if (showFlowVectors) {
        createFlowVectors();
    } else {
        renderer->RemoveActor(flowVectorsActor);
    }
    
    renderWindow()->Render();
}

void VTKWaterVisualizer::setWaterSurfaceVisible(bool visible)
{
    if (waterSurfaceVisible == visible) return;
    
    waterSurfaceVisible = visible;
    
    if (waterActor) {
        waterActor->SetVisibility(waterSurfaceVisible);
        renderWindow()->Render();
    }
}

void VTKWaterVisualizer::setShowHeightField(bool enable)
{
    if (showHeightField == enable) return;
    
    showHeightField = enable;
    
    // Switch between color by depth and height field visualization
    if (waterMapper) {
        if (showHeightField) {
            // Color by height instead of water depth
            waterMapper->ScalarVisibilityOff();
            waterActor->GetProperty()->SetColor(0.0, 0.4, 0.8); // Uniform blue
        } else {
            // Color by water depth
            waterMapper->ScalarVisibilityOn();
        }
        
        renderWindow()->Render();
    }
}

void VTKWaterVisualizer::syncCameraWithTerrain()
{
    if (!terrainVisualizer) return;
    
    // Get the terrain's camera
    vtkCamera* terrainCamera = terrainVisualizer->getRenderer()->GetActiveCamera();
    
    if (terrainCamera) {
        // Copy camera parameters to this view
        vtkCamera* waterCamera = renderer->GetActiveCamera();
        
        double pos[3], focal[3], viewUp[3];
        terrainCamera->GetPosition(pos);
        terrainCamera->GetFocalPoint(focal);
        terrainCamera->GetViewUp(viewUp);
        
        waterCamera->SetPosition(pos);
        waterCamera->SetFocalPoint(focal);
        waterCamera->SetViewUp(viewUp);
        waterCamera->SetClippingRange(terrainCamera->GetClippingRange());
        
        if (terrainCamera->GetParallelProjection()) {
            waterCamera->ParallelProjectionOn();
            waterCamera->SetParallelScale(terrainCamera->GetParallelScale());
        } else {
            waterCamera->ParallelProjectionOff();
        }
        
        renderWindow()->Render();
    }
}

void VTKWaterVisualizer::resetCamera()
{
    if (!renderer) return;
    
    // Reset camera to show the entire scene
    renderer->ResetCamera();
    
    // Adjust for a default 3D view
    vtkCamera* camera = renderer->GetActiveCamera();
    camera->Elevation(30);
    camera->Azimuth(30);
    camera->OrthogonalizeViewUp();
    
    renderWindow()->Render();
} 