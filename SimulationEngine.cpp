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
    resolution(0.25), // default cell resolution, will be overwritten by GeoTIFF
    n_manning(0.03),
    Ks(1e-6),
    min_depth(1e-5),
    totalTime(1800.0),
    time(0.0),
    dt(1.0),
    rainfallRate(0.0),
    useTimeVaryingRainfall(false),
    outletRow(0),
    useManualOutlets(false),
    outletPercentile(0.1), // Default to 10%
    drainageVolume(0.0),
    showGrid(true),
    gridInterval(10)
{
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
    if (nx <= 0 || ny <= 0) {
        qDebug() << "ERROR: Cannot initialize simulation - invalid grid dimensions:" << nx << "x" << ny;
        return false;
    }
    
    // Check if we have any valid outlet cells
    if (outletCells.empty()) {
        qDebug() << "WARNING: No outlet cells defined. Attempting to compute default outlets.";
        computeDefaultAutomaticOutletCells();
        
        // Check again after attempting to compute default outlets
        if (outletCells.empty()) {
            qDebug() << "ERROR: Still no outlet cells after computing defaults. Cannot initialize simulation.";
            return false;
        }
    }
    
    // Validate simulation parameters
    if (resolution <= 0) {
        qDebug() << "ERROR: Invalid cell resolution:" << resolution;
        return false;
    }
    
    if (n_manning <= 0) {
        qDebug() << "ERROR: Invalid Manning coefficient:" << n_manning;
        return false;
    }
    
    if (totalTime <= 0) {
        qDebug() << "ERROR: Invalid total simulation time:" << totalTime;
        return false;
    }
    
    // Reset simulation time and water depth grid
    time = 0.0;
    dt = 1.0;
    drainageVolume = 0.0;
    
    // Initialize water depth grid
    try {
        h.assign(nx * ny, 0.0);
    } 
    catch (const std::exception& e) {
        qDebug() << "ERROR: Failed to initialize water depth grid:" << e.what();
        return false;
    }
    
    // Clear previous time series data
    drainageTimeSeries.clear();
    
    // Clear per-outlet drainage data
    perOutletDrainage.clear();
    
    // If using time-varying rainfall but no schedule is provided, add default entry
    if (useTimeVaryingRainfall && rainfallSchedule.isEmpty()) {
        rainfallSchedule.append(qMakePair(0.0, rainfallRate));
    }
    
    // Make sure we have outlet cells before initialization
    if (!useManualOutlets) {
        computeDefaultAutomaticOutletCells();
    }
    
    // Initialize per-outlet drainage tracking for all outlet cells
    qDebug() << "Initializing drainage tracking for outlets";
    
    perOutletDrainage.clear();  // Ensure we start with a clean state
    
    if (useManualOutlets) {
        qDebug() << "Using" << manualOutletCells.size() << "manual outlet cells";
        for (const QPoint &p : manualOutletCells) {
            perOutletDrainage[p] = 0.0;
            qDebug() << "  Added outlet at:" << p.x() << p.y();
        }
    } else {
        qDebug() << "Using" << outletCells.size() << "automatic outlet cells";
        QVector<QPoint> autoOutlets = getAutomaticOutletCells();
        for (const QPoint &p : autoOutlets) {
            perOutletDrainage[p] = 0.0;
            qDebug() << "  Added automatic outlet at:" << p.x() << p.y();
        }
    }
    
    // Add initial data point (time=0, drainage=0)
    drainageTimeSeries.append(qMakePair(0.0, 0.0));
    
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
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            // Check if the cell is on the boundary
            if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1) {
                // Check if it's a valid DEM cell (not no-data)
                if (dem[idx(i,j)] > -999998.0) {
                    int index1D = i * ny + j;
                    boundaryCells.push_back({dem[idx(i,j)], index1D});
                }
            }
        }
    }

    if (boundaryCells.empty()) {
        qDebug() << "No valid boundary cells found for automatic outlet selection.";
        // As a fallback, maybe select the globally lowest cell?
        double minElev = std::numeric_limits<double>::max();
        int minIdx = -1;
        for(int i=0; i<nx; ++i) {
            for(int j=0; j<ny; ++j) {
                if (dem[idx(i,j)] > -999998.0 && dem[idx(i,j)] < minElev) {
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

    // Determine the number of outlets based on percentile
    int numOutlets = std::max(1, (int)(percentile * boundaryCells.size()));
    // Ensure at least one outlet, but cap at a reasonable number (e.g., 10% of boundary, or 50)
    numOutlets = std::min({numOutlets, (int)(boundaryCells.size() * 0.1), 50}); 
    numOutlets = std::max(1, numOutlets); // Ensure at least one

    qDebug() << "Selecting top" << numOutlets << "lowest boundary cells as automatic outlets.";

    // Add the lowest 'numOutlets' cells to outletCells
    for (int k = 0; k < numOutlets; ++k) {
        outletCells.push_back(boundaryCells[k].second);
        int i = boundaryCells[k].second / ny;
        int j = boundaryCells[k].second % ny;
        qDebug() << "Added automatic outlet at:" << i << j << "with elevation:" << boundaryCells[k].first;
    }

    // Optional: Ensure some minimum spacing between outlets if they are too clustered?
    // (Could be added later if needed)

    qDebug() << "Selected" << outletCells.size() << "automatic outlet cells along boundary.";
}

void SimulationEngine::setTimeVaryingRainfall(bool enabled)
{
    useTimeVaryingRainfall = enabled;
}

void SimulationEngine::setRainfallSchedule(const QVector<QPair<double, double>>& schedule)
{
    // Clear existing schedule
    rainfallSchedule.clear();
    
    // Copy and sort the schedule by timestamp
    rainfallSchedule = schedule;
    std::sort(rainfallSchedule.begin(), rainfallSchedule.end(), 
              [](const QPair<double, double>& a, const QPair<double, double>& b) {
                  return a.first < b.first;
              });
    
    // Ensure first entry is at time 0
    if (!rainfallSchedule.isEmpty() && rainfallSchedule.first().first > 0) {
        rainfallSchedule.prepend(qMakePair(0.0, rainfallSchedule.first().second));
    }
    
    // If schedule is empty, add a single entry with the current constant rate
    if (rainfallSchedule.isEmpty()) {
        rainfallSchedule.append(qMakePair(0.0, rainfallRate));
    }
}

double SimulationEngine::getCurrentRainfallRate() const
{
    if (!useTimeVaryingRainfall || rainfallSchedule.isEmpty()) {
        return rainfallRate; // Fall back to constant rate
    }
    
    // Find the applicable rainfall rate for the current time
    double currentRate = rainfallSchedule.first().second; // Default to first rate
    
    for (int i = 0; i < rainfallSchedule.size(); i++) {
        const QPair<double, double>& entry = rainfallSchedule[i];
        
        // If this entry's time is in the future, use the previous entry's rate
        if (entry.first > time) {
            break;
        }
        
        // Update current rate
        currentRate = entry.second;
    }
    
    return currentRate;
}

void SimulationEngine::stepSimulation()
{
    if (nx <= 0 || ny <= 0)
        return;

    // Get the rainfall rate to use (either constant or time-varying)
    double currentRainfallRate = useTimeVaryingRainfall ? getCurrentRainfallRate() : rainfallRate;
    
    // Debug - print simulation state at regular intervals, but not inside inner loops
    if (int(time) % 10 == 0) {
        qDebug() << ">>> Simulation Time:" << time << "s";
        qDebug() << "    Current Rainfall Rate:" << currentRainfallRate << "m/s";
        qDebug() << "    Total Drainage So Far:" << drainageVolume << "m³";
    }

    // Apply rainfall and infiltration to each cell
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] <= -999998.0) {
                h[k] = 0.0; 
                continue;
            }
            double delta = (currentRainfallRate - Ks) * dt;
            h[k] += delta;
            if (h[k] < 0.0) h[k] = 0.0;
        }
    }

    // Calculate total system water *after* rainfall/infiltration
    double totalSystemWater = 0.0;
    double cellArea = resolution * resolution;
    #pragma omp parallel for reduction(+:totalSystemWater) schedule(static)
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] > -999998.0) {
                totalSystemWater += h[k] * cellArea;
            }
        }
    }

    // --- Refactored Flux Calculation and Water Depth Update --- 
    std::vector<std::array<double, 4>> Q_out(nx * ny, {0.0, 0.0, 0.0, 0.0});
    std::vector<double> Q_total_out(nx * ny, 0.0);

    int di[4] = {-1, 0, 1, 0}; // N, E, S, W
    int dj[4] = {0, 1, 0, -1};

    // First pass: Calculate potential outflow Q_out
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] <= -999998.0) continue; 
            double h_i = h[k];
            if (h_i < min_depth) continue;
            double H_i = h_i + dem[k];

            for (int d = 0; d < 4; d++) { 
                int ni = i + di[d];
                int nj = j + dj[d];
                if (ni < 0 || ni >= nx || nj < 0 || nj >= ny) continue;
                int nk = idx(ni, nj);
                if (dem[nk] <= -999998.0) continue;
                double h_j = h[nk];
                double H_j = h_j + dem[nk];
                double deltaH = H_i - H_j;

                if (deltaH > 0) {
                    double S = deltaH / resolution;
                    double A = h_i * resolution; 
                    double R = h_i;             
                    double Q = (A * std::pow(R, 2.0/3.0) * std::sqrt(S)) / n_manning;
                    Q_out[k][d] = Q;
                    Q_total_out[k] += Q;
                }
            }
        }
    }

    // Second pass: Calculate delta_h using mass conservation scaling
    std::vector<double> delta_h(nx * ny, 0.0);

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] <= -999998.0) continue;

            double V_t = h[k] * cellArea; 
            double c = 1.0;                 

            if (Q_total_out[k] * dt > V_t && Q_total_out[k] > 0) {
                c = V_t / (Q_total_out[k] * dt);
            }

            double netFluxVolume = 0.0;
            // Scaled Outflows FROM cell (i,j)
            netFluxVolume -= Q_total_out[k] * c * dt; // Apply dt here

            // Scaled Inflows TO cell (i,j) FROM neighbors
            for (int d = 0; d < 4; d++) {
                int ni = i - di[d]; // Neighbor index (source of flow)
                int nj = j - dj[d];
                if (ni < 0 || ni >= nx || nj < 0 || nj >= ny) continue;
                
                int nk = idx(ni, nj);
                if (dem[nk] <= -999998.0) continue;
                
                int flow_direction_from_neighbor = (d + 2) % 4; // Flow direction index from neighbor's perspective

                double V_neighbor = h[nk] * cellArea;
                double c_neighbor = 1.0;
                if (Q_total_out[nk] * dt > V_neighbor && Q_total_out[nk] > 0) {
                    c_neighbor = V_neighbor / (Q_total_out[nk] * dt);
                }
                netFluxVolume += Q_out[nk][flow_direction_from_neighbor] * c_neighbor * dt; // Apply dt here
            }
            delta_h[k] = netFluxVolume / cellArea;
        }
    }

    // Third pass: Update water depths
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] <= -999998.0) continue;
            h[k] += delta_h[k];
            if (h[k] < 0.0) h[k] = 0.0;
        }
    }
    // --- End of Refactored Section ---

    // Route water TO outlets (potentially tune down later)
    routeWaterToOutlets(); 

    // Compute actual drainage FROM outlet cells 
    double outflow = 0.0;
    double totalWaterOnOutlets = 0.0;
    // Calculate adaptive drainage factor (less aggressive)
    double systemWaterThreshold = 1.0; 
    double drainageFactor = 1.0; 
    if (totalSystemWater > systemWaterThreshold) {
        drainageFactor = 1.0 + std::min(2.0, (totalSystemWater - systemWaterThreshold) / 10.0); 
    }
    double timeProgress = std::min(1.0, time / 120.0); 
    double timeFactor = 0.7 + 0.3 * timeProgress; 
    drainageFactor *= timeFactor;
    
    for (const auto& idx : outletCells) {
        int i = idx / ny; int j = idx % ny;
        int k = idx(i, j);
        if (i >= 0 && i < nx && j >= 0 && j < ny && dem[k] > -999998.0) {
            totalWaterOnOutlets += h[k] * cellArea;
            double h_i = h[k];
            
            if (h_i > min_depth) {
                double S = 0.2; 
                double A = h_i * resolution;
                double Q = 2.5 * drainageFactor * (A * std::pow(h_i, 2.0/3.0) * std::sqrt(S)) / n_manning; 
                double vol = Q * dt;
                double availableVolume = h_i * cellArea;
                if (vol > availableVolume * 0.95) vol = availableVolume * 0.95;

                h[k] -= vol / cellArea;
                outflow += vol;
                QPoint outletPoint(i, j);
                perOutletDrainage[outletPoint] += vol;
            }
        }
    }
    
    drainageVolume += outflow;
    drainageTimeSeries.append(qMakePair(time + dt, drainageVolume));
    time += dt; // Use fixed dt for now

    // Emit signals to update UI
    emit simulationTimeUpdated(time, totalTime);
    
    // Update UI only every 5 steps to reduce overhead
    static int stepCount = 0;
    if (++stepCount % 5 == 0) {
        emit simulationStepCompleted(getWaterDepthImage());
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
        return QImage();

    // Pre-allocate buffer if needed (mutable member)
    if (waterImg.size() != QSize(ny, nx)) {
        waterImg = QImage(ny, nx, QImage::Format_RGB32);
    }

    // Find the max water depth for scaling
    double maxDepth = 1e-9; // Ensure non-zero
    for (int k = 0; k < nx * ny; k++) {
        if (dem[k] > -999998.0) {
            maxDepth = std::max(maxDepth, h[k]);
        }
    }

    // Create a color gradient from white to blue using scanLine for better performance
    for (int i = 0; i < nx; i++) {
        QRgb* line = reinterpret_cast<QRgb*>(waterImg.scanLine(i));
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] <= -999998.0) {
                // No-data cells are light gray
                line[j] = qRgb(200, 200, 200);
                continue;
            }
            
            double depth = h[k];
            // Normalize depth to 0-1 range
            double normalizedDepth = depth / maxDepth;
            
            // Create a color gradient: white (no water) to blue (deep water)
            int blue = 255;
            int red = int(255 * (1.0 - normalizedDepth));
            int green = int(255 * (1.0 - normalizedDepth));
            
            line[j] = qRgb(red, green, blue);
        }
    }

    // Draw gridlines to help identify cell positions if enabled
    if (showGrid) {
        QPainter painter(&waterImg);
        
        // Use lighter, more subtle grid lines
        painter.setPen(QPen(QColor(0, 0, 0, 40), 0.5)); // Very light black, mostly transparent, thin
        
        // Adjust grid interval based on resolution
        int interval = gridInterval;
        if (resolution > 5.0) {
            interval = std::max(1, gridInterval / 2);
        }
        
        // Draw horizontal gridlines
        for (int i = 0; i <= nx; i += interval) {
            painter.drawLine(0, i, ny, i);
        }
        
        // Draw vertical gridlines
        for (int j = 0; j <= ny; j += interval) {
            painter.drawLine(j, 0, j, nx);
        }
        
        // Draw rulers/coordinates if enabled
        if (showRulers) {
            painter.setPen(Qt::black);
            
            // Calculate appropriate ruler interval based on image size and resolution
            int rulerInterval = gridInterval * 2; // Base value
            if (resolution > 5.0) {
                rulerInterval = std::max(1, gridInterval);
            }
            
            // Adjust font size based on resolution
            QFont rulerFont = painter.font();
            if (resolution > 5.0) {
                rulerFont.setPointSize(7);
            } else {
                rulerFont.setPointSize(9);
            }
            painter.setFont(rulerFont);
            
            // Draw coordinate labels on horizontal ruler
            for (int i = 0; i < nx; i += rulerInterval) {
                painter.drawText(2, i + 12, QString::number(i));
            }
            
            // Draw coordinate labels on vertical ruler
            for (int j = 0; j < ny; j += rulerInterval) {
                painter.drawText(j + 2, 12, QString::number(j));
            }
        }
    }
    
    return waterImg;
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
        if (resolution > 5.0) rulerInterval = std::max(1, gridInterval);
        else if (resolution < 1.0) rulerInterval = gridInterval * 3;
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
    for (const auto& idx : outletCells) {
        int i = idx / ny;  // row
        int j = idx % ny;  // column
        
        if (i >= 0 && i < nx && j >= 0 && j < ny) {
            result.append(QPoint(i, j));
        }
    }
    
    qDebug() << "Returning" << result.size() << "automatic outlet cells";
    return result;
}

