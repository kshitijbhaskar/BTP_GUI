#include "SimulationEngine.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <limits>
#include <QImage>
#include <QPainter>
#include <QColor>
#include <QPen>
#include <QFileInfo>
#include <queue>

// For OpenMP support
#ifdef _OPENMP
#include <omp.h>
#endif

// Include GDAL headers
#include "gdal_priv.h"
#include "ogr_spatialref.h"

SimulationEngine::SimulationEngine(QObject *parent)
    : QObject(parent),
    nx(0), ny(0),
    resolution(0.25),
    n_manning(0.03),
    Ks(1e-6),
    min_depth(1e-5),
    totalTime(1800.0),
    time(0.0),
    dt(1.0),
    rainfallRate(0.001),
    useTimeVaryingRainfall(false),
    outletRow(0),
    useManualOutlets(false),
    outletPercentile(0.1),
    drainageVolume(0.0),
    showGrid(true),
    gridInterval(10)
{
    lastUpdateTime = QDateTime::currentDateTime();
}

void SimulationEngine::createGrid(int _nx, int _ny, double _resolution)
{
    // Store grid dimensions
    nx = _nx;
    ny = _ny;
    resolution = _resolution;
    
    // Reserve space for data arrays to improve performance
    int size = nx * ny;
    dem.resize(size, 0.0);
    h.resize(size, 0.0);
    flowAccumulationGrid.resize(size, 0.0);
    isActive.resize(size, 0);
    
    // Initialize neighbors array
    neighbors.resize(size);
    
    // Precompute neighbor indices for each cell
    #pragma omp parallel for
    for(int i=0; i<nx; ++i) {
        for(int j=0; j<ny; ++j) {
            int k = idx(i, j);
            
            // Order: left, top, right, bottom (W, N, E, S)
            neighbors[k][0] = (i > 0) ? idx(i-1, j) : -1;         // left
            neighbors[k][1] = (j < ny-1) ? idx(i, j+1) : -1;      // top
            neighbors[k][2] = (i < nx-1) ? idx(i+1, j) : -1;      // right
            neighbors[k][3] = (j > 0) ? idx(i, j-1) : -1;         // bottom
        }
    }
    
    // Initialize active cells with whole domain
    activeCells.clear();
    nextActiveCells.clear();
    
    qDebug() << "Grid created:" << nx << "x" << ny << "with resolution" << resolution << "m";
}

