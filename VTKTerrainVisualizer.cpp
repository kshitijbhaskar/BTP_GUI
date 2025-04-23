#include "VTKTerrainVisualizer.h"
#include "SimulationEngine.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

// Additional VTK headers
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkAxesActor.h>
#include <vtkTransform.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkSphereSource.h>
#include <vtkGlyph3D.h>
#include <vtkPolyDataMapper.h>
#include <vtkLineSource.h>
#include <vtkAppendPolyData.h>
#include <vtkCubeAxesActor.h>
#include <vtkScalarBarActor.h>
#include <vtkCylinderSource.h>
#include <vtkCaptionActor2D.h>
#include <vtkPicker.h>

VTKTerrainVisualizer::VTKTerrainVisualizer(QWidget *parent)
    : QVTKOpenGLNativeWidget(parent),
      simulationEngine(nullptr),
      elevationMin(0.0),
      elevationMax(100.0),
      gridWidth(0),
      gridHeight(0),
      resolution(1.0),
      gridVisible(true),
      rulersVisible(false),
      gridInterval(10),
      view3DEnabled(true),
      dragging(false)
{
    try {
        qDebug() << "VTKTerrainVisualizer constructor - initialization started";

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
        
        // Safely initialize VTK components
        qDebug() << "VTKTerrainVisualizer - Creating VTK components";
        setupRenderer();
        qDebug() << "VTKTerrainVisualizer constructor - completed successfully";
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in VTKTerrainVisualizer constructor:" << e.what();
        // Try to create minimal render structures to avoid crashes
        createMinimalRenderComponents();
    }
    catch (...) {
        qDebug() << "Unknown exception in VTKTerrainVisualizer constructor";
        createMinimalRenderComponents();
    }
}

VTKTerrainVisualizer::~VTKTerrainVisualizer()
{
    // VTK smart pointers handle cleanup automatically
}

void VTKTerrainVisualizer::setupRenderer()
{
    try {
        // Create the renderer
        renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->SetBackground(0.9, 0.9, 0.9); // Light gray background
        
        // Set up the render window
        if (renderWindow()) {
            renderWindow()->AddRenderer(renderer);
            
            // Create an interactor style for terrain navigation
            if (renderWindow()->GetInteractor()) {
                vtkSmartPointer<vtkInteractorStyleTerrain> style = 
                    vtkSmartPointer<vtkInteractorStyleTerrain>::New();
                renderWindow()->GetInteractor()->SetInteractorStyle(style);
            }
            else {
                qDebug() << "Warning: Interactor is null in setupRenderer";
            }
        }
        else {
            qDebug() << "Warning: RenderWindow is null in setupRenderer";
        }
        
        // Create the terrain components
        terrainPoints = vtkSmartPointer<vtkPoints>::New();
        elevationData = vtkSmartPointer<vtkFloatArray>::New();
        elevationData->SetName("Elevation");
        
        terrainPolyData = vtkSmartPointer<vtkPolyData>::New();
        terrainMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        terrainActor = vtkSmartPointer<vtkActor>::New();
        
        // Create lookup table for elevation colors
        elevationLUT = vtkSmartPointer<vtkLookupTable>::New();
        
        // Create actors for grid, outlets, etc.
        gridActor = vtkSmartPointer<vtkActor>::New();
        manualOutletsActor = vtkSmartPointer<vtkActor>::New();
        autoOutletsActor = vtkSmartPointer<vtkActor>::New();
        rulersActor = vtkSmartPointer<vtkActor>::New();
        
        // Add a scalar bar (color legend)
        vtkSmartPointer<vtkScalarBarActor> scalarBar = 
            vtkSmartPointer<vtkScalarBarActor>::New();
        scalarBar->SetLookupTable(elevationLUT);
        scalarBar->SetTitle("Elevation (m)");
        scalarBar->SetNumberOfLabels(5);
        scalarBar->SetPosition(0.9, 0.1);
        scalarBar->SetWidth(0.1);
        scalarBar->SetHeight(0.8);
        renderer->AddActor2D(scalarBar);
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in setupRenderer:" << e.what();
    }
    catch (...) {
        qDebug() << "Unknown exception in setupRenderer";
    }
}