void SimulationEngine::routeWaterToOutlets()
{
    if (outletCells.empty() || nx <= 0 || ny <= 0)
        return;
        
    qDebug() << "Routing water to" << outletCells.size() << "outlet cells";
    
    // Create a flow accumulation grid to identify main drainage paths
    std::vector<double> flowAccumulation(nx * ny, 0.0);
    
    // Depression filling - identify and fill local depressions to ensure flow continuity
    std::vector<double> filledDEM = dem; // Copy original DEM
    bool depressionsFilled = false;
    int fillIterations = 0;
    const int MAX_FILL_ITERATIONS = 3; // Limit iterations to avoid excessive processing
    
    while (!depressionsFilled && fillIterations < MAX_FILL_ITERATIONS) {
        depressionsFilled = true;
        
        for (int i = 1; i < nx - 1; i++) {
            for (int j = 1; j < ny - 1; j++) {
                int k = idx(i, j);
                // Skip no-data cells
                if (filledDEM[k] <= -999998.0) {
                    continue;
                }
                
                // Check if this is a depression (all neighbors are higher)
                bool isDepression = true;
                double lowestNeighbor = std::numeric_limits<double>::max();
                
                // Check 8 neighbors
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue; // Skip self
                        
                        int ni = i + di;
                        int nj = j + dj;
                        
                        // Skip out of bounds or no-data cells
                        if (ni < 0 || ni >= nx || nj < 0 || nj >= ny) {
                            continue;
                        }
                        
                        int nk = idx(ni, nj);
                        if (filledDEM[nk] <= -999998.0) {
                            continue;
                        }
                        
                        if (filledDEM[nk] < filledDEM[k]) {
                            isDepression = false;
                            break;
                        }
                        
                        lowestNeighbor = std::min(lowestNeighbor, filledDEM[nk]);
                    }
                }
                
                // Fill depression by setting elevation slightly below lowest neighbor
                if (isDepression && lowestNeighbor < std::numeric_limits<double>::max()) {
                    double oldElev = filledDEM[k];
                    filledDEM[k] = lowestNeighbor - 0.01; // Set slightly below lowest neighbor
                    depressionsFilled = false; // Need another iteration
                    
                    if (fillIterations == 0) { // Only log on first iteration to avoid spam
                        qDebug() << "Filled depression at" << i << j << "from" << oldElev << "to" << filledDEM[k];
                    }
                }
            }
        }
        
        fillIterations++;
    }
    
    qDebug() << "Depression filling completed in" << fillIterations << "iterations";
    
    // Calculate flow directions and accumulation based on the filled DEM
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            // Skip no-data cells
            if (filledDEM[k] <= -999998.0) {
                continue;
            }
            
            // Find the steepest downhill neighbor
            int di[8] = {-1, -1, 0, 1, 1, 1, 0, -1}; // Include diagonals
            int dj[8] = {0, 1, 1, 1, 0, -1, -1, -1};
            double maxSlope = 0.0;
            int flowDir = -1;
            
            for (int d = 0; d < 8; d++) {
                int ni = i + di[d];
                int nj = j + dj[d];
                
                // Skip if out of bounds or no-data cell
                if (ni < 0 || ni >= nx || nj < 0 || nj >= ny) {
                    continue;
                }
                
                int nk = idx(ni, nj);
                if (filledDEM[nk] <= -999998.0) {
                    continue;
                }
                
                // Calculate elevation difference and slope
                double dElev = filledDEM[k] - filledDEM[nk];
                double distance = (d % 2 == 0) ? resolution : resolution * 1.414; // Adjust for diagonals
                double slope = dElev / distance;
                
                // If this slope is steeper, update flow direction
                if (slope > maxSlope) {
                    maxSlope = slope;
                    flowDir = d;
                }
            }
            
            // If we found a downhill direction, add to flow accumulation
            if (flowDir >= 0) {
                int ni = i + di[flowDir];
                int nj = j + dj[flowDir];
                int nk = idx(ni, nj);
                flowAccumulation[nk] += 1.0 + flowAccumulation[k];
            }
        }
    }
    
    // Retain a copy of flow accumulation for visualization
    flowAccumulationGrid = flowAccumulation;
    
    // For each outlet cell, create a path of increased water depth leading to it
    for (const auto& idx : outletCells) {
        int i = idx / ny;  // row
        int j = idx % ny;  // column
        
        // Make sure coordinates are valid
        if (i < 0 || i >= nx || j < 0 || j >= ny || dem[idx(i,j)] <= -999998.0)
            continue;
            
        // Find multiple paths from the outlet toward higher accumulation areas
        // This creates a more realistic drainage network
        std::vector<std::pair<int, int>> paths;
        int maxPaths = 3; // Try to find up to 3 distinct drainage paths
        
        // Start with the outlet cell
        paths.push_back(std::make_pair(i, j));
        
        // Create multiple flow paths to ensure comprehensive drainage
        for (int pathId = 0; pathId < maxPaths; pathId++) {
            int currentI = i;
            int currentJ = j;
            
            // If we're creating additional paths, offset the starting point
            if (pathId > 0) {
                if (pathId == 1 && j > 1) {
                    currentJ = j - 1;  // Start one cell to the left
                } else if (pathId == 2 && j < ny-2) {
                    currentJ = j + 1;  // Start one cell to the right
                } else {
                    continue; // Skip if we can't create an offset path
                }
            }
            
            // Create a path uphill from the outlet
            // We'll look for cells with higher flow accumulation to find natural channels
            int pathLength = std::min(15, nx/2); // Extend path length for even better coverage
            double pathFactor = 1.0; // Start with full effect at outlet
            
            // Add the initial cell to the path
            if (pathId > 0) {
                paths.push_back(std::make_pair(currentI, currentJ));
            }

            // Keep track of cells already in this path to avoid loops
            std::set<int> pathCellIndices;
            pathCellIndices.insert(idx(currentI, currentJ));

            for (int step = 0; step < pathLength; step++) {
                // Find the best next cell to include in the path (highest flow accumulation)
                int di[8] = {-1, -1, 0, 1, 1, 1, 0, -1}; // Include diagonals
                int dj[8] = {0, 1, 1, 1, 0, -1, -1, -1};
                
                double bestScore = -1.0;
                int nextI = -1, nextJ = -1;
                
                // Prioritize movement uphill and toward high flow accumulation
                for (int k = 0; k < 8; k++) {
                    int ni = currentI + di[k];
                    int nj = currentJ + dj[k];
                    
                    // Skip if out of bounds or no-data cell
                    if (ni < 0 || ni >= nx || nj < 0 || nj >= ny) {
                        continue;
                    }
                    
                    int cellIndex = idx(ni, nj);
                    if (dem[cellIndex] <= -999998.0) {
                        continue;
                    }
                    
                    // Skip cells already in this path to avoid loops
                    if (pathCellIndices.find(cellIndex) != pathCellIndices.end()) {
                        continue;
                    }
                    
                    // Calculate a score based on flow accumulation and elevation
                    // Prefer cells that: 1) have high flow accumulation, 2) are uphill
                    double flowScore = flowAccumulation[cellIndex] * 0.7; // Increased weight for flow accumulation
                    double elevScore = std::max(0.0, dem[cellIndex] - dem[idx]) * 1.5;
                    double upstreamScore = (ni < i) ? 2.5 : 0.0; // Prefer upstream cells
                    
                    // New: Add a penalty for flat areas to help guide water through depressions
                    double flatPenalty = 0.0;
                    if (std::abs(dem[cellIndex] - dem[idx]) < 0.01) {
                        flatPenalty = -1.0; // Penalty for flat areas
                    }
                    
                    double score = flowScore + elevScore + upstreamScore + flatPenalty;
                    
                    // Check if this is the best option so far
                    if (score > bestScore) {
                        bestScore = score;
                        nextI = ni;
                        nextJ = nj;
                    }
                }
                
                // If we couldn't find a next cell, stop this path
                if (nextI < 0) break;
                
                // Add this cell to the path
                currentI = nextI;
                currentJ = nextJ;
                paths.push_back(std::make_pair(currentI, currentJ));
                pathCellIndices.insert(idx(currentI, currentJ));
            }
        }
        
        // Debug path information
        qDebug() << "Created" << paths.size() << "path segments for outlet at" << i << j;
    }
}