bool SimulationEngine::loadDEM(const QString &filename)
{
    // Determine file type based on extension
    QFileInfo fileInfo(filename);
    QString suffix = fileInfo.suffix().toLower();

    // Register GDAL drivers (only needs to be done once)
    GDALAllRegister();

    if (suffix == "tif" || suffix == "tiff")
    {
        // --- Load GeoTIFF using GDAL --- 
        qDebug() << "Attempting to load GeoTIFF file:" << filename;
        GDALDataset *poDataset = (GDALDataset *) GDALOpen(filename.toStdString().c_str(), GA_ReadOnly);
        
        if (poDataset == NULL)
        {
            qDebug() << "GDAL failed to open file:" << filename;
            qDebug() << "GDAL Error:" << CPLGetLastErrorMsg();
            return false;
        }
        
        // Get raster dimensions
        nx = poDataset->GetRasterYSize(); // Number of rows
        ny = poDataset->GetRasterXSize(); // Number of columns
        
        qDebug() << "DEM dimensions (nx, ny):" << nx << ny;
        
        if (nx <= 0 || ny <= 0) {
            qDebug() << "Invalid dimensions read from GeoTIFF.";
            GDALClose(poDataset);
            return false;
        }
        
        // Get geotransform for resolution
        double adfGeoTransform[6];
        if (poDataset->GetGeoTransform(adfGeoTransform) == CE_None)
        {
            // adfGeoTransform[1] is pixel width (X resolution)
            // adfGeoTransform[5] is pixel height (Y resolution, usually negative)
            double resX = std::abs(adfGeoTransform[1]);
            double resY = std::abs(adfGeoTransform[5]);
            
            // Debug output to help diagnose resolution issues
            qDebug() << "Full GeoTransform array:";
            for (int i = 0; i < 6; i++) {
                qDebug() << "  adfGeoTransform[" << i << "] =" << adfGeoTransform[i];
            }
            
            // Check for valid resolution values
            if (resX < 0.000001 || resY < 0.000001) {
                qDebug() << "Warning: Invalid GeoTransform values detected. Using default resolution:" << resolution;
            } else {
                // Assuming square pixels for simplicity, check if they are close
                if (std::abs(resX - resY) < 1e-6) {
                    resolution = resX;
                    qDebug() << "GeoTIFF resolution set to:" << resolution << "meters/pixel";
                } else {
                    qDebug() << "Warning: Non-square pixels detected (resX:" << resX << ", resY:" << resY << "). Using X resolution.";
                    resolution = resX;
                }
                
                // Apply a sanity check for resolution bounds (prevent extremely small or large values)
                if (resolution < 0.001) {
                    qDebug() << "Warning: Resolution too small (" << resolution << "), clamping to 0.001 m";
                    resolution = 0.001;
                } else if (resolution > 1000) {
                    qDebug() << "Warning: Resolution too large (" << resolution << "), clamping to 1000 m";
                    resolution = 1000;
                }
                qDebug() << "Final resolution set to:" << resolution << "meters/pixel";
            }
        }
        else
        {
            qDebug() << "Warning: Could not get GeoTransform from GeoTIFF. Using default resolution:" << resolution;
            // Keep the default or user-set resolution if geotransform is missing
        }
        
        // Get the first raster band (assuming single band DEM)
        GDALRasterBand *poBand = poDataset->GetRasterBand(1);
        if (poBand == NULL) {
             qDebug() << "Could not get raster band 1 from GeoTIFF.";
             GDALClose(poDataset);
             return false;
        }

        // Get NoData value if available
        int bGotNoData;
        double noDataValue = poBand->GetNoDataValue(&bGotNoData);
        if (!bGotNoData) {
            noDataValue = -999999.0; // Use a default if not specified
            qDebug() << "NoData value not found in TIF, using default:" << noDataValue;
        }
        qDebug() << "Using NoData value:" << noDataValue;

        // Allocate memory for flattened DEM data
        dem.resize(nx * ny);
        std::vector<double> rowData(ny);

        // Read data row by row
        bool success = true;
        for (int i = 0; i < nx; ++i)
        {
            CPLErr eErr = poBand->RasterIO(GF_Read, 0, i, ny, 1, 
                                         &rowData[0], ny, 1, GDT_Float64, 
                                         0, 0);
            if (eErr != CE_None)
            {
                qDebug() << "GDAL RasterIO failed reading row" << i;
                success = false;
                break;
            }
            // Assign row data to flattened array, handle NoData values
            for (int j = 0; j < ny; ++j) {
                if (bGotNoData && std::abs(rowData[j] - noDataValue) < 1e-6) { // Compare with tolerance
                    dem[idx(i,j)] = -999999.0; // Standard internal NoData value
                } else {
                    dem[idx(i,j)] = rowData[j];
                }
            }
        }

        // Close the GDAL dataset
        GDALClose(poDataset);
        
        if (!success) {
            return false;
        }

    }
    else if (suffix == "csv")
    {
        // --- Load CSV using original method but with flattened output --- 
        qDebug() << "Attempting to load CSV file:" << filename;
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;

        QTextStream in(&file);
        std::vector<std::vector<double>> tmpDEM;
        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();
            if (line.isEmpty())
                continue;
            // Use QRegularExpression and Qt::SkipEmptyParts instead of QRegExp and QString::SkipEmptyParts.
            QStringList tokens = line.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
            std::vector<double> row;
            for (const QString &str : tokens)
            {
                bool ok;
                double val = str.toDouble(&ok);
                if (!ok)
                    return false;
                row.push_back(val);
            }
            tmpDEM.push_back(row);
        }
        file.close();

        // Assume uniform number of columns
        if (tmpDEM.empty())
            return false;
            
        // Store dimensions
        nx = tmpDEM.size();
        ny = (nx > 0) ? tmpDEM[0].size() : 0;
        
        // Copy data to flattened array
        dem.resize(nx * ny);
        for (int i = 0; i < nx; i++) {
            for (int j = 0; j < ny; j++) {
                dem[idx(i,j)] = tmpDEM[i][j];
            }
        }
        
        // Keep the user-defined or default resolution for CSV
        qDebug() << "CSV loaded. Dimensions (nx, ny):" << nx << ny << ", Using resolution:" << resolution;
    }
    else
    {
        qDebug() << "Unsupported file format:" << suffix;
        return false;
    }

    // Common post-loading steps for both TIF and CSV
    if (nx <= 0 || ny <= 0) {
        qDebug() << "Error: Invalid grid dimensions after loading.";
        return false;
    }
    
    // Initialize water depth grid (h) to zero
    h.assign(nx * ny, 0.0);
    
    // Initialize flow accumulation grid
    flowAccumulationGrid.assign(nx * ny, 0.0);

    // Initialize active cells tracking vectors
    activeCells.clear();
    nextActiveCells.clear();
    isActive.assign(nx * ny, 0);

    // Precompute neighbor offsets
    neighbors.resize(nx * ny);
    int di[4] = {-1, 0, 1, 0}; // N, E, S, W
    int dj[4] = {0, 1, 0, -1};
    
    for(int i=0; i<nx; i++) {
        for(int j=0; j<ny; j++) {
            int k = idx(i, j);
            for(int d=0; d<4; d++) {
                int ni = i + di[d];
                int nj = j + dj[d];
                neighbors[k][d] = (ni>=0 && ni<nx && nj>=0 && nj<ny) ? idx(ni, nj) : -1;
            }
        }
    }

    // Initialize outlet-related members
    outletRow = nx - 1; // Default outlet row (may be changed by methods)
    useManualOutlets = false;
    computeDefaultAutomaticOutletCells(); // Compute default outlets based on loaded DEM
    drainageVolume = 0.0;
    
    qDebug() << "DEM loaded successfully. nx:" << nx << "ny:" << ny << "Resolution:" << resolution;
    
    return true;
}

void SimulationEngine::setRainfall(double rate)
{
    rainfallRate = rate;
}

void SimulationEngine::setManningCoefficient(double coefficient)
{
    n_manning = coefficient;
}

void SimulationEngine::setInfiltrationRate(double rate)
{
    Ks = rate;
}

void SimulationEngine::setMinWaterDepth(double depth)
{
    min_depth = depth;
}

void SimulationEngine::setCellResolution(double res)
{
    resolution = res;
}

void SimulationEngine::setTotalTime(double time)
{
    totalTime = time;
}

void SimulationEngine::configureOutletsByPercentile(double percentile)
{
    if (percentile <= 0.0 || percentile >= 1.0) {
        // Ensure percentile is in valid range
        percentile = 0.1; // Default to 10%
    }
    
    outletPercentile = percentile;  // Store the percentile value
    useManualOutlets = false;
    computeOutletCellsByPercentile(percentile);
}

void SimulationEngine::setManualOutletCells(const QVector<QPoint> &cells)
{
    if (cells.isEmpty() || nx <= 0 || ny <= 0)
        return;
    
    // Store manual outlet cells
    manualOutletCells = cells;
    useManualOutlets = true;
    
    // Convert to outlet cell indices for the simulation
    outletCells.clear();
    
    // Since outlets can now be anywhere, we just store the 1D indices
    // of all selected cells for the simulation to use
    for (const QPoint &p : cells) {
        // Only use points that are within grid bounds
        if (p.x() >= 0 && p.x() < nx && p.y() >= 0 && p.y() < ny) {
            // Store the outlet points as 1D indices
            outletCells.push_back(p.x() * ny + p.y());
        }
    }
    
    // If no valid cells were found, revert to automatic method
    if (outletCells.empty()) {
        useManualOutlets = false;
        computeDefaultAutomaticOutletCells();
    }
}

