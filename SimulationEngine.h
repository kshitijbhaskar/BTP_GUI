#ifndef SIMULATIONENGINE_H
#define SIMULATIONENGINE_H

#include <QObject>
#include <QImage>
#include <vector>
#include <QString>
#include <QPoint>
#include <QVector>
#include <QPair>
#include <QMap>

// Define operator< for QPoint to use with QMap
// This is needed because QPoint doesn't have a built-in operator<
inline bool operator<(const QPoint& a, const QPoint& b) {
    if (a.x() != b.x())
        return a.x() < b.x();
    return a.y() < b.y();
}

class SimulationEngine : public QObject
{
    Q_OBJECT

public:
    explicit SimulationEngine(QObject *parent = nullptr);

    // Load DEM data from CSV file.
    // The CSV must contain only elevation values arranged as rows.
    bool loadDEM(const QString &filename);

    // Parameter setters
    void setRainfall(double rate);
    void setManningCoefficient(double coefficient);
    void setInfiltrationRate(double rate);
    void setMinWaterDepth(double depth);
    void setCellResolution(double res);
    void setTotalTime(double time);
    
    // Time-varying rainfall methods
    void setTimeVaryingRainfall(bool enabled);
    bool isTimeVaryingRainfall() const { return useTimeVaryingRainfall; }
    void setRainfallSchedule(const QVector<QPair<double, double>>& schedule);
    QVector<QPair<double, double>> getRainfallSchedule() const { return rainfallSchedule; }
    double getCurrentRainfallRate() const;

    // Outlet configuration methods
    void configureOutletsByPercentile(double percentile);
    void setManualOutletCells(const QVector<QPoint> &cells);

    // Initialize simulation (reset time, water depth grid, etc.)
    void initSimulation();

    // Perform one simulation step
    void stepSimulation();

    // Return the current simulation time in seconds
    double getCurrentTime() const { return time; }

    // Total simulation time
    double getTotalTime() const { return totalTime; }

    // Return the total drainage volume computed
    double getTotalDrainage() const;

    // Return the drainage time series data (time, volume)
    QVector<QPair<double, double>> getDrainageTimeSeries() const;
    
    // Return the per-outlet drainage data
    QMap<QPoint, double> getPerOutletDrainage() const;

    // Create and return a QImage showing the water depth grid.
    QImage getWaterDepthImage() const;
    
    // Create and return a DEM preview image for outlet selection
    QImage getDEMPreviewImage() const;

    // New: Create and return a flow accumulation visualization image
    QImage getFlowAccumulationImage() const;

    // Get grid dimensions
    int getGridWidth() const { return ny; }
    int getGridHeight() const { return nx; }
    double getCellResolution() const { return resolution; }
    
    // Grid and rulers display settings
    void setShowGrid(bool show) { showGrid = show; }
    bool getShowGrid() const { return showGrid; }
    void setShowRulers(bool show) { showRulers = show; }
    bool getShowRulers() const { return showRulers; }
    void setGridInterval(int interval) { gridInterval = interval; }
    int getGridInterval() const { return gridInterval; }
    
    // Get manual outlet cells for UI display
    QVector<QPoint> getManualOutletCells() const { return manualOutletCells; }
    
    // New method to get automatic outlet cells for display purposes
    QVector<QPoint> getAutomaticOutletCells() const;

private:
    // Grid dimensions
    int nx, ny;
    double resolution;  // meters per cell

    // DEM and water depth grids
    std::vector<std::vector<double>> dem;
    std::vector<std::vector<double>> h;  // water depth (m)

    // Flow accumulation grid for visualization
    std::vector<std::vector<double>> flowAccumulationGrid;

    // Simulation parameters
    double n_manning;    // Manning's roughness coefficient
    double Ks;           // infiltration rate (m/s)
    double min_depth;    // minimum water depth threshold
    double totalTime;    // total simulation time in seconds
    double time;         // current simulation time
    double dt;           // time step (s)
    double rainfallRate; // constant rainfall rate (m/s)
    
    // Time-varying rainfall parameters
    bool useTimeVaryingRainfall; // Whether to use time-varying rainfall
    QVector<QPair<double, double>> rainfallSchedule; // (time, rainfall rate) pairs

    // Outlet-related variables
    // Outlet cells along bottom row (index j values) based on lowest 10% of elevations
    std::vector<int> outletCells;
    int outletRow;
    bool useManualOutlets;
    QVector<QPoint> manualOutletCells;

    // Total drainage volume (m^3) over time (accumulated discharge)
    double drainageVolume;
    
    // Store drainage volume per outlet cell
    QMap<QPoint, double> perOutletDrainage;
    
    // Store time series data (time, drainage volume)
    QVector<QPair<double, double>> drainageTimeSeries;
    
    // Display options
    bool showGrid = true;
    bool showRulers = false; // Default to rulers disabled initially
    int gridInterval = 10;

    // Helper functions
    void computeDefaultAutomaticOutletCells();
    void computeOutletCellsByPercentile(double percentile);
    void routeWaterToOutlets();
};

#endif // SIMULATIONENGINE_H