void VTKTerrainVisualizer::initializeTerrain(SimulationEngine* engine)
{
    simulationEngine = engine;
    
    if (!simulationEngine) {
        qDebug() << "Error: SimulationEngine is null";
        return;
    }
    
    // Get grid dimensions from the engine
    gridWidth = engine->getGridWidth();
    gridHeight = engine->getGridHeight();
    resolution = engine->getCellResolution();
    
    qDebug() << "Initializing VTK terrain with dimensions:" << gridWidth << "x" << gridHeight
             << "and resolution:" << resolution;
    
    // Create the terrain mesh
    createTerrainMesh();
    
    // Create grid lines if enabled
    if (gridVisible) {
        createGridLines();
    }
    
    // Create rulers if enabled
    if (rulersVisible) {
        createRulers();
    }
    
    // Update outlet points
    updateOutlets(engine->getManualOutletCells(), engine->getAutomaticOutletCells());
    
    // Reset the camera to show the entire terrain
    resetCamera();
    
    // Force a render
    renderWindow()->Render();
}

void VTKTerrainVisualizer::createTerrainMesh()
{
    if (!simulationEngine || gridWidth <= 0 || gridHeight <= 0) {
        qDebug() << "Cannot create terrain mesh: invalid dimensions or no engine";
        return;
    }
    
    // Clear existing data
    terrainPoints->Reset();
    elevationData->Reset();
    elevationData->SetNumberOfComponents(1);
    
    // Access the DEM data from the engine (we'll need to add a getter method)
    // Simulating DEM access for now - this would be replaced by actual data access
    double noDataValue = -999999.0;
    
    // Find min/max elevation for scaling
    elevationMin = std::numeric_limits<double>::max();
    elevationMax = std::numeric_limits<double>::lowest();
    
    // First pass to find min/max elevation
    for (int i = 0; i < gridHeight; i++) {
        for (int j = 0; j < gridWidth; j++) {
            // This would be replaced by simulationEngine->getDEMValue(i, j)
            double elevation = 50.0 + 25.0 * sin(i*0.1) * cos(j*0.1); // Placeholder
            
            // Check if this is a valid elevation (not a no-data value)
            if (elevation > noDataValue + 1) {
                elevationMin = std::min(elevationMin, elevation);
                elevationMax = std::max(elevationMax, elevation);
            }
        }
    }
    
    // Ensure we have a reasonable elevation range
    if (elevationMax <= elevationMin) {
        elevationMin = 0.0;
        elevationMax = 100.0;
    }
    
    qDebug() << "Elevation range:" << elevationMin << "to" << elevationMax;
    
    // Create a vertical scale factor for 3D visualization
    double verticalScale = view3DEnabled ? resolution * 0.2 : 0.0;
    double zBase = view3DEnabled ? 0.0 : 0.0;
    
    // Second pass to create points and assign elevations
    for (int i = 0; i < gridHeight; i++) {
        for (int j = 0; j < gridWidth; j++) {
            // This would be replaced by simulationEngine->getDEMValue(i, j)
            double elevation = 50.0 + 25.0 * sin(i*0.1) * cos(j*0.1); // Placeholder
            
            // Skip no-data cells or use a base elevation
            if (elevation <= noDataValue + 1) {
                elevation = elevationMin; // Use min elevation for no-data
            }
            
            // Create 3D point with scaled elevation
            double x = j * resolution;
            double y = i * resolution;
            double z = zBase + (elevation - elevationMin) * verticalScale;
            
            terrainPoints->InsertNextPoint(x, y, z);
            elevationData->InsertNextValue(elevation);
        }
    }
    
    // Create a triangular mesh using Delaunay triangulation
    vtkSmartPointer<vtkDelaunay2D> delaunay = 
        vtkSmartPointer<vtkDelaunay2D>::New();
    
    vtkSmartPointer<vtkPolyData> pointsPolydata = 
        vtkSmartPointer<vtkPolyData>::New();
    pointsPolydata->SetPoints(terrainPoints);
    
    delaunay->SetInputData(pointsPolydata);
    delaunay->Update();
    
    // Get the resulting triangulation
    terrainPolyData = delaunay->GetOutput();
    terrainPolyData->GetPointData()->SetScalars(elevationData);
    
    // Set up the color mapping
    setupElevationMapping();
    
    // Create the mapper and actor
    terrainMapper->SetInputData(terrainPolyData);
    terrainMapper->SetLookupTable(elevationLUT);
    terrainMapper->SetScalarRange(elevationMin, elevationMax);
    
    terrainActor->SetMapper(terrainMapper);
    terrainActor->GetProperty()->SetAmbient(0.3);
    terrainActor->GetProperty()->SetDiffuse(0.7);
    terrainActor->GetProperty()->SetSpecular(0.1);
    terrainActor->GetProperty()->SetSpecularPower(10);
    
    // Add to renderer
    renderer->AddActor(terrainActor);
}