bool SimulationEngine::initSimulation()
{
    // Make basic validation checks
    if (nx <= 0 || ny <= 0)
    {
        qDebug() << "ERROR: Cannot initialize simulation - invalid grid dimensions:" << nx << "x" << ny;
        return false;
    }
    
    // Create grid with current dimensions if necessary
    if (dem.empty() || h.empty() || h.size() != nx * ny || neighbors.empty())
    {
        createGrid(nx, ny, resolution);
    }
    
    // Check if we have any valid outlet cells
    if (outletCells.empty())
    {
        qDebug() << "WARNING: No outlet cells defined. Attempting to compute default outlets.";
        computeDefaultAutomaticOutletCells();
        
        // Check again after attempting to compute default outlets
        if (outletCells.empty())
        {
            qDebug() << "ERROR: Still no outlet cells after computing defaults. Cannot initialize simulation.";
            return false;
        }
    }
    
    // Validate simulation parameters
    if (resolution <= 0)
    {
        qDebug() << "ERROR: Invalid cell resolution:" << resolution;
        return false;
    }
    
    if (n_manning <= 0)
    {
        qDebug() << "ERROR: Invalid Manning coefficient:" << n_manning;
        return false;
    }
    
    if (totalTime <= 0)
    {
        qDebug() << "ERROR: Invalid total simulation time:" << totalTime;
        return false;
    }
    
    // Reset simulation time and water depth grid
    time = 0.0;
    dt = 0.1; // Start with a small time step
    drainageVolume = 0.0;
    
    // Initialize water depth grid
    h.assign(nx * ny, 0.0);
    
    // Reset active cells tracking
    activeCells.clear();
    nextActiveCells.clear();
    isActive.assign(nx * ny, 0);
    
    // Initialize active cells for the first step (will be populated in stepSimulation by rainfall)
    qDebug() << "Initialized simulation with inactive cells (will activate when water appears)";
    
    // Clear previous time series data
    drainageTimeSeries.clear();
    
    // Clear per-outlet drainage data
    perOutletDrainage.clear();
    
    // Setup time-varying rainfall data
    if (useTimeVaryingRainfall && timeVaryingRainfall.isEmpty())
    {
        timeVaryingRainfall.append(qMakePair(0.0, rainfallRate * 3600.0)); // Convert to mm/hr for storage
    }
    
    // Initialize per-outlet drainage tracking for all outlet cells
    if (useManualOutlets)
    {
        qDebug() << "Using" << manualOutletCells.size() << "manual outlet cells";
        for (int i = 0; i < manualOutletCells.size(); i++)
        {
            const QPoint &p = manualOutletCells[i];
            perOutletDrainage[p] = 0.0;
        }
    }
    else
    {
        qDebug() << "Using" << outletCells.size() << "automatic outlet cells";
        QVector<QPoint> autoOutlets = getAutomaticOutletCells();
        for (int i = 0; i < autoOutlets.size(); i++)
        {
            const QPoint &p = autoOutlets[i];
            perOutletDrainage[p] = 0.0;
        }
    }
    
    // Add initial data point (time=0, drainage=0)
    drainageTimeSeries.append(qMakePair(0.0, 0.0));
    
    // Initialize last update time for performance tracking
    lastUpdateTime = QDateTime::currentDateTime();
    
    qDebug() << "Simulation initialization successful";
    return true;
}

// Compute outletCells based on lowest elevation percentile along the entire boundary
void SimulationEngine::computeOutletCellsByPercentile(double percentile)
{
    outletCells.clear();
    if (nx <= 0 || ny <= 0)
        return;

    std::vector<std::pair<double, int>> boundaryCells; // Store <elevation, 1D_index>

    // Iterate through all boundary cells (top, bottom, left, right)
    for (int i = 0; i < nx; ++i) 
    {
        for (int j = 0; j < ny; ++j) 
        {
            // Check if the cell is on the boundary
            if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1) 
            {
                // Check if it's a valid DEM cell (not no-data)
                if (dem[idx(i,j)] > -999998.0) 
                {
                    int index1D = i * ny + j;
                    boundaryCells.push_back({dem[idx(i,j)], index1D});
                }
            }
        }
    }

    if (boundaryCells.empty()) 
    {
        qDebug() << "No valid boundary cells found for automatic outlet selection.";
        // As a fallback, maybe select the globally lowest cell?
        double minElev = std::numeric_limits<double>::max();
        int minIdx = -1;
        for(int i=0; i<nx; ++i) 
        {
            for(int j=0; j<ny; ++j) 
            {
                if (dem[idx(i,j)] > -999998.0 && dem[idx(i,j)] < minElev) 
                {
                    minElev = dem[idx(i,j)];
                    minIdx = i * ny + j;
                }
            }
        }
        if(minIdx != -1) outletCells.push_back(minIdx);
        return;
    }

    // Sort boundary cells by elevation
    std::sort(boundaryCells.begin(), boundaryCells.end());

    // Determine the number of outlets based on percentile - fix ambiguous max
    int numOutlets = static_cast<int>(std::max(1.0, static_cast<double>(percentile * boundaryCells.size())));
    
    // Use multiple std::min calls instead of initializer list to avoid ambiguity
    int cap1 = static_cast<int>(std::min(static_cast<double>(numOutlets), static_cast<double>(boundaryCells.size() * 0.1)));
    int cap2 = static_cast<int>(std::min(static_cast<double>(cap1), 50.0));
    numOutlets = static_cast<int>(std::max(1.0, static_cast<double>(cap2)));

    qDebug() << "Selecting top" << numOutlets << "lowest boundary cells as automatic outlets.";

    // Add the lowest 'numOutlets' cells to outletCells
    for (int k = 0; k < numOutlets; ++k) 
    {
        outletCells.push_back(boundaryCells[k].second);
        int i = boundaryCells[k].second / ny;
        int j = boundaryCells[k].second % ny;
        qDebug() << "Added automatic outlet at:" << i << j << "with elevation:" << boundaryCells[k].first;
    }

    qDebug() << "Selected" << outletCells.size() << "automatic outlet cells along boundary.";
}

