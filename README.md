# Advanced Hydrological Simulation Application

[![Qt](https://img.shields.io/badge/Qt-6.10.0-green.svg)](https://www.qt.io)
[![GDAL](https://img.shields.io/badge/GDAL-latest-blue.svg)](https://gdal.org)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Quick Start

1. **Install Required Software**
   ```bash
   # Download and install:
   - Qt 6.10.0 MSVC2022 64-bit
   - GDAL (via OSGeo4W)
   - Visual Studio 2022 with C++
   ```

2. **Configure Environment**
   ```bash
   # Add to PATH:
   C:/OSGeo4W/bin
   C:/OSGeo4W/apps/gdal-dev/bin
   C:/Qt/6.10.0/msvc2022_64/bin
   ```

3. **Build and Run**
   ```bash
   # Debug build
   cmake -DCMAKE_BUILD_TYPE=Debug -B build .
   cmake --build build --config Debug
   
   # Launch application
   ./build/Desktop_Qt_6_10_0_MSVC2022_64bit-Debug/BTP_GUI.exe
   ```

## 1. Overview

A Qt-based desktop application for simulating surface water flow and drainage basin behavior using Digital Elevation Models (DEMs). The application implements a physically-based hydrological model incorporating Manning's equation for flow resistance, infiltration processes, and advanced drainage routing algorithms.

## 2. Features

- **DEM Input Support**:
  - GeoTIFF format with automatic resolution detection via GDAL
  - CSV format with custom resolution specification
  - Automatic handling of no-data values (internally mapped to -999999.0)
  - Supports square-cell DEMs with resolutions from 0.001m to 1000m
  - Automatic resolution detection from GeoTIFF geotransform metadata

- **Simulation Parameters**:
  - Manning's roughness coefficient (n) for flow resistance calculation
  - Soil infiltration rate (Ks) in m/s
  - Minimum water depth threshold for flow calculation
  - Variable time step with mass conservation
  - Constant or time-varying rainfall schedules with custom intervals

- **Drainage Configuration**:
  - Automatic outlet detection using elevation percentile analysis
  - Manual outlet placement via interactive GUI
  - Multiple outlet support (unlimited) with per-outlet drainage tracking
  - Adaptive drainage factor based on water accumulation and simulation time
  - Supports both boundary and interior outlet cells

- **Visualization Features**:
  - Real-time water depth visualization with dynamic color mapping
  - DEM preview with elevation-based color gradients
  - Flow accumulation visualization with logarithmic scaling
  - Interactive pan/zoom with mouse controls
  - Configurable grid overlay (default interval: 10 cells)
  - Optional coordinate rulers with adaptive intervals
  - Color-coded outlet markers (red=manual, blue=automatic)
  - Legend showing elevation range and outlet types

- **Analysis and Export**:
  - Time series drainage data collection
  - Per-outlet drainage volume tracking
  - CSV export with simulation parameters
  - Real-time monitoring of water balance

## 3. Dependencies Installation

### Required Software
1. **Qt Framework Setup**:
   ```bash
   # Download Qt Online Installer
   # Install the following components:
   - Qt 6.10.0 MSVC2022 64-bit
   - Qt Creator 16.0.1
   - CMake 3.16+
   - Ninja build system
   ```

2. **GDAL Installation**:
   ```bash
   # Using OSGeo4W Installer
   - Download OSGeo4W Network Installer
   - Select Advanced Install
   - Choose gdal-dev package
   - Install PROJ dependencies
   ```

3. **Visual Studio 2022**:
   - Install Desktop development with C++
   - Include MSVC v143 build tools
   - Windows 11 SDK

### Environment Setup
```bash
# Add to System Path:
C:/OSGeo4W/bin
C:/OSGeo4W/apps/gdal-dev/bin
C:/Qt/6.10.0/msvc2022_64/bin

# Set Environment Variables:
GDAL_DATA=C:/OSGeo4W/apps/gdal-dev/share/gdal
PROJ_LIB=C:/OSGeo4W/share/proj
```

## 4. Recent Changes (as of April 25, 2025)

### Build System Updates
- Migrated to explicit Qt 6.10.0 paths
- Added PROJ dependency tracking
- Improved GDAL DLL handling
- Forced 64-bit build architecture

### Code Improvements
- Enhanced depression filling algorithm
- Optimized flow accumulation calculation
- Added adaptive drainage factors
- Improved visualization performance

## Command Line Interface

The application supports command-line arguments for automated processing:

```bash
Usage: BTP_GUI.exe [options]

Options:
  --dem <path>          Path to DEM file (GeoTIFF or CSV)
  --manning <value>     Manning's coefficient [default: 0.03]
  --infiltration <val>  Infiltration rate in m/s [default: 1e-6]  
  --rainfall <value>    Rainfall rate in m/s [default: 0.000028]
  --duration <secs>     Simulation duration in seconds [default: 1800]
  --auto-outlets        Use automatic outlet detection [default]
  --outlet-pct <val>    Percentile for auto outlets [default: 0.1]
  --export-results      Export results after simulation
  --export-path <dir>   Directory for exported files [default: ./results]
  
Examples:
  # Run simulation with default parameters
  BTP_GUI.exe --dem input.tif --export-results

  # Specify custom parameters
  BTP_GUI.exe --dem input.tif --manning 0.05 --rainfall 0.00005 --duration 3600

  # Batch processing with export
  BTP_GUI.exe --dem input.tif --auto-outlets --outlet-pct 0.15 --export-path results/

Notes:
- Units are in meters and seconds
- Use scientific notation for small values (e.g., 1e-6)
- Paths can be absolute or relative to working directory
```

## Example Workflows

### Basic Simulation
1. Load a DEM file (e.g., resources/DEM_Central_Park.tif)
2. Accept default Manning's coefficient (0.03)
3. Set constant rainfall rate (0.000028 m/s ≈ 100mm/hour)
4. Use automatic outlet selection (10th percentile)
5. Run simulation for 1800s (30 minutes)

### Advanced Configuration
1. Load high-resolution DEM
2. Configure time-varying rainfall:
   - 0s: 0.000014 m/s (50mm/hour)
   - 300s: 0.000028 m/s (100mm/hour)
   - 600s: 0.000042 m/s (150mm/hour)
   - 900s: 0.000014 m/s (50mm/hour)
3. Manually place outlets at low points
4. Adjust infiltration rate based on soil type
5. Run extended simulation (3600s)

### Batch Processing
```bash
# Example script for processing multiple DEMs
for dem in resources/DEM_*.tif; do
    # Launch application with command line arguments
    ./BTP_GUI.exe --dem "$dem" --auto-outlets \
                  --rainfall 0.000028 \
                  --duration 1800 \
                  --export-results
done
```

## FAQ (Extended)

### Setup and Installation

1. **Q: Which Qt version should I use exactly?**
   - A: Qt 6.10.0 MSVC2022 64-bit is required
   - A: Earlier/later versions may work but are untested

2. **Q: How do I prepare my DEM data?**
   - A: Ensure square pixels (equal X/Y resolution)
   - A: Convert to GeoTIFF if possible
   - A: Fill obvious data gaps
   - A: Check coordinate system is defined

### Runtime Issues

3. **Q: Simulation seems too slow**
   - A: Check DEM resolution vs area size
   - A: Reduce output frequency
   - A: Use automatic outlet selection
   - A: Consider running Release build

4. **Q: Water accumulating unrealistically**
   - A: Verify DEM has no data holes
   - A: Check infiltration rate setting
   - A: Ensure sufficient outlets
   - A: Validate rainfall rate units

### Data Analysis

5. **Q: How to interpret flow accumulation?**
   - A: Brighter blue = higher flow volume
   - A: Check legend for scale
   - A: Compare with terrain visualization
   - A: Look for natural channel patterns

6. **Q: Export data for further analysis?**
   - A: Use "Save Results" for CSV output
   - A: Screenshots capture visualizations
   - A: Time series data includes all metrics
   - A: Per-outlet statistics available

## 5. Architecture Details

### Signal-Slot Communication Flow

1. **MainWindow ↔ SimulationEngine**:
   ```cpp
   // Core Simulation Signals
   SimulationEngine::simulationTimeUpdated(double currentTime, double totalTime)
   SimulationEngine::simulationStepCompleted(const QImage& waterDepthImage)

   // UI Control Flow
   MainWindow::onSimulationStep() → SimulationEngine::stepSimulation()
   MainWindow::onStartSimulation() → SimulationEngine::initSimulation()
   ```

2. **Parameter Updates**:
   ```cpp
   // Real-time parameter synchronization
   MainWindow::onManningCoefficientChanged() → SimulationEngine::setManningCoefficient()
   MainWindow::onInfiltrationRateChanged() → SimulationEngine::setInfiltrationRate()
   MainWindow::onTimeVaryingRainfallToggled() → SimulationEngine::setTimeVaryingRainfall()
   ```

### Class Responsibilities

1. **SimulationEngine**:
   - Core Simulation Logic
   ```cpp
   class SimulationEngine : public QObject {
       // State Management
       std::vector<std::vector<double>> dem;      // Elevation data
       std::vector<std::vector<double>> h;        // Water depths
       std::vector<std::vector<double>> flowAcc;  // Flow accumulation

       // Simulation Parameters
       double resolution;     // Cell size (m)
       double n_manning;      // Manning's coefficient
       double Ks;            // Infiltration rate (m/s)
       double min_depth;     // Water depth threshold
       double dt;            // Time step (s)

       // Key Methods
       void stepSimulation();              // Core computation step
       void routeWaterToOutlets();        // Drainage handling
       void computeOutletCellsByPercentile(double);  // Outlet detection
   };
   ```

2. **MainWindow**:
   - UI Management & Event Handling
   ```cpp
   class MainWindow : public QMainWindow {
       // Timers for simulation control
       QTimer *simTimer;        // Simulation steps
       QTimer *uiUpdateTimer;   // UI refresh

       // Visualization state
       float zoomLevel;
       QPoint panOffset;
       QImage currentSimulationImage;

       // Core simulation interface
       void setupSimulationEngineConnections();
       void updateUIState();
       void updateDisplay();
   };
   ```

### Core Algorithms Implementation

1. **Water Flow Routing** (SimulationEngine::stepSimulation):
   ```cpp
   // 1. Initial water balance
   for each cell (i,j):
       h[i][j] += (rainfall - infiltration) * dt

   // 2. Flow calculation (Manning's equation)
   for each cell (i,j):
       for each neighbor (ni,nj):
           S = (H[i][j] - H[ni][nj]) / resolution
           A = h[i][j] * resolution
           R = h[i][j]
           Q = (A * pow(R, 2.0/3.0) * sqrt(S)) / n_manning
           Q_out[i][j] += Q

   // 3. Mass conservation
   for each cell (i,j):
       V_t = h[i][j] * cellArea
       if (Q_total_out[i][j] * dt > V_t):
           c = V_t / (Q_total_out[i][j] * dt)
           Q_out[i][j] *= c
   ```

2. **Depression Handling** (SimulationEngine::fillDepressions):
   ```cpp
   while (!depressionsFilled && iterations < MAX_ITERATIONS):
       for each cell (i,j):
           if isDepression(i,j):
               h_fill = min(neighbor_elevations) - 0.01
               dem[i][j] = h_fill
               depressionsFilled = false
   ```

3. **Flow Accumulation** (SimulationEngine::calculateFlowAccumulation):
   ```cpp
   // D8 flow direction algorithm
   for each cell (i,j):
       flow_dir = maxSlope(dem, i, j)
       flowAccumulation[i][j] = 1.0
       while hasDownstreamCell(flow_dir):
           downstream_cell = getDownstreamCell(flow_dir)
           flowAccumulation[downstream_cell] += flowAccumulation[i][j]
   ```

### Numerical Methods and Stability

1. **Adaptive Time Stepping**
   ```cpp
   // Flow-based timestep adjustment
   dt = base_dt
   if (max_velocity > velocity_threshold):
       dt *= stability_factor * dx / max_velocity
   dt = min(dt, max_timestep)
   ```

2. **Flow Direction Computation**
   ```cpp
   // D8 flow direction with diagonal compensation
   foreach direction in [0..7]:
       distance = (direction % 2 == 0) ? 
           resolution : resolution * 1.414
       slope = dElev / distance
       if (slope > maxSlope):
           maxSlope = slope
           flowDir = direction
   ```

3. **Depression Filling Convergence**
   ```cpp
   // Convergence criteria
   maxIterations = 3
   convergenceThreshold = 0.01
   foreach iteration:
       changeCount = fillDepressions()
       if (changeCount == 0 || 
           maxChange < convergenceThreshold):
           break
   ```

4. **Volume Conservation**
   ```cpp
   // Per-cell volume tracking
   Q_total = 0.0
   foreach direction in [N,E,S,W]:
       Q = manning_equation(direction)
       Q_total += Q
   
   if (Q_total * dt > available_volume):
       scale_factor = available_volume / (Q_total * dt)
       Q *= scale_factor  // Scale all fluxes
   ```

5. **Numerical Thresholds**
   ```cpp
   // Critical thresholds
   MIN_SLOPE = 1e-6          // Minimum slope for flow
   MIN_DEPTH = 1e-5          // Minimum water depth
   MAX_FLOW_ITER = 100       // Flow routing iterations
   FLAT_AREA_THRESH = 0.01   // Depression detection
   ```

6. **Flow Accumulation Stability**
   ```cpp
   // Logarithmic accumulation scaling
   if (flowValue > 0):
       normalized = log(flowValue + 1) / log(maxFlow + 1)
   else:
       normalized = 0
   
   // Prevent numerical instability
   normalized = max(0.0, min(1.0, normalized))
   ```

### Drainage Path Optimization

1. **Outlet Selection Algorithm**
   ```cpp
   // Boundary cell analysis
   foreach boundary_cell in DEM:
       if elevation > NO_DATA_VALUE:
           boundaryCells.push_back({elevation, index})
   
   // Adaptive outlet count
   numOutlets = min(
       max(1, percentile * boundaryCells.size()),
       min(boundaryCells.size() * 0.1, 50)
   )
   ```

2. **Path Finding Logic**
   ```cpp
   // For each outlet, create multiple drainage paths
   maxPaths = 3
   foreach outlet in outlets:
       pathLength = min(15, nx/2)
       for pathId in [0..maxPaths):
           // Score calculation for next cell selection
           score = flowAccumulation * 0.7 +    // Flow weight
                  elevationDiff * 1.5 +        // Uphill preference
                  (isUpstream ? 2.5 : 0.0) +   // Upstream bonus
                  (isFlat ? -1.0 : 0.0)        // Flat area penalty
   ```

3. **Depression Handling**
   ```cpp
   // Depression filling with adaptive elevation adjustment
   while (!depressionsFilled && iterations < 3):
       foreach cell in DEM:
           if isDepression(cell):
               newElev = min(neighborElevations) - 0.01
               markForUpdate(cell, newElev)
       applyUpdates()
       checkDepressionsFilled()
   ```

### Advanced Flow Routing

1. **Adaptive Drainage Factor**
   ```cpp
   // Dynamic adjustment based on system state
   systemWaterThreshold = 1.0
   baseDrainageFactor = 1.0
   
   if (totalSystemWater > threshold):
       drainageFactor = 1.0 + min(2.0, (totalSystemWater - threshold) / 10.0)
   
   // Time-based scaling
   timeProgress = min(1.0, time / 120.0)
   timeFactor = 0.7 + 0.3 * timeProgress
   drainageFactor *= timeFactor
   ```

2. **Flow Path Generation**
   ```cpp
   // Multiple path generation per outlet
   foreach outlet in outlets:
       // Primary path
       path = findUpstreamPath(outlet)
       
       // Secondary paths (offset)
       if (canCreateLeftPath):
           path = findUpstreamPath(outlet.left())
       if (canCreateRightPath):
           path = findUpstreamPath(outlet.right())
   ```

3. **Path Scoring System**
   ```cpp
   // Cell selection criteria
   flowScore = flowAccumulation[cell] * 0.7
   elevScore = max(0.0, elevation_diff) * 1.5
   upstreamScore = isUpstream ? 2.5 : 0.0
   flatPenalty = isFlat ? -1.0 : 0.0
   totalScore = flowScore + elevScore + upstreamScore + flatPenalty
   ```

### Mass Conservation and Boundary Conditions

1. **Volume Balance**
   ```cpp
   // Per-cell volume tracking
   cellArea = resolution * resolution
   foreach cell in grid:
       if (isValidCell):
           totalSystemWater += h[cell] * cellArea
   ```

2. **Outlet Flow**
   ```cpp
   // Manning's equation with adaptive scaling
   foreach outlet in outlets:
       if (h[outlet] > min_depth):
           A = h[outlet] * resolution
           Q = 2.5 * drainageFactor * 
               (A * pow(h[outlet], 2/3) * sqrt(S)) / n_manning
           vol = min(Q * dt, 0.95 * availableVolume)
           updateDrainage(outlet, vol)
   ```

3. **Flux Limitations**
   ```cpp
   // Directional flow components
   Q_out[4] = {N, E, S, W}
   foreach direction in [0..3]:
       if (slope > 0):
           Q = calculateManning()
           Q_total += Q
           Q_out[direction] = Q
   
   // Mass conservation scaling
   if (Q_total * dt > availableVolume):
       scale = availableVolume / (Q_total * dt)
       Q_out *= scale
   ```

### Performance Optimizations

1. **Path Finding Optimizations**
   - Path length capped at min(15, nx/2)
   - Early termination for invalid paths
   - Cached path indices to prevent loops

2. **Flow Accumulation**
   - Progressive calculation from outlets
   - Logarithmic scaling for visualization
   - Cached intermediate results

3. **Memory Management**
   - Pre-allocated flow direction arrays
   - Reusable path containers
   - Efficient grid traversal patterns

### Visualization Algorithms

1. **Water Depth Visualization**
   ```cpp
   // Color mapping algorithm:
   normalizedDepth = depth / maxDepth
   blue = 255  // Constant blue component
   red = green = 255 * (1.0 - normalizedDepth)  // White to blue gradient
   ```

2. **DEM Preview Coloring**
   ```cpp
   // Elevation-based terrain coloring:
   normalizedElev = (elevation - minElev) / elevRange
   red = 155 + 100 * normalizedElev    // 155-255 range
   green = 200 - 60 * normalizedElev   // 140-200 range
   blue = 50 + 40 * normalizedElev     // 50-90 range
   ```

3. **Flow Accumulation Visualization**
   ```cpp
   // Logarithmic flow path visualization:
   normalizedFlow = log(flowValue + 1.0) / log(maxFlow + 1.0)
   blue = 255 * normalizedFlow
   green = 150 * normalizedFlow
   red = 50  // Constant base red for terrain contrast
   ```

### Grid and Ruler System

1. **Dynamic Grid Interval**
   ```cpp
   // Adaptive grid spacing based on resolution
   if (resolution > 5.0m):
       interval = max(1, gridInterval / 2)
   else:
       interval = gridInterval  // Default: 10
   ```

2. **Scale Calculation**
   ```cpp
   // Dynamic scaling based on resolution and grid size
   scale = 4  // Base scale
   if resolution <= 0.5: scale = 6
   else if resolution <= 1.0: scale = 5
   else if resolution <= 5.0: scale = 4
   else: scale = 3
   if (nx > 300 || ny > 300): scale = 2
   ```

### Outlet Visualization

1. **Manual Outlets**
   - Red circles (RGBA: 255,0,0,150)
   - Scale-adjusted size
   - Interactive placement

2. **Automatic Outlets**
   - Blue circles (RGBA: 0,150,255,150)
   - Based on elevation percentile
   - Updated dynamically

3. **Legend System**
   ```cpp
   // Compact legend layout
   legendWidth = 15
   legendHeight = 80
   position = (width - legendWidth - 10, 10)
   ```

### Image Generation Pipeline

1. **Base Layer Creation**
   - Initialize blank image (white background)
   - Apply DEM color mapping
   - Add grid overlay if enabled

2. **Data Visualization Layer**
   - Water depth coloring
   - Flow accumulation paths
   - Depression markers

3. **UI Elements Layer**
   - Grid lines (40% opacity black)
   - Rulers and coordinates
   - Legend and scale information

4. **Interactive Elements**
   - Mouse interaction overlays
   - Selection highlights
   - Pan/zoom indicators

### Performance Optimization and Visualization Pipeline

1. **Thread Management**
   ```cpp
   // Visualization thread decoupling
   - Main thread: UI and user interaction
   - Simulation thread: Core computations
   - Render thread: Image generation and scaling
   ```

2. **Image Pipeline Optimization**
   ```cpp
   // Layer compositing strategy
   base_layer = generateBaseLayer()      // Terrain + grid (cached)
   data_layer = generateDataLayer()      // Water depth/flow (dynamic)
   ui_layer = generateUILayer()          // Markers + text (overlay)
   
   // Partial updates
   if (onlyWaterChanged):
       updateDataLayerOnly()
   if (onlyViewChanged):
       updateTransformationOnly()
   ```

3. **Memory Optimization**
   ```cpp
   // Image buffer management
   - Double buffering for smooth updates
   - Shared image data between threads
   - Precomputed color maps
   - Cached scale transformations
   ```

4. **View Transformation Pipeline**
   ```cpp
   // Efficient zoom and pan
   scale_matrix = createScaleMatrix(zoomLevel)
   translation_matrix = createTranslationMatrix(panOffset)
   combined_transform = scale_matrix * translation_matrix
   
   // Optimized region updates
   visible_rect = calculateVisibleRegion(combined_transform)
   updateVisibleRegionOnly(visible_rect)
   ```

5. **Render Queue Management**
   ```cpp
   // Priority-based rendering
   HIGH_PRIORITY:   User interface elements
   MEDIUM_PRIORITY: Active simulation data
   LOW_PRIORITY:    Background terrain updates
   
   // Frame skipping logic
   if (renderQueueSize > threshold):
       skipIntermediateFrames()
   ```

6. **Cache Management**
   ```cpp
   // Multi-level caching
   L1_CACHE: Current view transformations
   L2_CACHE: Recent water depth states
   L3_CACHE: Precomputed terrain visualizations
   
   // Cache invalidation
   onParameterChange():
       invalidateRelevantCaches()
   onViewChange():
       invalidateViewCache()
   ```

7. **Resource Management**
   ```cpp
   // GPU resource optimization
   - Texture atlas for UI elements
   - Shared vertex buffers
   - Batched rendering calls
   
   // CPU resource management
   - Thread pool for computations
   - Worker thread affinity
   - Load balancing
   ```

8. **Debug Performance Metrics**
   ```cpp
   // Performance monitoring
   - Frame timing statistics
   - Memory usage tracking
   - Cache hit/miss ratios
   - Thread utilization
   ```

### User Interface Integration

1. **Real-time Updates**
   ```cpp
   onSimulationStep():
       1. Update water depths
       2. Generate visualization
       3. Apply current view transformations
       4. Refresh display
   ```

2. **View Transformations**
   - Zoom levels: 0.25x to 4x
   - Pan with boundary constraints
   - Automatic centering options

3. **Performance Optimizations**
   - Double-buffered rendering
   - Cached intermediate results
   - Resolution-dependent detail levels

### Export Capabilities

1. **Image Export**
   - Full resolution DEM preview
   - Current simulation state
   - Flow accumulation visualization

2. **Data Export**
   - Time series drainage data
   - Outlet-specific volumes
   - Flow accumulation statistics

### Color Scheme Design

1. **Water Depth**
   - White (no water) → Blue (deep water)
   - RGB ranges optimized for visibility
   - Special handling for no-data cells

2. **Terrain Elevation**
   - Green-brown gradient base
   - Enhanced contrast at boundaries
   - Consistent with cartographic standards

3. **Flow Accumulation**
   - Blue intensity for flow volume
   - Terrain context preservation
   - Logarithmic scale for detail

### Memory Management

1. **Grid Data Structures**:
   - Pre-allocated 2D vectors
   - Contiguous memory layout
   - Automatic cleanup via RAII

2. **Visualization Optimization**:
   - Double-buffered rendering
   - Cached intermediate results
   - Efficient QImage manipulation

3. **Large DEM Handling**:
   - Progressive loading
   - Tiled processing
   - Memory-mapped file support

### GeoTIFF Handling and Coordinate Systems

1. **Resolution Detection**
   ```cpp
   // GeoTransform parsing
   adfGeoTransform[1] = pixel width (X resolution)
   adfGeoTransform[5] = pixel height (Y resolution)
   
   // Resolution validation
   if (abs(resX - resY) < 1e-6):
       resolution = resX
   else:
       resolution = resX  // Warning: Non-square pixels
   ```

2. **NoData Value Management**
   ```cpp
   // NoData handling chain
   GeoTIFF NoData → Internal NoData (-999999.0) → Visualization (Gray)
   
   // NoData detection
   noDataValue = band->GetNoDataValue(&hasNoData)
   if (!hasNoData):
       noDataValue = -999999.0  // Default fallback
   ```

3. **Metadata Validation**
   ```cpp
   // Core metadata checks
   - Band count validation
   - Data type verification
   - Coordinate system presence
   - Transform matrix validity
   ```

4. **Data Loading**
   ```cpp
   // Row-by-row loading with validation
   foreach row in DEM:
       status = band->RasterIO(GF_Read, 0, row, cols, 1, 
                             buffer, cols, 1, GDT_Float64, 0, 0)
       if (status != CE_None):
           handleError()
   ```

5. **Error Recovery**
   ```cpp
   // Safe data loading
   if (!dataset):
       logGDALError()
       return false
   
   if (!band):
       GDALClose(dataset)
       return false
   ```

6. **Memory Efficiency**
   ```cpp
   // Progressive loading strategy
   - Row-by-row reading
   - Buffer reuse
   - Immediate NoData conversion
   ```

### Resource Management and Lifecycle

1. **Initialization Chain**
   ```cpp
   // MainWindow initialization
   MainWindow::MainWindow():
       1. Setup Qt styling and window properties
       2. Initialize UI components (setupUI)
       3. Create SimulationEngine instance
       4. Initialize timers and state flags
       5. Setup signal/slot connections
   
   // SimulationEngine initialization
   SimulationEngine::SimulationEngine():
       1. Set default parameters
       2. Initialize grid dimensions
       3. Setup visualization properties
   ```

2. **Resource Allocation**
   ```cpp
   // Core data structures
   std::vector<std::vector<double>> dem;    // DEM grid (RAII)
   std::vector<std::vector<double>> h;      // Water depths
   QMap<QPoint, double> perOutletDrainage;  // Drainage tracking
   
   // UI resources
   QTimer* simTimer;         // Simulation updates
   QTimer* uiUpdateTimer;    // UI refresh timer
   QImage currentDEMImage;   // Cached DEM visualization
   ```

3. **Safe State Management**
   ```cpp
   // Simulation state reset
   void initSimulation():
       time = 0.0
       dt = 1.0
       drainageVolume = 0.0
       h.assign(nx, std::vector<double>(ny, 0.0))
       drainageTimeSeries.clear()
       perOutletDrainage.clear()
   
   // UI state reset
   void resetDisplayView():
       zoomLevel = 1.0
       panOffset = QPoint(0, 0)
       updateDisplay()
   ```

4. **Exception Safety**
   ```cpp
   // Safe grid initialization
   try:
       h.assign(nx, std::vector<double>(ny, 0.0))
   catch (std::exception& e):
       qDebug() << "Grid allocation failed:" << e.what()
       return false
   
   // Safe DEM loading
   if (!dataset):
       logGDALError()
       return false
   ```

5. **Resource Cleanup**
   ```cpp
   // GDAL cleanup
   GDALClose(poDataset)
   
   // Qt cleanup (automatic via parent-child)
   - UI elements deleted with MainWindow
   - Timers cleaned up via Qt parent system
   - SimulationEngine deleted as child of MainWindow
   ```

6. **Thread Safety**
   ```cpp
   // UI thread synchronization
   void updateDisplay():
       scrollArea->setUpdatesEnabled(false)
       // Perform updates
       scrollArea->setUpdatesEnabled(true)
       QApplication::processEvents()
   ```

7. **Memory Management**
   ```cpp
   // Visualization buffer management
   - Double-buffered display updates
   - Cached DEM preview images
   - Automatic cleanup of Qt resources
   ```

8. **Error Recovery**
   ```cpp
   // Simulation error handling
   if (!initSuccess):
       resetState()
       updateUIState()
       notifyUser()
   
   // DEM loading recovery
   if (loadError):
       cleanup()
       resetUI()
       showErrorMessage()
   ```

## 6. Building and Testing

### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DGDAL_DIR="C:/OSGeo4W/apps/gdal-dev/lib/cmake/gdal" -B build .
cmake --build build --config Debug
```

### Release Build
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DGDAL_DIR="C:/OSGeo4W/apps/gdal-dev/lib/cmake/gdal" -B build .
cmake --build build --config Release
```

### Testing
1. **DEM Validation**:
   - Resolution bounds checking
   - No-data value handling
   - Coordinate system validation

2. **Mass Conservation**:
   - Input/output volume balance
   - Boundary condition verification
   - Outlet discharge monitoring

3. **Performance Testing**:
   - Large DEM processing
   - Memory usage tracking
   - UI responsiveness

## 7. Performance Considerations

1. **Memory Management**
   - Pre-allocated grid structures
   - Efficient water routing algorithms
   - Optimized visualization updates

2. **Computational Optimization**
   - Vectorized grid operations
   - Cached flow calculations
   - Adaptive update intervals

3. **Large DEM Handling**
   - Automatic scale adjustment
   - Memory-efficient grid storage
   - Progressive loading for large files

## 8. Project Structure
```
BTP_GUI/
├── CMakeLists.txt          # Build configuration
├── main.cpp                # Application entry
├── mainwindow.cpp/h        # GUI implementation
├── SimulationEngine.cpp/h  # Core simulation
└── resources/              # DEM test files
```

## 9. Version History

- v0.1.0 - Initial release
  - Basic DEM loading
  - Water flow simulation
  - Visualization system

## 10. Development Guidelines

1. **Coding Standards**
   - C++17 features usage
   - Qt best practices
   - GDAL integration patterns

2. **Testing**
   - DEM validation
   - Mass conservation checks
   - Boundary condition testing

3. **Documentation**
   - Code comments
   - Algorithm descriptions
   - Parameter explanations

## 11. License

MIT License

## 12. Contact

For questions or contributions, please contact the maintainer at `kshitijbhaskar22@gmail.com`.

## 13. Acknowledgments

- GDAL Development Team
- Qt Framework Team
- Original research papers on hydraulic modeling

### Error Handling and Validation

1. **DEM Loading Validation**
   ```cpp
   // Input data validation
   - Resolution bounds: 0.001m to 1000m
   - Non-empty grid dimensions
   - Square pixel validation (GeoTIFF)
   - No-data value handling (-999999.0)
   ```

2. **Parameter Range Checking**
   ```cpp
   // Core parameter validation
   if (resolution <= 0 || n_manning <= 0 || totalTime <= 0):
       return error
   
   // Grid dimension validation
   if (nx <= 0 || ny <= 0):
       return error
   ```

3. **Outlet Configuration Safeguards**
   ```cpp
   // Automatic outlet fallback logic
   if (boundaryCells.empty()):
       // Find globally lowest cell as fallback
       minElev = max_double
       for each cell in DEM:
           if (elevation < minElev):
               minElev = elevation
               minIdx = cellIndex
   ```

4. **Depression Handling**
   ```cpp
   // Adaptive depression filling
   MAX_ITERATIONS = 3
   while (!depressionsFilled && iterations < MAX_ITERATIONS):
       foreach cell:
           if (isDepressionCell):
               newElev = min(neighborElevs) - 0.01
               markCellForUpdate(cell, newElev)
   ```

5. **Mass Conservation**
   ```cpp
   // Volume balance checks
   foreach outlet in outlets:
       Q = calculateManning()
       vol = Q * dt
       if (vol > availableVolume * 0.95):
           vol = availableVolume * 0.95
   ```

6. **Boundary Condition Handling**
   ```cpp
   // Edge case handling
   - No-data cell skipping
   - Grid boundary checks
   - Diagonal flow adjustments
   - Depression boundary detection
   ```

7. **Numerical Stability**
   ```cpp
   // Flow calculation stability
   if (Q_total_out * dt > V_t):
       c = V_t / (Q_total_out * dt)
       Q_out *= c  // Scale to prevent over-drainage
   ```

8. **UI Input Validation**
   ```cpp
   // Parameter input constraints
   - Manning's n > 0
   - Resolution within bounds
   - Time step positive
   - Water depth threshold > 0
   ```

9. **Memory Management**
   ```cpp
   // Safe initialization
   try:
       h.assign(nx, std::vector<double>(ny, 0.0))
   catch (std::exception& e):
       handleAllocationError()
   ```

10. **Error Recovery**
    ```cpp
    // Graceful degradation
    - Fallback to default resolution
    - Automatic outlet recalculation
    - Safe state reset on failure
    ```

11. **Data Format Validation**
    ```cpp
    // File format checks
    - GeoTIFF metadata validation
    - CSV format verification
    - Uniform column count check
    - No-data value detection
    ```

12. **Runtime Checks**
    ```cpp
    // Simulation stability
    - Water depth bounds
    - Flow direction validation
    - Time step constraints
    - Outlet flow monitoring
    ```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