void VTKTerrainVisualizer::setupElevationMapping()
{
    // Use a color map that transitions from green (low) to brown to white (high)
    elevationLUT->SetNumberOfTableValues(256);
    elevationLUT->SetHueRange(0.375, 0.0);  // From blue-green to red
    elevationLUT->SetSaturationRange(1.0, 0.0);  // Full saturation to white
    elevationLUT->SetValueRange(0.5, 1.0);  // Darker to lighter
    elevationLUT->SetTableRange(elevationMin, elevationMax);
    elevationLUT->Build();
}

void VTKTerrainVisualizer::createGridLines()
{
    if (gridWidth <= 0 || gridHeight <= 0) {
        return;
    }
    
    // Remove existing grid
    renderer->RemoveActor(gridActor);
    
    // Create a polydata to store all grid lines
    vtkSmartPointer<vtkAppendPolyData> appendFilter = 
        vtkSmartPointer<vtkAppendPolyData>::New();
    
    // Create grid lines at specified intervals
    int interval = gridInterval;
    if (interval < 1) interval = 1;
    
    // Small offset above the terrain to avoid z-fighting
    double zOffset = view3DEnabled ? 0.01 : 0.001;
    
    // Create horizontal grid lines
    for (int i = 0; i <= gridHeight; i += interval) {
        // Skip if outside grid
        if (i > gridHeight) continue;
        
        vtkSmartPointer<vtkLineSource> line = 
            vtkSmartPointer<vtkLineSource>::New();
            
        double yPos = i * resolution;
        
        // For 3D view, follow the terrain roughly
        if (view3DEnabled) {
            line->SetPoint1(0.0, yPos, elevationMin + zOffset);
            line->SetPoint2(gridWidth * resolution, yPos, elevationMin + zOffset);
        } else {
            line->SetPoint1(0.0, yPos, zOffset);
            line->SetPoint2(gridWidth * resolution, yPos, zOffset);
        }
        
        line->Update();
        appendFilter->AddInputData(line->GetOutput());
    }
    
    // Create vertical grid lines
    for (int j = 0; j <= gridWidth; j += interval) {
        // Skip if outside grid
        if (j > gridWidth) continue;
        
        vtkSmartPointer<vtkLineSource> line = 
            vtkSmartPointer<vtkLineSource>::New();
            
        double xPos = j * resolution;
        
        // For 3D view, follow the terrain roughly
        if (view3DEnabled) {
            line->SetPoint1(xPos, 0.0, elevationMin + zOffset);
            line->SetPoint2(xPos, gridHeight * resolution, elevationMin + zOffset);
        } else {
            line->SetPoint1(xPos, 0.0, zOffset);
            line->SetPoint2(xPos, gridHeight * resolution, zOffset);
        }
        
        line->Update();
        appendFilter->AddInputData(line->GetOutput());
    }
    
    // Create mapper and actor for the grid
    appendFilter->Update();
    
    vtkSmartPointer<vtkPolyDataMapper> gridMapper = 
        vtkSmartPointer<vtkPolyDataMapper>::New();
    gridMapper->SetInputConnection(appendFilter->GetOutputPort());
    
    gridActor->SetMapper(gridMapper);
    gridActor->GetProperty()->SetColor(0.0, 0.0, 0.0);  // Black
    gridActor->GetProperty()->SetOpacity(0.3);  // Semi-transparent
    gridActor->GetProperty()->SetLineWidth(1.0);
    
    // Add to renderer
    renderer->AddActor(gridActor);
}