void SimulationEngine::setTimeVaryingRainfall(bool enabled)
{
    useTimeVaryingRainfall = enabled;
}

void SimulationEngine::setRainfallSchedule(const QVector<QPair<double, double>>& schedule)
{
    // Clear existing schedule
    timeVaryingRainfall.clear();
    
    // Copy and sort the schedule by timestamp
    timeVaryingRainfall = schedule;
    
    // Sort using a lambda function
    if (timeVaryingRainfall.size() > 1)
    {
        std::sort(timeVaryingRainfall.begin(), timeVaryingRainfall.end(), 
                  [](const QPair<double, double>& a, const QPair<double, double>& b) -> bool
                  {
                  return a.first < b.first;
              });
    }
    
    // Ensure first entry is at time 0
    if (!timeVaryingRainfall.isEmpty() && timeVaryingRainfall.first().first > 0.0)
    {
        QPair<double, double> firstEntry = qMakePair(0.0, timeVaryingRainfall.first().second);
        timeVaryingRainfall.prepend(firstEntry);
    }
    
    // If schedule is empty, add a single entry with the current constant rate
    if (timeVaryingRainfall.isEmpty())
    {
        QPair<double, double> defaultEntry = qMakePair(0.0, rainfallRate);
        timeVaryingRainfall.append(defaultEntry);
    }
}

double SimulationEngine::getCurrentRainfallRate() const
{
    if (!useTimeVaryingRainfall || timeVaryingRainfall.isEmpty())
    {
        return rainfallRate; // Fall back to constant rate
    }
    
    // Find the applicable rainfall rate for the current time
    double currentRate = timeVaryingRainfall.first().second; // Default to first rate
    
    for (int i = 0; i < timeVaryingRainfall.size(); i++)
    {
        const QPair<double, double>& entry = timeVaryingRainfall[i];
        
        // If this entry's time is in the future, use the previous entry's rate
        if (entry.first > time)
        {
            break;
        }
        
        // Update current rate
        currentRate = entry.second;
    }
    
    return currentRate;
}

