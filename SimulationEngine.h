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
// This enables QPoint to be used as a key in QMap for tracking per-outlet drainage
inline bool operator<(const QPoint& a, const QPoint& b) {
    if (a.x() != b.x())
        return a.x() < b.x();
    return a.y() < b.y();
}

/**
 * @brief Core simulation engine for hydrological modeling
 * 
 * Implements a grid-based hydrological simulation that:
 * 1. Processes Digital Elevation Models (DEM)
 * 2. Simulates surface water flow using Manning's equation
 * 3. Handles both constant and time-varying rainfall
 * 4. Supports automatic and manual outlet placement
 * 5. Provides real-time visualization
 * 
 * Key Features:
 * - Mass conservation enforcement
 * - Depression filling for continuous flow
 * - Adaptive time stepping
 * - Multi-outlet drainage tracking
 * - Interactive visualization generation
 */
class SimulationEngine : public QObject
{
    Q_OBJECT

public:
    explicit SimulationEngine(QObject *parent = nullptr);
    ~SimulationEngine() = default;

    /**
     * @brief Loads and validates a DEM file
     * @param filename Path to DEM file (.tif or .csv)
     * @return true if loading succeeded
     */
    bool loadDEM(const QString &filename);

    /**
     * @brief Initializes simulation state
     * @return true if initialization succeeded
     */
    bool initSimulation();

    /**
     * @brief Advances simulation by one time step
     * 
     * Core simulation loop that:
     * 1. Updates rainfall and infiltration
     * 2. Computes water flow between cells
     * 3. Updates drainage volumes
     * 4. Generates visualization data
     */
    void stepSimulation();

    // Getters and setters for simulation parameters
    /**
     * @brief Sets Manning's roughness coefficient
     * @param n Manning's n value (typical range: 0.01-0.1)
     */
    void setManningCoefficient(double n);

    /**
     * @brief Sets soil infiltration rate
     * @param ks Saturated hydraulic conductivity (m/s)
     */
    void setInfiltrationRate(double ks);

    /**
     * @brief Sets constant rainfall rate
     * @param rate Rainfall intensity (m/s)
     */
    void setRainfallRate(double rate);

    /**
     * @brief Sets minimum water depth for simulation
     * @param depth Minimum water depth (m)
     */
    void setMinWaterDepth(double depth);

    /**
     * @brief Sets cell resolution for the grid
     * @param res Cell resolution (m)
     */
    void setCellResolution(double res);

    /**
     * @brief Sets total simulation duration
     * @param time Total simulation time (seconds)
     */
    void setTotalTime(double time);

    /**
     * @brief Configures outlets based on percentile
     * @param percentile Percentile for outlet selection
     */
    void configureOutletsByPercentile(double percentile);

    /**
     * @brief Sets manual outlet cells
     * @param cells Vector of outlet cell coordinates
     */
    void setManualOutletCells(const QVector<QPoint> &cells);

    /**
     * @brief Sets rainfall schedule
     * @param schedule Vector of time-rainfall pairs
     */
    void setRainfallSchedule(const QVector<QPair<double, double>> &schedule);

    /**
     * @brief Enables or disables time-varying rainfall
     * @param enabled True to enable, false to disable
     */
    void setTimeVaryingRainfall(bool enabled);

    /**
     * @brief Gets current simulation time
     * @return Elapsed simulation time (seconds)
     */
    double getCurrentTime() const { return time; }

    /**
     * @brief Gets total simulation duration
     * @return Total simulation time (seconds)
     */
    double getTotalTime() const { return totalTime; }

    /**
     * @brief Gets total drainage volume
     * @return Total drained water volume (m³)
     */
    double getTotalDrainage() const { return drainageVolume; }

    /**
     * @brief Gets drainage time series
     * @return Vector of time-drainage pairs
     */
    QVector<QPair<double, double>> getDrainageTimeSeries() const { return drainageTimeSeries; }