void VTKTerrainVisualizer::createRulers()
{
    if (gridWidth <= 0 || gridHeight <= 0) {
        return;
    }
    
    // Remove existing rulers
    renderer->RemoveActor(rulersActor);
    
    // Use a cube axes actor to display coordinates
    vtkSmartPointer<vtkCubeAxesActor> cubeAxes = 
        vtkSmartPointer<vtkCubeAxesActor>::New();
    
    double bounds[6];
    if (terrainActor) {
        terrainActor->GetBounds(bounds);
    } else {
        // Default bounds based on grid dimensions
        bounds[0] = 0.0;  // xmin
        bounds[1] = gridWidth * resolution;  // xmax
        bounds[2] = 0.0;  // ymin
        bounds[3] = gridHeight * resolution;  // ymax
        bounds[4] = 0.0;  // zmin
        bounds[5] = view3DEnabled ? (elevationMax - elevationMin) * 0.2 * resolution : 0.0;  // zmax
    }
    
    // Add some padding
    bounds[0] -= resolution * 2;
    bounds[1] += resolution * 2;
    bounds[2] -= resolution * 2;
    bounds[3] += resolution * 2;
    bounds[4] -= resolution * 2;
    bounds[5] += resolution * 2;
    
    cubeAxes->SetBounds(bounds);
    cubeAxes->SetCamera(renderer->GetActiveCamera());
    
    // Set ticks and labels
    cubeAxes->SetXAxisMinorTickVisibility(0);
    cubeAxes->SetYAxisMinorTickVisibility(0);
    cubeAxes->SetZAxisMinorTickVisibility(0);
    
    // Format labels - use grid coordinates and distance units (meters)
    cubeAxes->SetXLabelFormat("%g");
    cubeAxes->SetYLabelFormat("%g");
    cubeAxes->SetZLabelFormat("%g");
    
    cubeAxes->SetXTitle("Distance (m)");
    cubeAxes->SetYTitle("Distance (m)");
    cubeAxes->SetZTitle("Elevation (m)");
    
    // VTK 9.3 doesn't have SetNumberOfLabels, set using alternative methods
    // Instead of SetNumberOfLabels(5)
    cubeAxes->SetXAxisLabelVisibility(1);
    cubeAxes->SetYAxisLabelVisibility(1);
    cubeAxes->SetZAxisLabelVisibility(1);
    
    // Customize appearance
    cubeAxes->GetTitleTextProperty(0)->SetColor(0.0, 0.0, 0.0);
    cubeAxes->GetTitleTextProperty(1)->SetColor(0.0, 0.0, 0.0);
    cubeAxes->GetTitleTextProperty(2)->SetColor(0.0, 0.0, 0.0);
    
    cubeAxes->GetLabelTextProperty(0)->SetColor(0.0, 0.0, 0.0);
    cubeAxes->GetLabelTextProperty(1)->SetColor(0.0, 0.0, 0.0);
    cubeAxes->GetLabelTextProperty(2)->SetColor(0.0, 0.0, 0.0);
    
    // Add to renderer
    renderer->AddActor(cubeAxes);
    
    // Store the actor for future removal
    rulersActor = vtkSmartPointer<vtkActor>::New();
    rulersActor->SetUserMatrix(cubeAxes->GetUserMatrix());
}

void VTKTerrainVisualizer::updateOutlets(const QVector<QPoint>& manualOutletCells, const QVector<QPoint>& autoOutletCells)
{
    // Update manual outlets
    createOutletPoints(manualOutletsActor, manualOutletCells, QColor(255, 0, 0)); // Red
    
    // Update automatic outlets
    createOutletPoints(autoOutletsActor, autoOutletCells, QColor(0, 100, 255)); // Blue
    
    // Force a render
    renderWindow()->Render();
}