void SimulationEngine::stepSimulation()
{
    // Basic validation
    if (dem.empty() || h.empty()) 
    {
        qDebug() << "Cannot step simulation: DEM or water depth grid is empty";
        return;
    }

    // Skip if all zero water depth
    bool allZero = true;
    for (int i = 0; i < (int)h.size(); i++) 
    {
        if (h[i] > 0.0) 
        {
            allZero = false;
            break;
        }
    }

    // Debug output every few steps
    if (int(time) % 10 == 0) {
        qDebug() << ">>> Simulation Time:" << time << "s";
        qDebug() << "    Current Rainfall Rate:" << rainfallRate << "m/s";
        qDebug() << "    Total Drainage So Far:" << drainageVolume << "m³";
        qDebug() << "    Active Cells:" << activeCells.size();
    }

    // Check if we need to initialize active cells
    if (activeCells.empty() && !allZero) 
    {
        // Initialize active cells for first time
        for (int i = 0; i < nx; i++) 
        {
            for (int j = 0; j < ny; j++) 
            {
                int k = idx(i, j);
                if (h[k] > min_depth) 
                {
                    activeCells.push_back(k);
                    isActive[k] = 1;
                }
            }
        }
        
        // Sort active cells for efficient binary search
        std::sort(activeCells.begin(), activeCells.end());
    }

    // Return if nothing is active and no rainfall
    if (activeCells.empty() && allZero && rainfallRate <= 0 && !useTimeVaryingRainfall) 
    {
        qDebug() << "No active cells and no rainfall, skipping step";
        return;
    }

    // For mass conservation tracking
    double cellArea = resolution * resolution;
    double totalSystemWater = 0.0;
    
    // Pre-allocate vectors based on active cells size (not whole domain)
    std::vector<double> Q_out(activeCells.size() * 4, 0.0);  // 4 neighbors per cell
    std::vector<double> Q_total_out(activeCells.size(), 0.0);
    
    // Use sparse approach for delta_h to save memory
    std::unordered_map<int, double> delta_h;
    delta_h.reserve(activeCells.size() * 2);  // Reserve a reasonable size

    // Get current rainfall rate (either constant or time-varying)
    double rRate = rainfallRate;
    if (useTimeVaryingRainfall && timeVaryingRainfall.size() > 0) 
    {
        // Find appropriate time slot
        for (int i = 0; i < (int)timeVaryingRainfall.size(); i++) 
        {
            if (time >= timeVaryingRainfall[i].first) 
            {
                rRate = timeVaryingRainfall[i].second / 3600.0; // Convert from mm/hr to m/s
            } 
            else 
            {
                break;
            }
        }
    }

    // Apply rainfall to ALL cells (not just active ones)
    if (rRate > 0) {
        #pragma omp parallel for
        for (int i = 0; i < nx; i++) {
            for (int j = 0; j < ny; j++) {
                int k = idx(i, j);
                
                // Skip invalid DEM cells
                if (dem[k] <= -999998.0) continue;
                
                // Apply rainfall
                h[k] += rRate * dt;
                
                // Apply infiltration using Green-Ampt model
                double infiltration = std::min<double>(h[k], Ks * dt);
                h[k] -= infiltration;
                
                // Activate if cell has water
                if (h[k] > min_depth && !isActive[k]) {
                    #pragma omp critical
                    {
                        isActive[k] = 1;
                        if (std::find(activeCells.begin(), activeCells.end(), k) == activeCells.end()) {
                            activeCells.push_back(k);
                        }
                    }
                }
            }
        }
        
        // Re-sort active cells if new ones were added
        std::sort(activeCells.begin(), activeCells.end());
    }
    
    // Calculate total water in system
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] > -999998.0) {
                totalSystemWater += h[k] * cellArea;
            }
        }
    }
    
    // Debug output for water volume
    if (int(time) % 10 == 0) {
        qDebug() << "    Total System Water:" << totalSystemWater << "m³";
    }
    
    // Calculate outflows for each active cell
    int active_count = 0;
    for (int a = 0; a < (int)activeCells.size(); a++) 
    {
        int k = activeCells[a];
        if (h[k] <= min_depth) 
        {
            continue; // Skip cells with minimal water
        }

        double total_out = 0.0;
        
        // Vector to store outflows to each direction
        std::vector<std::pair<int, double>> outflows;
        outflows.reserve(4); // Maximum of 4 outflows (4 neighbors)
        
        // Manning's formula: Q = (1/n) * A * R^(2/3) * S^(1/2)
        // For shallow water, R ~= h, A ~= h * width
        
        // Calculate outflows in 4 directions using precomputed neighbors
        for (int dir = 0; dir < 4; dir++) 
        {
            int nb_idx = neighbors[k][dir];
            if (nb_idx < 0) 
            {
                continue; // Skip invalid neighbors
            }
            
            double h_diff = (dem[k] + h[k]) - (dem[nb_idx] + h[nb_idx]);
            if (h_diff <= 0) 
            {
                continue; // Skip if water doesn't flow this way
            }
            
            double slope = h_diff / resolution;
            double Q = (1.0 / n_manning) * h[k] * pow(h[k], 2.0/3.0) * sqrt(slope) * resolution;
            
            // Check if outflow would remove too much water
            double h_local_max = h[k] * cellArea / (resolution * dt);
            Q = std::min<double>(Q, h_local_max); // Use explicit template argument
            
            std::pair<int, double> outflow_pair;
            outflow_pair.first = nb_idx;
            outflow_pair.second = Q;
            outflows.push_back(outflow_pair);
            total_out += Q;
        }
        
        // Mass conservation scaling for this cell's outflows
        if (total_out > 0) 
        {
            double maxOutVolume = h[k] * cellArea;
            double totalOutVolume = total_out * dt;
            
            if (totalOutVolume > maxOutVolume) 
            {
                double ratio = maxOutVolume / totalOutVolume;
                
                // Scale all outflows for this cell
                for (int o = 0; o < (int)outflows.size(); o++) 
                {
                    outflows[o].second *= ratio;
                }
                total_out = maxOutVolume / dt;
            }
            
            // Store delta_h for source cell
            #pragma omp critical
            {
                delta_h[k] -= total_out * dt / cellArea;
            }
            
            // Update delta_h for receiving cells
            for (int o = 0; o < (int)outflows.size(); o++) 
            {
                int nb_idx = outflows[o].first;
                double flow = outflows[o].second;
                
                #pragma omp critical
                {
                    delta_h[nb_idx] += flow * dt / cellArea;
                }
            }
        }
        
        Q_total_out[active_count] = total_out;
        active_count++;
    }
    
    // Update water depths and mark active cells for next iteration
    nextActiveCells.clear();
    
    // Use iterator-based loop for the map to avoid any structured binding issues
    for (std::unordered_map<int, double>::iterator it = delta_h.begin(); it != delta_h.end(); ++it) 
    {
        int idx = it->first;
        double dh = it->second;
        
        h[idx] += dh;
        
        // Only mark as active if it has significant water
        if (h[idx] > min_depth) 
        {
            isActive[idx] = 1;
            nextActiveCells.push_back(idx);
            
            // Also check neighbors for next step
            for (int dir = 0; dir < 4; dir++) 
            {
                int nb_idx = neighbors[idx][dir];
                if (nb_idx >= 0 && !isActive[nb_idx]) 
                {
                    isActive[nb_idx] = 1;
                    nextActiveCells.push_back(nb_idx);
                }
            }
        } 
        else 
        {
            h[idx] = 0.0; // Reset to exactly zero
            isActive[idx] = 0;
        }
    }
    
    // Process outlet cells to handle drainage
    routeWaterToOutlets(); 

    // Swap active cell lists for next step
    std::swap(activeCells, nextActiveCells);
    
    // Sort active cells for efficient binary search in future steps
    std::sort(activeCells.begin(), activeCells.end());
    
    // Update simulation time
    time += dt;
    
    // Emit progress signals
    emit simulationTimeUpdated(time, totalTime);
    
    // Emit progress signal every 5 steps
    static int step_count = 0;
    step_count++;
    if (step_count >= 5) 
    {
        step_count = 0;
        QDateTime currentTime = QDateTime::currentDateTime();
        if (lastUpdateTime.msecsTo(currentTime) > 50) 
        { 
            // Update at most every 50ms
            emit simulationUpdated();
            emit simulationStepCompleted(getWaterDepthImage());
            lastUpdateTime = currentTime;
        }
    }
}

double SimulationEngine::getTotalDrainage() const
{
    return drainageVolume;
}

QVector<QPair<double, double>> SimulationEngine::getDrainageTimeSeries() const
{
    return drainageTimeSeries;
}

