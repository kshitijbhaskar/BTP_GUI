#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QPainter>
#include <QVector>
#include <QPointF>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTableWidget>
#include "SimulationEngine.h"

/**
 * @brief Custom QLabel subclass that handles mouse interactions for DEM visualization
 * 
 * Implements mouse events for:
 * - Click handling (outlet selection)
 * - Drag handling (panning)
 * - Wheel handling (zooming)
 * - Double click (view reset)
 */
class ClickableLabel;

/**
 * @brief Main application window managing UI and simulation control
 * 
 * The MainWindow class provides:
 * 1. User Interface Management
 *    - Parameter input panels
 *    - Simulation controls
 *    - Visualization displays
 *    - Outlet selection interface
 * 
 * 2. Simulation Control
 *    - DEM loading and validation
 *    - Parameter synchronization
 *    - Time step execution
 *    - Results visualization
 * 
 * 3. Interactive Features
 *    - Manual outlet selection
 *    - Pan and zoom controls
 *    - Time-varying rainfall configuration
 *    - Grid and ruler customization
 * 
 * 4. Data Export
 *    - Drainage time series
 *    - Per-outlet statistics
 *    - Visualization snapshots
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    /**
     * @brief Handles window resize events to maintain visualization aspect ratios
     * Updates the display layout and recalculates visualization scaling
     */
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // DEM and Simulation Control
    void onSelectDEM();              ///< Open file dialog for DEM selection
    void onStartSimulation();        ///< Initialize and start simulation
    void onPauseSimulation();        ///< Pause ongoing simulation
    void onStopSimulation();         ///< Stop and reset simulation
    void onSimulationStep();         ///< Process one simulation timestep
    
    // UI State Management
    void onOutletMethodChanged(int index); ///< Switch between auto/manual outlets
    void updateUIState();            ///< Sync UI with simulation state
    void onManualOutletCellsToggled(bool checked);  ///< Enable/disable manual selection
    
    // Visualization Control
    void onToggleGrid(bool checked);         ///< Show/hide grid overlay
    void onToggleRulers(bool checked);       ///< Show/hide coordinate rulers
    void onGridIntervalChanged(int value); ///< Update grid line spacing
    
    // Time-varying Rainfall Management
    void onTimeVaryingRainfallToggled(bool checked);  ///< Enable/disable schedule
    void onAddRainfallRow();         ///< Add new rainfall schedule entry
    void onRemoveRainfallRow();      ///< Remove selected schedule entry
    void updateRainfallSchedule();    ///< Sync schedule with engine

    void onSaveResults();
    void onClearOutlets();
    void onSelectOutlet();
    void onSimDisplayClicked(QPoint pos);
    void updateVisualization();
    void resetVisualizationView();
    void returnToPreviousTab();
    void onResolutionChanged(double newResolution);