void VTKTerrainVisualizer::createOutletPoints(vtkSmartPointer<vtkActor>& actor, const QVector<QPoint>& points, const QColor& color)
{
    // Remove existing actor
    renderer->RemoveActor(actor);
    
    if (points.isEmpty()) {
        return; // No points to add
    }
    
    // Create points dataset
    vtkSmartPointer<vtkPoints> outletPoints = 
        vtkSmartPointer<vtkPoints>::New();
    
    // Create a polydata for the points
    vtkSmartPointer<vtkPolyData> outletPolyData = 
        vtkSmartPointer<vtkPolyData>::New();
    
    // Vertical scale factor for 3D visualization
    double verticalScale = view3DEnabled ? resolution * 0.2 : 0.0;
    double zBase = view3DEnabled ? 0.0 : 0.0;
    double zOffset = view3DEnabled ? 0.1 : 0.01; // Offset above terrain
    
    // Add each outlet point
    for (const QPoint& point : points) {
        int i = point.x();
        int j = point.y();
        
        // Skip if outside grid
        if (i < 0 || i >= gridHeight || j < 0 || j >= gridWidth) {
            continue;
        }
        
        // Get the elevation at this point
        double elevation = 50.0; // Placeholder - use actual DEM data
        
        // Create a sphere for the outlet
        double x = j * resolution;
        double y = i * resolution;
        double z = zBase + (elevation - elevationMin) * verticalScale + zOffset;
        
        outletPoints->InsertNextPoint(x, y, z);
    }
    
    outletPolyData->SetPoints(outletPoints);
    
    // Create vertex cells for each point
    vtkSmartPointer<vtkCellArray> vertices = 
        vtkSmartPointer<vtkCellArray>::New();
    
    for (vtkIdType i = 0; i < outletPoints->GetNumberOfPoints(); i++) {
        vertices->InsertNextCell(1);
        vertices->InsertCellPoint(i);
    }
    
    outletPolyData->SetVerts(vertices);
    
    // Use spherical glyphs to represent outlet points
    vtkSmartPointer<vtkSphereSource> sphereSource = 
        vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetRadius(resolution * 0.4);
    sphereSource->SetPhiResolution(12);
    sphereSource->SetThetaResolution(12);
    
    vtkSmartPointer<vtkGlyph3D> glyph = 
        vtkSmartPointer<vtkGlyph3D>::New();
    glyph->SetSourceConnection(sphereSource->GetOutputPort());
    glyph->SetInputData(outletPolyData);
    glyph->ScalingOff();
    glyph->Update();
    
    // Create mapper and actor
    vtkSmartPointer<vtkPolyDataMapper> outletMapper = 
        vtkSmartPointer<vtkPolyDataMapper>::New();
    outletMapper->SetInputConnection(glyph->GetOutputPort());
    
    actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(outletMapper);
    actor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
    actor->GetProperty()->SetOpacity(0.7);  // Semi-transparent
    
    // Add to renderer
    renderer->AddActor(actor);
}

void VTKTerrainVisualizer::setGridVisible(bool visible)
{
    gridVisible = visible;
    
    if (gridVisible) {
        createGridLines();
    } else {
        renderer->RemoveActor(gridActor);
    }
    
    renderWindow()->Render();
}

void VTKTerrainVisualizer::setGridInterval(int interval)
{
    if (interval < 1) interval = 1;
    gridInterval = interval;
    
    if (gridVisible) {
        createGridLines();
        renderWindow()->Render();
    }
}

void VTKTerrainVisualizer::setRulersVisible(bool visible)
{
    rulersVisible = visible;
    
    if (rulersVisible) {
        createRulers();
    } else {
        renderer->RemoveActor(rulersActor);
    }
    
    renderWindow()->Render();
}

QPoint VTKTerrainVisualizer::screenToTerrainCoordinates(const QPoint& screenPos)
{
    // Convert screen coordinates to world coordinates using the renderer
    int displayX = screenPos.x();
    int displayY = height() - screenPos.y(); // Flip Y for VTK coordinate system
    
    // Get the 3D world coordinates from the display coordinates
    // First, find if we're clicking on the terrain
    vtkSmartPointer<vtkPicker> picker = 
        vtkSmartPointer<vtkPicker>::New();
    picker->SetTolerance(0.001);
    
    if (picker->Pick(displayX, displayY, 0, renderer)) {
        double worldPos[3];
        picker->GetPickPosition(worldPos);
        
        // Convert world position to grid coordinates
        int gridI = (int)(worldPos[1] / resolution + 0.5);
        int gridJ = (int)(worldPos[0] / resolution + 0.5);
        
        // Clamp to grid bounds
        gridI = std::max(0, std::min(gridHeight - 1, gridI));
        gridJ = std::max(0, std::min(gridWidth - 1, gridJ));
        
        return QPoint(gridI, gridJ);
    }
    
    // If not clicking on terrain, try a projection approach
    double clickPos[3];
    double clickDir[3];
    
    renderer->SetDisplayPoint(displayX, displayY, 0);
    renderer->DisplayToWorld();
    renderer->GetWorldPoint(clickPos);
    
    renderer->SetDisplayPoint(displayX, displayY, 1);
    renderer->DisplayToWorld();
    renderer->GetWorldPoint(clickDir);
    
    // Calculate the ray's direction vector
    clickDir[0] = clickDir[0] - clickPos[0];
    clickDir[1] = clickDir[1] - clickPos[1];
    clickDir[2] = clickDir[2] - clickPos[2];
    
    // Project to z=0 plane for 2D mode
    if (!view3DEnabled) {
        if (clickDir[2] != 0) {
            double t = -clickPos[2] / clickDir[2];
            double intersectX = clickPos[0] + t * clickDir[0];
            double intersectY = clickPos[1] + t * clickDir[1];
            
            int gridI = (int)(intersectY / resolution + 0.5);
            int gridJ = (int)(intersectX / resolution + 0.5);
            
            // Clamp to grid bounds
            gridI = std::max(0, std::min(gridHeight - 1, gridI));
            gridJ = std::max(0, std::min(gridWidth - 1, gridJ));
            
            return QPoint(gridI, gridJ);
        }
    }
    
    // Default if no intersection found
    return QPoint(-1, -1);
}

