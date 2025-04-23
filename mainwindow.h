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
#include <QVTKOpenGLNativeWidget.h>
#include "SimulationEngine.h"
#include "VTKTerrainVisualizer.h"
#include "VTKWaterVisualizer.h"

// Forward declaration of ClickableLabel
class ClickableLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(bool enableVTKVisualization = true, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // Handle window resize events
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSelectDEM();
    void onStartSimulation();
    void onPauseSimulation();
    void onStopSimulation();
    void onSimulationStep();
    void onOutletMethodChanged(int index);
    void updateUIState();
    void onManualOutletCellsToggled(bool checked);
    void onSaveResults();
    void onClearOutlets();
    void onSelectOutlet();
    void onSimDisplayClicked(QPoint pos);
    void updateVisualization();
    void resetVisualizationView();
    void returnToPreviousTab();
    // Display toggle options
    void onToggleGrid(bool checked);
    void onToggleRulers(bool checked);
    void onGridIntervalChanged(int value);
    // Time-varying rainfall slots
    void onTimeVaryingRainfallToggled(bool checked);
    void onAddRainfallRow();
    void onRemoveRainfallRow();
    void updateRainfallSchedule();
    // Resolution change handling
    void onResolutionChanged(double newResolution);
    
    // New VTK-specific slots
    void onToggle3DView(bool enabled);
    void onWaterOpacityChanged(int value);
    void onVerticalExaggerationChanged(double value);
    void onShowFlowVectorsToggled(bool checked);
    void onTerrainClicked(QPoint position);
    void onWaterSurfaceSmoothingToggled(bool checked);
    void onWaterDepthUpdated(double maxDepth);
    void onSaveVTKView();
    void onSyncCameras(bool checked);

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
    
    // New VTK visualization panels - only created if VTK is available
    void createVTKTerrainPanel();
    void createVTKWaterPanel();
    
    // Flag to indicate if VTK visualization is available/enabled
    bool vtkVisualizationEnabled;
    
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
    
    // New VTK visualization options
    QCheckBox *view3DCheckbox;           // Toggle 3D view
    QSlider *waterOpacitySlider;         // Control water transparency
    QDoubleSpinBox *verticalExaggerationSpinBox; // Control 3D height scaling
    QCheckBox *showFlowVectorsCheckbox;  // Toggle flow vector display
    QCheckBox *waterSmoothingCheckbox;   // Toggle water surface smoothing
    QLabel *maxWaterDepthLabel;          // Display max water depth
    QPushButton *saveVTKViewButton;      // Save VTK view as image
    QCheckBox *syncCamerasCheckbox;      // Sync terrain and water cameras
    
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
    
    // Visualization Elements - Legacy 2D 
    ClickableLabel *simDisplayLabel;     // Area where DEM and outlet selection is shown (changed from QLabel* to ClickableLabel*)
    ClickableLabel *resultDisplayLabel;  // Area where simulation results are shown (changed from QLabel to ClickableLabel)
    
    // VTK Visualization Elements - 3D
    VTKTerrainVisualizer *vtkTerrainVisualizer;
    VTKWaterVisualizer *vtkWaterVisualizer;
    
    // Output Controls
    QPushButton *saveResultsButton;
    QLabel *outputLabel;                 // Shows selection feedback
    QLabel *resultsOutputLabel;          // Shows simulation results
    
    // Layout containers
    QTabWidget *mainTabWidget;
    QWidget *inputTab;
    QWidget *visualizationTab;
    QWidget *demPreviewTab;
    QWidget *vtkTerrainTab;
    QWidget *vtkWaterTab;
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
};

#endif // MAINWINDOW_H