QImage SimulationEngine::getFlowAccumulationImage() const
{
    if (nx <= 0 || ny <= 0 || flowAccumulationGrid.empty())
        return QImage();

    // Create an image with dimensions matching the DEM grid
    QImage img(ny, nx, QImage::Format_RGB32);
    img.fill(Qt::white);

    // Find the max flow accumulation value for scaling
    double maxFlow = 0.0;
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] > -999998.0) {
                maxFlow = std::max(maxFlow, flowAccumulationGrid[k]);
            }
        }
    }
    
    // Use log scale for better visualization of flow accumulation
    maxFlow = std::log(maxFlow + 1.0);
    
    // Create a color gradient to visualize flow paths
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            int k = idx(i, j);
            if (dem[k] <= -999998.0) {
                // No-data cells are light gray
                img.setPixel(j, i, qRgb(200, 200, 200));
                continue;
            }
            
            double flowValue = flowAccumulationGrid[k];
            
            // Use log scale to better visualize the full range of values
            double normalizedFlow = (flowValue > 0) ? 
                std::log(flowValue + 1.0) / maxFlow : 0.0;
            
            // Create a blue gradient for flow paths
            int blue = int(255 * normalizedFlow);
            int green = int(150 * normalizedFlow);
            int red = 50;
            
            // Add terrain coloring for context
            if (normalizedFlow < 0.2) {
                // For low flow areas, use terrain coloring (green to brown gradient)
                double normalizedElev = (dem[k] - dem[idx(outletRow,j)]) / 10.0;
                normalizedElev = std::max(0.0, std::min(1.0, normalizedElev));
                
                red = int(155 + 100 * normalizedElev);
                green = int(200 - 60 * normalizedElev);
                blue = int(50 + 40 * normalizedElev);
            }
            
            // Mark outlet cells with a distinctive color
            bool isOutlet = false;
            for (const auto& idx : outletCells) {
                int oi = idx / ny;
                int oj = idx % ny;
                if (i == oi && j == oj) {
                    isOutlet = true;
                    break;
                }
            }
            
            if (isOutlet) {
                red = 255;
                green = 50;
                blue = 50;
            }
            
            img.setPixel(j, i, qRgb(red, green, blue));
        }
    }

    // Draw gridlines similar to the DEM preview
    if (showGrid) {
        QPainter painter(&img);
        
        // Use lighter, more subtle grid lines
        painter.setPen(QPen(QColor(0, 0, 0, 40), 0.5));
        
        // Adjust grid interval based on resolution
        int interval = gridInterval;
        if (resolution > 5.0) {
            interval = std::max(1, gridInterval / 2);
        }
        
        // Draw horizontal gridlines
        for (int i = 0; i <= nx; i += interval) {
            painter.drawLine(0, i, ny, i);
        }
        
        // Draw vertical gridlines
        for (int j = 0; j <= ny; j += interval) {
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
    for (int y = 0; y < legendHeight; y++) {
        double normalizedFlow = 1.0 - double(y) / legendHeight; // 1 at top, 0 at bottom
        
        int blue = int(255 * normalizedFlow);
        int green = int(150 * normalizedFlow);
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