QImage SimulationEngine::getWaterDepthImage() const
{
    if (nx <= 0 || ny <= 0)
    {
        return QImage();
    }

    // Create image
    QImage img(ny, nx, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    // Draw water depths
    for (int i = 0; i < nx; i++)
    {
        for (int j = 0; j < ny; j++)
        {
            int k = idx(i, j);
            
            // Skip invalid cells
            if (i < 0 || i >= nx || j < 0 || j >= ny || h.empty())
            {
                continue;
            }
            
            if (h[k] > min_depth)
            {
                // Scale water depth to blue intensity
                // Use a logarithmic scale to better show small water depths
                double depth = h[k];
                // Clamp depth to a reasonable range for visualization
                depth = std::min<double>(depth, 0.1); // Decrease from 2.0 to 0.1 to make small depths more visible
                double depthValue = std::min(1.0, log10(1.0 + depth * 1000.0) / 2.0); // Increase from 100.0 to 1000.0
                int blueValue = static_cast<int>(255 * depthValue);
                
                // Regular water: blue with alpha based on depth
                img.setPixel(j, i, qRgba(0, 128, 255, 200)); // Brighter blue (0, 128, 255 instead of 0, 64, blueValue)
            }
        }
    }
    
    return img;
}

QImage SimulationEngine::getDEMPreviewImage() const
{
    if (nx <= 0 || ny <= 0)
        return QImage();

    // Define margins for rulers and text
    int rulerMargin = 30; // Space for rulers and coordinates
    int topMargin = 40; // Space for title and instructions

    // Calculate scale based on resolution
    int scale = 4; // Increase default scale factor from 2 to 4
    if (resolution <= 0.5) scale = 6;
    else if (resolution <= 1.0) scale = 5;
    else if (resolution <= 5.0) scale = 4;
    else scale = 3;
    if (nx > 300 || ny > 300) scale = 2;
    
    // Calculate image dimensions including margins
    int demWidth = ny * scale;
    int demHeight = nx * scale;
    int totalWidth = demWidth + 2 * rulerMargin;
    int totalHeight = demHeight + topMargin + rulerMargin;

    QImage img(totalWidth, totalHeight, QImage::Format_RGB32);
    img.fill(QColor(240, 240, 240)); // Fill background with light gray
    
    // Define the rectangle for the actual DEM display
    QRect demRect(rulerMargin, topMargin, demWidth, demHeight);

    // Find min/max elevations
    double minElev = std::numeric_limits<double>::max();
    double maxElev = std::numeric_limits<double>::lowest();
    const double NO_DATA_VALUE = -999999.0;
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            if (dem[idx(i,j)] > NO_DATA_VALUE + 1) { 
                minElev = std::min(minElev, dem[idx(i,j)]);
                maxElev = std::max(maxElev, dem[idx(i,j)]);
            }
        }
    }
    double elevRange = maxElev - minElev;
    if (elevRange <= 0) elevRange = 1.0;
    
    // Create a temporary QImage for the DEM itself
    QImage demOnlyImg(demWidth, demHeight, QImage::Format_RGB32);

    // Draw DEM color map onto the temporary image
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            QColor cellColor;
            if (dem[idx(i,j)] <= NO_DATA_VALUE + 1) { 
                cellColor = QColor(200, 200, 200);
            } else {
                double normalizedElev = (dem[idx(i,j)] - minElev) / elevRange;
                int red = int(155 + 100 * normalizedElev);
                int green = int(200 - 60 * normalizedElev);
                int blue = int(50 + 40 * normalizedElev);
                cellColor = QColor(red, green, blue);
            }
            // Draw scaled cell onto demOnlyImg
            for (int si = 0; si < scale; si++) {
                for (int sj = 0; sj < scale; sj++) {
                    demOnlyImg.setPixel(j * scale + sj, i * scale + si, cellColor.rgb());
                }
            }
        }
    }

    // --- Painting Starts --- 
    QPainter painter(&img);
    painter.drawImage(demRect, demOnlyImg);
    painter.setPen(QPen(Qt::black, 1)); 
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(demRect.adjusted(-1, -1, 1, 1));

    // Draw Grid (inside the DEM area)
    if (showGrid) {
        painter.setPen(QPen(QColor(0, 0, 0, 40), 0.5));
        int interval = gridInterval;
        if (resolution > 5.0) interval = std::max(1, gridInterval / 2);
        // Horizontal gridlines
        for (int i = 0; i <= nx; i += interval) {
            painter.drawLine(demRect.left(), demRect.top() + i * scale, demRect.right(), demRect.top() + i * scale);
        }
        // Vertical gridlines
        for (int j = 0; j <= ny; j += interval) {
            painter.drawLine(j, 0, j, nx);
        }
    }
    
    // Draw Rulers and Coordinates (in the margins)
    if (showRulers) {
        painter.setPen(Qt::black);
        int rulerInterval = gridInterval * 2;
        if (resolution > 5.0) {
            rulerInterval = static_cast<int>(std::max(1.0, static_cast<double>(gridInterval)));
        }
        else if (resolution < 1.0) {
            rulerInterval = gridInterval * 3;
        }
        QFont rulerFont = painter.font();
        rulerFont.setPointSize(8); // Smaller font for rulers
        painter.setFont(rulerFont);
        
        // Vertical Ruler (Left Margin)
        for (int i = 0; i < nx; i += rulerInterval) {
            int yPos = demRect.top() + i * scale + (scale / 2);
            painter.drawLine(rulerMargin - 5, yPos, rulerMargin, yPos);
            painter.drawText(5, yPos + 4, QString::number(i));
        }
        // Horizontal Ruler (Bottom Margin)
        for (int j = 0; j < ny; j += rulerInterval) {
            int xPos = demRect.left() + j * scale + (scale / 2);
            painter.drawLine(xPos, demRect.bottom(), xPos, demRect.bottom() + 5);
            painter.drawText(xPos - 5, demRect.bottom() + 15, QString::number(j));
        }
    }

    // Mark selected outlet cells (inside the DEM area)
    painter.setPen(Qt::NoPen);
    
    // First draw automatic outlets (if any and not using manual)
    if (!useManualOutlets) {
        painter.setBrush(QColor(0, 150, 255, 150)); // Blue for automatic outlets
        QVector<QPoint> autoOutlets = getAutomaticOutletCells();
        for (const QPoint &p : autoOutlets) {
            if (p.x() >= 0 && p.x() < nx && p.y() >= 0 && p.y() < ny) {
                int cellX = demRect.left() + p.y() * scale;
                int cellY = demRect.top() + p.x() * scale;
                // Draw a circle for automatic outlets
                painter.drawEllipse(cellX, cellY, scale, scale);
            }
        }
    }
    
    // Draw manual outlets on top
    painter.setBrush(QColor(255, 0, 0, 150)); // Red for manual outlets
    for (const QPoint &p : manualOutletCells) {
        if (p.x() >= 0 && p.x() < nx && p.y() >= 0 && p.y() < ny) {
            int cellX = demRect.left() + p.y() * scale;
            int cellY = demRect.top() + p.x() * scale;
            // Draw a circle instead of a rectangle for less visual clutter
            painter.drawEllipse(cellX, cellY, scale, scale);
        }
    }

    // Draw Legend (Top Right) - Smaller and simpler
    int legendWidth = 15;
    int legendHeight = 80;
    int legendX = totalWidth - legendWidth - 10;
    int legendY = 10;
    QFont legendFont = painter.font();
    legendFont.setPointSize(7); // Even smaller font
    painter.setFont(legendFont);
    
    // Elevation Legend
    painter.setPen(Qt::black);
    painter.drawText(legendX - 2, legendY - 2, "Elev.");
    for (int y = 0; y < legendHeight; y++) {
        double normalizedElev = 1.0 - double(y) / legendHeight;
        int red = int(155 + 100 * normalizedElev);
        int green = int(200 - 60 * normalizedElev);
        int blue = int(50 + 40 * normalizedElev);
        painter.setPen(QColor(red, green, blue));
        painter.drawLine(legendX, legendY + y, legendX + legendWidth, legendY + y);
    }
    painter.setPen(Qt::black);
    painter.drawText(legendX + legendWidth + 1, legendY + 6, QString::number(maxElev, 'f', 0));
    painter.drawText(legendX + legendWidth + 1, legendY + legendHeight - 1, QString::number(minElev, 'f', 0));
    
    // Outlet Legend Marker
    painter.setBrush(QColor(255, 0, 0, 150));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(legendX + legendWidth/2 - 3, legendY + legendHeight + 5, 6, 6); // Small circle
    painter.setPen(Qt::black);
    painter.drawText(legendX + legendWidth + 2, legendY + legendHeight + 12, "Manual Outlet");
    
    // Add automatic outlet to legend if not using manual outlets
    if (!useManualOutlets) {
        painter.setBrush(QColor(0, 150, 255, 150));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(legendX + legendWidth/2 - 3, legendY + legendHeight + 20, 6, 6); // Small circle
        painter.setPen(Qt::black);
        painter.drawText(legendX + legendWidth + 2, legendY + legendHeight + 27, "Auto Outlet");
    }

    // Instructions and Info (Top Margin) - Simplified
    QFont infoFont = painter.font();
    infoFont.setPointSize(9);
    painter.setFont(infoFont);
    painter.setPen(Qt::darkGray); // Less prominent color
    painter.drawText(rulerMargin, 15, "Click DEM to select outlets. Drag=Pan, Scroll=Zoom, DblClick=Reset.");
    painter.drawText(rulerMargin, 30, QString("Res: %1m | Outlets: %2")
                         .arg(resolution, 0, 'f', 1)
                         .arg(manualOutletCells.size()));

    painter.end();
    // --- Painting Ends --- 

    return img;
}