    /**
     * @brief Gets automatic outlet cells
     * @return Vector of automatic outlet cell coordinates
     */
    QVector<QPoint> getAutomaticOutletCells() const;

    /**
     * @brief Gets per-outlet drainage volumes
     * @return Map of outlet coordinates to drainage volumes
     */
    QMap<QPoint, double> getPerOutletDrainage() const { return perOutletDrainage; }

    /**
     * @brief Gets current rainfall rate
     * @return Current rainfall intensity (m/s)
     */
    double getCurrentRainfallRate() const;

    /**
     * @brief Gets water depth visualization image
     * @return Water depth image
     */
    QImage getWaterDepthImage() const;

    /**
     * @brief Gets DEM preview image
     * @return DEM preview image
     */
    QImage getDEMPreviewImage() const;

    /**
     * @brief Gets flow accumulation visualization image
     * @return Flow accumulation image
     */
    QImage getFlowAccumulationImage() const;

signals:
    /**
     * @brief Emitted when simulation time is updated
     * @param currentTime Current simulation time
     * @param totalTime Total simulation time
     */
    void simulationTimeUpdated(double currentTime, double totalTime);

    /**
     * @brief Emitted when a simulation step is completed
     * @param waterDepthImage Updated water depth visualization
     */
    void simulationStepCompleted(const QImage& waterDepthImage);

    /**
     * @brief Emitted when simulation progress changes
     * @param progress Progress percentage (0-100)
     */
    void progressUpdated(int progress);

    /**
     * @brief Emitted when an error occurs
     * @param message Error description
     */
    void errorOccurred(const QString &message);

private:
    // Internal simulation methods
    /**
     * @brief Routes water through preferential flow paths
     * 
     * Implements:
     * 1. Depression filling
     * 2. Flow direction computation
     * 3. Flow accumulation tracking
     * 4. Multi-outlet routing
     */
    void routeWaterToOutlets();

    /**
     * @brief Computes outlet cells based on percentile
     * @param percentile Percentile for outlet selection
     */
    void computeOutletCellsByPercentile(double percentile);

    /**
     * @brief Computes default automatic outlet cells
     */
    void computeDefaultAutomaticOutletCells();

    // Member variables with detailed documentation
    double n_manning;      ///< Manning's roughness coefficient
    double Ks;            ///< Infiltration rate (m/s)
    double min_depth;     ///< Minimum water depth (m)
    double rainfallRate;  ///< Current rainfall intensity (m/s)
    double time;          ///< Current simulation time (s)
    double totalTime;     ///< Total simulation duration (s)
    double dt;            ///< Current time step (s)
    double drainageVolume; ///< Total drainage volume (m³)
    
    // Grid properties
    int nx, ny;           ///< Grid dimensions
    double resolution;    ///< Cell size (m)
    
    // Simulation grids
    std::vector<std::vector<double>> dem; ///< Ground elevation grid (m)
    std::vector<std::vector<double>> h;   ///< Water depth grid (m)
    std::vector<std::vector<double>> flowAccumulationGrid; ///< Flow accumulation grid
    
    // Outlet management
    bool useManualOutlets;             ///< Manual outlet selection flag
    double outletPercentile;           ///< Percentile for auto-outlets
    int outletRow;                     ///< Outlet row index
    std::vector<int> outletCells;      ///< Outlet cell indices
    QVector<QPoint> manualOutletCells; ///< Manual outlet cell coordinates
    
    // Rainfall configuration
    bool useTimeVaryingRainfall;       ///< Time-varying rainfall flag
    QVector<QPair<double, double>> rainfallSchedule; ///< Rainfall schedule
    
    // Drainage tracking
    QVector<QPair<double, double>> drainageTimeSeries; ///< Drainage time series
    QMap<QPoint, double> perOutletDrainage; ///< Per-outlet drainage volumes
    
    // Visualization state
    bool showGrid;                     ///< Grid overlay flag
    bool showRulers;                   ///< Ruler overlay flag
    int gridInterval;                  ///< Grid line spacing
};

#endif // SIMULATIONENGINE_H