void VTKTerrainVisualizer::resetCamera()
{
    if (!renderer) return;
    
    // Reset the camera to show the entire terrain
    renderer->ResetCamera();
    
    // For 2D view, set up an orthographic top-down view
    if (!view3DEnabled) {
        vtkCamera* camera = renderer->GetActiveCamera();
        camera->ParallelProjectionOn();
        camera->SetViewUp(0, 1, 0);
        camera->SetPosition(gridWidth * resolution / 2, gridHeight * resolution / 2, 
                            gridWidth * resolution * 2);
        camera->SetFocalPoint(gridWidth * resolution / 2, gridHeight * resolution / 2, 0);
    }
    
    renderWindow()->Render();
}

void VTKTerrainVisualizer::zoomIn()
{
    if (!renderer) return;
    
    vtkCamera* camera = renderer->GetActiveCamera();
    
    if (camera->GetParallelProjection()) {
        // For parallel projection, adjust the parallel scale
        double scale = camera->GetParallelScale();
        camera->SetParallelScale(scale * 0.8);
    } else {
        // For perspective projection, move the camera closer
        double pos[3], focal[3], viewUp[3];
        camera->GetPosition(pos);
        camera->GetFocalPoint(focal);
        camera->GetViewUp(viewUp);
        
        // Calculate vector from focal point to camera
        double vec[3] = {pos[0] - focal[0], pos[1] - focal[1], pos[2] - focal[2]};
        
        // Scale the vector to zoom in
        vec[0] *= 0.8;
        vec[1] *= 0.8;
        vec[2] *= 0.8;
        
        // Calculate new position
        pos[0] = focal[0] + vec[0];
        pos[1] = focal[1] + vec[1];
        pos[2] = focal[2] + vec[2];
        
        camera->SetPosition(pos);
    }
    
    renderWindow()->Render();
}

void VTKTerrainVisualizer::zoomOut()
{
    if (!renderer) return;
    
    vtkCamera* camera = renderer->GetActiveCamera();
    
    if (camera->GetParallelProjection()) {
        // For parallel projection, adjust the parallel scale
        double scale = camera->GetParallelScale();
        camera->SetParallelScale(scale * 1.25);
    } else {
        // For perspective projection, move the camera farther
        double pos[3], focal[3], viewUp[3];
        camera->GetPosition(pos);
        camera->GetFocalPoint(focal);
        camera->GetViewUp(viewUp);
        
        // Calculate vector from focal point to camera
        double vec[3] = {pos[0] - focal[0], pos[1] - focal[1], pos[2] - focal[2]};
        
        // Scale the vector to zoom out
        vec[0] *= 1.25;
        vec[1] *= 1.25;
        vec[2] *= 1.25;
        
        // Calculate new position
        pos[0] = focal[0] + vec[0];
        pos[1] = focal[1] + vec[1];
        pos[2] = focal[2] + vec[2];
        
        camera->SetPosition(pos);
    }
    
    renderWindow()->Render();
}

void VTKTerrainVisualizer::rotateView(double azimuth, double elevation)
{
    if (!renderer) return;
    
    vtkCamera* camera = renderer->GetActiveCamera();
    
    camera->Azimuth(azimuth);
    camera->Elevation(elevation);
    camera->OrthogonalizeViewUp();
    
    renderWindow()->Render();
}