QMap<QPoint, double> SimulationEngine::getPerOutletDrainage() const
{
    return perOutletDrainage;
}

QVector<QPoint> SimulationEngine::getAutomaticOutletCells() const
{
    QVector<QPoint> result;
    
    // Convert 1D indices back to 2D coordinates
    for (int i = 0; i < (int)outletCells.size(); i++)
    {
        int idx = outletCells[i];
        int row = idx / ny;  // row
        int col = idx % ny;  // column
        
        if (row >= 0 && row < nx && col >= 0 && col < ny)
        {
            result.append(QPoint(row, col));
        }
    }
    
    qDebug() << "Returning" << result.size() << "automatic outlet cells";
    return result;
}

void SimulationEngine::routeWaterToOutlets()
{
    if (dem.empty() || h.empty())
    {
        qDebug() << "No DEM or water depth data available for drainage calculation";
        return;
    }

    double cellArea = resolution * resolution;
    double outflowVolume = 0.0;
    
    // Process outlet cells only
    for (int i = 0; i < (int)outletCells.size(); i++)
    {
        int outlet_idx = outletCells[i];
        int row = outlet_idx / ny;
        int col = outlet_idx % ny;
        
        // Skip invalid cells
        if (row < 0 || row >= nx || col < 0 || col >= ny)
        {
            continue;
        }
        
        // Log outlet cell water depth periodically
        if (int(time) % 5 == 0) {
            qDebug() << "Outlet cell (" << row << "," << col << ") depth:" << h[outlet_idx]
                     << ", min_depth:" << min_depth;
        }
                        
        // Skip cells with no water
        if (h[outlet_idx] <= min_depth)
        {
            continue;
        }
            
        // Calculate outlet flow - use a higher coefficient for outlets to encourage drainage
        double outletDepth = h[outlet_idx];
        double drainFactor = 5.0; // Increased drainage rate at outlets from 3.0 to 5.0
        
        // Use simplified outlet flow calculation: portion of water leaves each time step
        double outletVolume = outletDepth * cellArea * 0.7 * drainFactor; // Increase from 0.5 to 0.7
        h[outlet_idx] -= outletVolume / cellArea;
        
        // Ensure we don't go negative
        if (h[outlet_idx] < 0.0)
        {
            outletVolume += h[outlet_idx] * cellArea;
            h[outlet_idx] = 0.0;
        }
        
        // Log drainage volumes when they occur
        qDebug() << "Draining at outlet (" << row << "," << col << "): " << outletVolume 
                 << " m³, remaining depth: " << h[outlet_idx];
        
        // Track per-outlet drainage
        QPoint outletPoint(row, col);
        perOutletDrainage[outletPoint] += outletVolume;
        outflowVolume += outletVolume;
        
        // Make sure outlet is in active cells list for next iteration
        if (h[outlet_idx] > min_depth)
        {
            isActive[outlet_idx] = 1;
            if (std::find(nextActiveCells.begin(), nextActiveCells.end(), outlet_idx) == nextActiveCells.end()) {
                nextActiveCells.push_back(outlet_idx);
            }
        }
    }
    
    // Update total drainage
    drainageVolume += outflowVolume;
    
    if (outflowVolume > 0) {
        qDebug() << "Total drainage this step: " << outflowVolume << " m³, cumulative: " << drainageVolume << " m³";
    }
    
    // Record drainage time series data
    drainageTimeSeries.append(qMakePair(time, drainageVolume));
}