private:
    // Initialize UI Components
    void setupUI();
    void createInputPanel();
    void createControlPanel();
    void createVisualizationPanel();
    void createDEMPreviewPanel();
    void createRainfallPanel(QWidget* parent = nullptr);
    void setupConnections();
    void setupSimulationEngineConnections();
    
    // Helper methods for manual outlet selection
    void showDEMPreview();
    void zoomIn();
    void zoomOut();
    void resetView();
    void panView(const QPoint& delta);
    void updateDEMDisplay();
    void updateOutletTable();
    
    // Unified view methods that work for both tabs
    void updateDisplay(QLabel* displayLabel, const QImage& image, QLabel* statusLabel, const QString& statusText, QPainter* customPainter = nullptr);
    void resetDisplayView(QLabel* displayLabel, QLabel* statusLabel, const QString& statusMessage);
    void zoomDisplay(QLabel* displayLabel, bool zoomIn);
    void panDisplay(const QPoint& delta);
    
    // GUI controls - Input Panel
    QPushButton *selectDEMButton;
    QLabel *demFileLabel;
    QDoubleSpinBox *rainfallEdit;        // Input rainfall rate (m/s)
    QDoubleSpinBox *manningCoeffEdit;    // Manning's coefficient
    QDoubleSpinBox *infiltrationEdit;    // Infiltration rate (m/s)
    QSpinBox *totalTimeEdit;             // Total simulation time (seconds)
    QDoubleSpinBox *minDepthEdit;        // Minimum water depth threshold
    QDoubleSpinBox *resolutionEdit;      // Cell resolution (meters per cell)
    
    // Time-varying rainfall controls
    QCheckBox *timeVaryingRainfallCheckbox;
    QTableWidget *rainfallTableWidget;
    QPushButton *addRainfallRowButton;
    QPushButton *removeRainfallRowButton;
    QPushButton *clearRainfallRowsButton;
    
    // Outlet selection
    QComboBox *outletMethodCombo;
    QSpinBox *outletPercentileEdit; // Percentile for auto-selection
    QCheckBox *manualOutletCheckbox;     // Manual outlet selection mode
    QPushButton *selectOutletButton;     // Button to select outlet cells
    QPushButton *clearOutletsButton;     // Clear manual outlets
    bool manualOutletSelectionMode;      // Whether manual outlet selection is active
    QVector<QPoint> manualOutletCells;   // Manually selected outlet cells
    QSize currentPixmapSize;             // Current size of the displayed pixmap
    
    // Outlet table for displaying coordinates and drainage data
    QTableWidget *outletTableWidget;
    
    // Visualization options
    QCheckBox *showGridCheckbox;         // Toggle grid display
    QCheckBox *showRulersCheckbox;       // Toggle rulers display
    QSpinBox *gridIntervalSpinBox;       // Grid line interval setting
    
    // Pan and zoom variables
    float zoomLevel;                     // Current zoom level
    QPoint panOffset;                    // Current pan offset
    bool isPanning;                      // Whether currently panning
    QPoint lastPanPos;                   // Last position during panning
    QImage currentDEMImage;              // Stored DEM image for panning/zooming
    
    // Simulation Controls
    QPushButton *startButton;
    QPushButton *pauseButton;
    QPushButton *stopButton;
    QProgressBar *simulationProgress;
    QLabel *timeElapsedLabel;
    QLabel *drainageVolumeLabel;
    
    // Visualization Elements
    ClickableLabel *simDisplayLabel;     // Area where DEM and outlet selection is shown (changed from QLabel* to ClickableLabel*)
    ClickableLabel *resultDisplayLabel;  // Area where simulation results are shown (changed from QLabel to ClickableLabel)
    
    // Output Controls
    QPushButton *saveResultsButton;
    QLabel *outputLabel;                 // Shows selection feedback
    QLabel *resultsOutputLabel;          // Shows simulation results
    
    // Layout containers
    QTabWidget *mainTabWidget;
    QWidget *inputTab;
    QWidget *visualizationTab;
    QWidget *demPreviewTab;
    int previousTabIndex; // To store the previous tab index
    
    // Simulation engine and control
    SimulationEngine *simEngine;
    QTimer *simTimer;
    QTimer *uiUpdateTimer;
    bool simulationRunning;
    bool simulationPaused;
    QImage currentSimulationImage;        // Store current simulation image
    
    // Mouse event handlers for the visualization panel
    void onSimDisplayMousePress(QMouseEvent* event);
    void onSimDisplayMouseMove(QMouseEvent* event);
    void onSimDisplayMouseRelease(QMouseEvent* event);
    void onSimDisplayMouseDoubleClick(QMouseEvent* event);
    void onSimDisplayWheel(QWheelEvent* event);
    
    // Visualization result panel methods
    void onResultDisplayClicked(QPoint pos);
    void panVisualization(QPoint delta);
    void zoomVisualization(int delta);

    /**
     * @brief Validates simulation parameters before starting
     * @return bool True if all parameters are valid, false otherwise
     * 
     * Checks:
     * - DEM file is loaded
     * - Resolution is appropriate
     * - Rainfall configuration is valid
     * - At least one outlet is defined
     * - Time settings are consistent
     */
    bool validateSimulationParameters();

    /**
     * @brief Validates rainfall schedule entries
     * @return QString Empty if valid, error message if invalid
     * 
     * Validates:
     * - Time values are ascending
     * - First entry starts at t=0
     * - No negative rainfall rates
     * - Times within simulation duration
     */
    QString validateRainfallSchedule();

    /**
     * @brief Validates manual outlet cell selections
     * @return bool True if outlets are valid, false otherwise
     * 
     * Checks:
     * - Outlets within DEM bounds
     * - No duplicate positions
     * - Valid elevation at positions
     * - Sufficient spacing between outlets
     */
    bool validateOutletSelections();

    /**
     * @brief Exports simulation results to file
     * @return bool True if export successful, false otherwise
     * 
     * Exports:
     * - Time series of drainage volumes
     * - Per-outlet drainage data
     * - Final water depth grid
     * - Flow accumulation paths
     * - Simulation parameters used
     */
    bool exportResults();

    /**
     * @brief Captures and saves visualization images
     * @param type Type of visualization (water depth, flow paths, etc.)
     * @return QString Path to saved image or error message
     * 
     * Features:
     * - High-resolution capture
     * - Optional grid overlay
     * - Scale bar and legend
     * - Timestamp and parameter overlay
     */
    QString saveVisualization(const QString& type);

    /**
     * @brief Generates summary statistics for the simulation
     * @return QString Formatted statistics text
     * 
     * Statistics include:
     * - Total drainage volume
     * - Peak flow rates
     * - Drainage efficiency per outlet
     * - Water balance components
     * - Runtime performance metrics
     */
    QString generateStatistics();
};

#endif // MAINWINDOW_H