void VTKTerrainVisualizer::set3DViewEnabled(bool enabled)
{
    if (view3DEnabled == enabled) return;
    
    view3DEnabled = enabled;
    
    // Update the terrain mesh to show elevation in 3D
    createTerrainMesh();
    
    // Update the grid lines
    if (gridVisible) {
        createGridLines();
    }
    
    // Update outlet points
    if (simulationEngine) {
        updateOutlets(simulationEngine->getManualOutletCells(), 
                      simulationEngine->getAutomaticOutletCells());
    }
    
    // Update the camera view
    resetCamera();
    
    // Force a render
    renderWindow()->Render();
}

void VTKTerrainVisualizer::mousePressEvent(QMouseEvent* event)
{
    // For left button clicks, emit the terrainClicked signal
    if (event->button() == Qt::LeftButton) {
        QPoint terrainPos = screenToTerrainCoordinates(event->pos());
        if (terrainPos.x() >= 0 && terrainPos.y() >= 0) {
            emit terrainClicked(terrainPos);
        }
        
        // Start dragging
        dragging = true;
        lastMousePos = event->pos();
    }
    
    // Pass event to parent for standard VTK interaction
    QVTKOpenGLNativeWidget::mousePressEvent(event);
}

void VTKTerrainVisualizer::mouseMoveEvent(QMouseEvent* event)
{
    // Handle dragging for camera control
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->pos() - lastMousePos;
        
        // Adjust camera position based on drag
        if (view3DEnabled) {
            rotateView(-delta.x() * 0.5, -delta.y() * 0.5);
        } else {
            // For 2D mode, pan the camera
            vtkCamera* camera = renderer->GetActiveCamera();
            
            double viewFocus[4], viewPoint[3];
            camera->GetFocalPoint(viewFocus);
            camera->GetPosition(viewPoint);
            
            camera->SetFocalPoint(viewFocus[0] - delta.x() * resolution * 0.1,
                                  viewFocus[1] + delta.y() * resolution * 0.1,
                                  viewFocus[2]);
                                  
            camera->SetPosition(viewPoint[0] - delta.x() * resolution * 0.1,
                                viewPoint[1] + delta.y() * resolution * 0.1,
                                viewPoint[2]);
            
            renderWindow()->Render();
        }
        
        lastMousePos = event->pos();
    }
    
    // Pass event to parent for standard VTK interaction
    QVTKOpenGLNativeWidget::mouseMoveEvent(event);
}

void VTKTerrainVisualizer::mouseReleaseEvent(QMouseEvent* event)
{
    // Stop dragging
    if (event->button() == Qt::LeftButton) {
        dragging = false;
    }
    
    // Pass event to parent for standard VTK interaction
    QVTKOpenGLNativeWidget::mouseReleaseEvent(event);
}

void VTKTerrainVisualizer::wheelEvent(QWheelEvent* event)
{
    // Handle mouse wheel for zooming
    int delta = event->angleDelta().y();
    
    if (delta > 0) {
        zoomIn();
    } else if (delta < 0) {
        zoomOut();
    }
    
    // Pass event to parent for standard VTK interaction
    QVTKOpenGLNativeWidget::wheelEvent(event);
}

// Helper method to create minimal render components in case of initialization failure
void VTKTerrainVisualizer::createMinimalRenderComponents()
{
    try {
        qDebug() << "Creating minimal render components to avoid crashes";
        renderer = vtkSmartPointer<vtkRenderer>::New();
        terrainPoints = vtkSmartPointer<vtkPoints>::New();
        elevationData = vtkSmartPointer<vtkFloatArray>::New();
        terrainPolyData = vtkSmartPointer<vtkPolyData>::New();
        terrainMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        terrainActor = vtkSmartPointer<vtkActor>::New();
        elevationLUT = vtkSmartPointer<vtkLookupTable>::New();
        gridActor = vtkSmartPointer<vtkActor>::New();
        manualOutletsActor = vtkSmartPointer<vtkActor>::New();
        autoOutletsActor = vtkSmartPointer<vtkActor>::New();
        rulersActor = vtkSmartPointer<vtkActor>::New();
    }
    catch (...) {
        qDebug() << "Failed to create even minimal VTK renderer components";
    }
} 