QImage SimulationEngine::getFlowAccumulationImage() const
{
    if (nx <= 0 || ny <= 0 || flowAccumulationGrid.empty())
    {
        return QImage();
    }

    // Create an image with dimensions matching the DEM grid
    QImage img(ny, nx, QImage::Format_RGB32);
    img.fill(Qt::white);

    // Find the max flow accumulation value for scaling
    double maxFlow = 0.0;
    for (int i = 0; i < nx; i++) 
    {
        for (int j = 0; j < ny; j++) 
        {
            int k = idx(i, j);
            if (dem[k] > -999998.0) 
            {
                maxFlow = std::max<double>(maxFlow, flowAccumulationGrid[k]);
            }
        }
    }
    
    // Use log scale for better visualization of flow accumulation
    maxFlow = std::log(maxFlow + 1.0);
    
    // Create a color gradient to visualize flow paths
    for (int i = 0; i < nx; i++) 
    {
        for (int j = 0; j < ny; j++) 
        {
            int k = idx(i, j);
            if (dem[k] <= -999998.0) 
            {
                // No-data cells are light gray
                img.setPixel(j, i, qRgb(200, 200, 200));
                continue;
            }
            
            double flowValue = flowAccumulationGrid[k];
            
            // Use log scale to better visualize the full range of values
            double normalizedFlow = (flowValue > 0) ? 
                std::log(flowValue + 1.0) / maxFlow : 0.0;
            
            // Create a blue gradient for flow paths
            int blue = static_cast<int>(255 * normalizedFlow);
            int green = static_cast<int>(150 * normalizedFlow);
            int red = 50;
            
            // Add terrain coloring for context
            if (normalizedFlow < 0.2) 
            {
                // For low flow areas, use terrain coloring (green to brown gradient)
                double normalizedElev = (dem[k] - dem[idx(outletRow,j)]) / 10.0;
                // Force both arguments to be double to avoid ambiguity
                normalizedElev = std::max(0.0, std::min(1.0, normalizedElev));
                
                red = static_cast<int>(155 + 100 * normalizedElev);
                green = static_cast<int>(200 - 60 * normalizedElev);
                blue = static_cast<int>(50 + 40 * normalizedElev);
            }
            
            // Mark outlet cells with a distinctive color
            bool isOutlet = false;
            for (int c = 0; c < (int)outletCells.size(); c++) 
            {
                int cell_idx = outletCells[c];
                int oi = cell_idx / ny;
                int oj = cell_idx % ny;
                if (i == oi && j == oj) 
                {
                    isOutlet = true;
                    break;
                }
            }
            
            if (isOutlet) 
            {
                red = 255;
                green = 50;
                blue = 50;
            }
            
            img.setPixel(j, i, qRgb(red, green, blue));
        }
    }

    // Draw gridlines similar to the DEM preview
    if (showGrid) 
    {
        QPainter painter(&img);
        
        // Use lighter, more subtle grid lines
        painter.setPen(QPen(QColor(0, 0, 0, 40), 0.5));
        
        // Adjust grid interval based on resolution
        int interval = gridInterval;
        if (resolution > 5.0) 
        {
            interval = static_cast<int>(std::max(1.0, static_cast<double>(gridInterval) / 2.0));
        }
        
        // Draw horizontal gridlines
        for (int i = 0; i <= nx; i += interval) 
        {
            painter.drawLine(0, i, ny, i);
        }
        
        // Draw vertical gridlines
        for (int j = 0; j <= ny; j += interval) 
        {
            painter.drawLine(j, 0, j, nx);
        }
    }
    
    // Add a legend
    QPainter painter(&img);
    int legendWidth = 30;
    int legendHeight = img.height() / 3;
    int legendX = img.width() - legendWidth - 10;
    int legendY = 10;
    
    // Draw flow accumulation color bar
    for (int y = 0; y < legendHeight; y++) 
    {
        double normalizedFlow = 1.0 - static_cast<double>(y) / legendHeight; // 1 at top, 0 at bottom
        
        int blue = static_cast<int>(255 * normalizedFlow);
        int green = static_cast<int>(150 * normalizedFlow);
        int red = 50;
        
        painter.setPen(QColor(red, green, blue));
        painter.drawLine(legendX, legendY + y, legendX + legendWidth, legendY + y);
    }
    
    // Add legend labels
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    
    painter.setPen(Qt::black);
    painter.drawText(legendX - 5, legendY - 5, "Flow Paths");
    painter.drawText(legendX + legendWidth + 2, legendY + 15, "High");
    painter.drawText(legendX + legendWidth + 2, legendY + legendHeight - 5, "Low");
    
    // Add a title
    font.setPointSize(12);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(10, 20, "Flow Accumulation Paths");
    
    return img;
}

void SimulationEngine::computeDefaultAutomaticOutletCells()
{
    computeOutletCellsByPercentile(outletPercentile);
}

// Get the index of the lowest elevation neighbor
int SimulationEngine::getLowestNeighborIdx(int i, int j) const
{
    const int centerIdx = idx(i, j);
    double lowestElevation = std::numeric_limits<double>::infinity();
    int lowestIdx = -1;
    double centerElevation = dem[centerIdx] + h[centerIdx];
    
    // Check all 4 neighbors using precomputed neighbors
    for (int n = 0; n < 4; n++) {
        // Get neighbor index directly from precomputed array
        int nk = neighbors[centerIdx][n];
        
        // Skip invalid neighbors
        if (nk < 0) continue;
        
        // Calculate neighbor elevation
        double neighborElevation = dem[nk] + h[nk];
        
        // Find the lowest neighbor
        if (neighborElevation < lowestElevation && neighborElevation < centerElevation) {
            lowestElevation = neighborElevation;
            lowestIdx = nk;
        }
    }
    
    return lowestIdx;
}

