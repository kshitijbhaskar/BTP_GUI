# Advanced Hydrological Simulation Application

## 1. Overview

A Qt-based desktop application for simulating drainage basin hydrological behavior using Digital Elevation Models (DEMs). It supports GeoTIFF and CSV inputs, configurable physical parameters, and real-time visualization of water depth and drainage.

## 2. Features

- **DEM Loading**: GeoTIFF via GDAL or custom CSV format
- **Simulation Parameters**:
  - Cell resolution (m)
  - Manning's roughness coefficient
  - Infiltration rate (Ks)
  - Minimum water depth threshold
  - Constant or time-varying rainfall schedules
- **Outlet Selection**:
  - Automatic: lowest-boundary percentile
  - Manual: interactive cell selection
- **Visualization**:
  - Water depth heatmap
  - Time-series drainage tracking
  - Grid/ruler overlays with adjustable interval
- **User Interface**:
  - Input Parameters tab
  - Rainfall Configuration tab
  - DEM Preview & Manual Outlet Selection tab
  - Simulation Results tab with controls (Start/Pause/Stop)

## 3. Prerequisites

- **Qt** (5.15+ or 6.x) with Widgets module
- **GDAL** (>= 3.0) for GeoTIFF support
- **C++17** compiler (GCC, Clang, MSVC)
- **CMake** (>= 3.14) or QMake

## 4. Build Instructions

```bash
# Clone repository
git clone <repo_url> hydrosim
cd hydrosim

# Using CMake\mkdir build && cd build
cmake ..
cmake --build . --config Release

# Or using QMake
qmake ../hydrosim.pro
make
```

## 5. Running the Application

1. Launch the executable (`hydrosim` or `hydrosim.exe`).
2. In **Input Parameters**, browse and load a DEM (GeoTIFF/CSV).
3. Configure resolution, Manning's coefficient, infiltration, and rainfall.
4. Switch to **Rainfall Configuration** to define time-varying schedules (optional).
5. In **Outlet Selection**, choose automatic or manual mode.
6. Go to **Simulation Results** and click **Start**.
7. Observe water depth visualization and drainage progress.
8. Use **Save Results** to export data and images.

## 6. Code Structure

```
hydrosim/
├── CMakeLists.txt      # Build configuration
├── src/
│   ├── main.cpp        # Application entry point
│   ├── MainWindow.h    # UI class definition
│   ├── mainwindow.cpp  # UI layout and signal/slot wiring
│   ├── SimulationEngine.h
│   └── SimulationEngine.cpp  # Core hydrological model
└── resources/          # Icons, sample DEMs, etc.
```

## 7. Extensibility

- **Parallelization**: integrate OpenMP or QtConcurrent for large DEMs
- **Algorithm Improvements**: add depression filling and flow accumulation preprocessing
- **Plugin System**: support custom rainfall/interflow modules

## 8. License

MIT License. See [LICENSE](LICENSE) for details.

## 9. Contact

For questions or contributions, please open an issue or contact the maintainer at `kshitijbhaskar22@gmail.com`.

