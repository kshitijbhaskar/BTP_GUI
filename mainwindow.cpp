#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QScrollBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>
#include <cmath>

// Added layout includes
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSplitter>

// VTK headers for file saving
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkRenderWindow.h>

// Custom clickable QLabel subclass to handle mouse clicks for outlet selection
class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    ClickableLabel(QWidget* parent = nullptr) : QLabel(parent) {
        // Enable mouse tracking for better interaction
        setMouseTracking(true);
    }

signals:
    void clicked(QPoint pos);
    void mouseWheelScrolled(int delta);
    void mouseDragged(QPoint delta);
    void dragStarted(QPoint pos);
    void dragEnded();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            dragging = true;
            lastDragPos = event->pos();
            emit dragStarted(event->pos());
        }
        QLabel::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (!dragged) {
                emit clicked(event->pos());
            }
            dragging = false;
            dragged = false;
            emit dragEnded();
        }
        QLabel::mouseReleaseEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging) {
            QPoint delta = event->pos() - lastDragPos;
            lastDragPos = event->pos();
            if (delta.manhattanLength() > 3) {
                dragged = true;
                emit mouseDragged(delta);
            }
        }
        QLabel::mouseMoveEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        emit mouseWheelScrolled(event->angleDelta().y());
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            emit doubleClicked();
        }
        QLabel::mouseDoubleClickEvent(event);
    }

private:
    bool dragging = false;
    bool dragged = false;
    QPoint lastDragPos;
};

// Need to include the Q_OBJECT macro implementation
#include "mainwindow.moc"

MainWindow::MainWindow(bool enableVTKVisualization, QWidget *parent)
    : QMainWindow(parent),
      simEngine(nullptr),
      simTimer(nullptr),
      uiUpdateTimer(nullptr),
      simulationRunning(false),
      simulationPaused(false),
      manualOutletSelectionMode(false),
      currentPixmapSize(0, 0),
      zoomLevel(1.0f),
      panOffset(0, 0),
      isPanning(false),
      previousTabIndex(0), // Initialize previousTabIndex
      vtkTerrainVisualizer(nullptr),
      vtkWaterVisualizer(nullptr),
      vtkVisualizationEnabled(enableVTKVisualization)
{
    qDebug() << "MainWindow constructor - START";
    try {
    // Set application style for a modern look
        qDebug() << "  Setting application style";
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Set window title and size
        qDebug() << "  Setting window title and size";
    setWindowTitle("Advanced Hydrological Simulation");
    resize(1200, 800);

    // Set minimum size to ensure UI elements are properly displayed
        qDebug() << "  Setting minimum size";
    setMinimumSize(800, 600);

    // Initialize UI components - this will create simulation engine and timers
        qDebug() << "  Setting up UI components";
    setupUI();

    // Set up signal-slot connections
        qDebug() << "  Setting up connections";
    setupConnections();

    // Update UI state
        qDebug() << "  Updating UI state";
    updateUIState();

    // Configure sizing behavior
        qDebug() << "  Setting size policy";
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        qDebug() << "MainWindow constructor - COMPLETE";
    } catch (const std::exception& e) {
        qDebug() << "Exception in MainWindow constructor:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in MainWindow constructor";
    }
}

MainWindow::~MainWindow()
{
    // Clean up any resources
}

void MainWindow::setupUI()
{
    // Apply a modern style if available
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Create the main layout and central widget
    setWindowTitle("BTP Drainage Simulator");
    resize(1200, 800);

    // Create the main tab widget
    mainTabWidget = new QTabWidget(this);
    setCentralWidget(mainTabWidget);

    // Create input panel (parameter settings)
    createInputPanel();

    // Create control panel (simulation controls)
    createControlPanel();

    // Create visualization panel
    createVisualizationPanel();

    // Create DEM preview panel
    createDEMPreviewPanel();

    // Create VTK panels only if enabled
    if (vtkVisualizationEnabled) {
        qDebug() << "Creating VTK visualization panels";
        try {
            // Create VTK terrain panel
            createVTKTerrainPanel();

            // Create VTK water panel
            createVTKWaterPanel();

            // Add VTK tabs to the main tab widget
            mainTabWidget->addTab(vtkTerrainTab, "3D Terrain");
            mainTabWidget->addTab(vtkWaterTab, "3D Water");
        }
        catch (const std::exception& e) {
            qDebug() << "Exception creating VTK panels:" << e.what();
            vtkVisualizationEnabled = false;
        }
        catch (...) {
            qDebug() << "Unknown exception creating VTK panels";
            vtkVisualizationEnabled = false;
        }
    } else {
        qDebug() << "VTK visualization is disabled - not creating 3D panels";
    }

    // Add tabs to the main tab widget
    mainTabWidget->addTab(inputTab, "Parameters");
    mainTabWidget->addTab(visualizationTab, "Simulation");
    mainTabWidget->addTab(demPreviewTab, "DEM Preview");

    // Initialize the UI state
    previousTabIndex = 0; // Parameters tab
    zoomLevel = 1.0;
    isPanning = false;

    // Set up connections between UI elements
    setupConnections();

    // Create simulation engine
    simEngine = new SimulationEngine(this);

    // Set up connections for the simulation engine
    setupSimulationEngineConnections();

    // Initialize simulation state
    simulationRunning = false;
    simulationPaused = false;

    // Create simulation timer
    simTimer = new QTimer(this);
    connect(simTimer, &QTimer::timeout, this, &MainWindow::onSimulationStep);

    // Create UI update timer
    uiUpdateTimer = new QTimer(this);
    connect(uiUpdateTimer, &QTimer::timeout, this, &MainWindow::updateUIState);
    uiUpdateTimer->start(250); // Update UI state 4 times per second

    // Update UI state
    updateUIState();
}

void MainWindow::createInputPanel()
{
    // ======== SIMULATION PARAMETERS GROUP ========
    QGroupBox *simulationParamGroup = new QGroupBox("Simulation Parameters", inputTab);
    QFormLayout *paramLayout = new QFormLayout();

    // DEM File Selection
    QHBoxLayout *demLayout = new QHBoxLayout();
    selectDEMButton = new QPushButton("Browse...", inputTab);
    demFileLabel = new QLabel("No file selected", inputTab);
    demLayout->addWidget(selectDEMButton);
    demLayout->addWidget(demFileLabel, 1);
    paramLayout->addRow("DEM CSV File:", demLayout);

    // Time parameters
    totalTimeEdit = new QSpinBox(inputTab);
    totalTimeEdit->setMinimum(1);
    totalTimeEdit->setMaximum(100000);
    totalTimeEdit->setValue(1800);
    totalTimeEdit->setSuffix(" seconds");
    paramLayout->addRow("Total Simulation Time:", totalTimeEdit);

    // Resolution - with a more detailed description
    QHBoxLayout *resolutionLayout = new QHBoxLayout();
    resolutionEdit = new QDoubleSpinBox(inputTab);
    resolutionEdit->setMinimum(0.01);
    resolutionEdit->setMaximum(100.0);
    resolutionEdit->setValue(0.25);
    resolutionEdit->setSuffix(" m");
    resolutionEdit->setDecimals(3);
    resolutionLayout->addWidget(resolutionEdit);

    // Add resolution help button
    QPushButton *resolutionHelpButton = new QPushButton("?");
    resolutionHelpButton->setMaximumWidth(30);
    resolutionHelpButton->setToolTip("Click for information about setting the correct resolution");
    connect(resolutionHelpButton, &QPushButton::clicked, [this]() {
        QMessageBox::information(this, "Resolution Information",
            "Cell Resolution refers to the physical size represented by each cell in your DEM.\n\n"
            "• For fine-detail DEMs (e.g., LiDAR): typically 0.1-1.0 meters\n"
            "• For standard DEMs: typically 1.0-5.0 meters\n"
            "• For coarse/regional DEMs: typically 5.0-30.0 meters\n\n"
            "Higher resolutions (smaller values) provide more detail but require more processing power. "
            "Resolution affects rendering quality and simulation accuracy.\n\n"
            "The resolution must match your DEM file's actual resolution for accurate results.");
    });
    resolutionLayout->addWidget(resolutionHelpButton);

    QLabel *resolutionDescription = new QLabel("Higher values = coarser resolution. Lower values = finer resolution.", inputTab);
    resolutionDescription->setStyleSheet("color: gray; font-style: italic;");

    paramLayout->addRow("Cell Resolution:", resolutionLayout);
    paramLayout->addRow("", resolutionDescription);

    // Note about rainfall configuration being moved to its own tab
    QLabel* rainfallNoteLabel = new QLabel("Rainfall configuration is available in the 'Rainfall Configuration' tab.", inputTab);
    rainfallNoteLabel->setStyleSheet("color: blue; font-style: italic;");
    paramLayout->addRow("", rainfallNoteLabel);

    // Manning's coefficient
    manningCoeffEdit = new QDoubleSpinBox(inputTab);
    manningCoeffEdit->setMinimum(0.01);
    manningCoeffEdit->setMaximum(1.0);
    manningCoeffEdit->setValue(0.03);
    manningCoeffEdit->setDecimals(3);
    paramLayout->addRow("Manning's Coefficient:", manningCoeffEdit);

    // Infiltration rate (Ks)
    infiltrationEdit = new QDoubleSpinBox(inputTab);
    infiltrationEdit->setMinimum(0.0);
    infiltrationEdit->setMaximum(0.001);
    // Set infiltration rate much lower than rainfall to ensure water accumulation
    infiltrationEdit->setValue(0.0000001); // 1e-7, reduced from 1e-6
    infiltrationEdit->setDecimals(8);
    infiltrationEdit->setSingleStep(1e-7);
    paramLayout->addRow("Infiltration Rate (m/s):", infiltrationEdit);

    // Min water depth threshold
    minDepthEdit = new QDoubleSpinBox(inputTab);
    minDepthEdit->setMinimum(1e-6);
    minDepthEdit->setMaximum(0.1);
    minDepthEdit->setValue(1e-5);
    minDepthEdit->setDecimals(6);
    paramLayout->addRow("Min Water Depth (m):", minDepthEdit);

    // Constant rainfall rate - this will be shown/hidden based on time-varying setting
    rainfallEdit = new QDoubleSpinBox;
    rainfallEdit->setDecimals(8);  // Allow very small values
    rainfallEdit->setRange(0.0, 0.001);  // Up to 1 mm/s (3.6 m/hour)
    rainfallEdit->setSingleStep(0.0000001);
    // Increase default rainfall to very heavy rain (100 mm/hour = ~0.000028 m/s)
    rainfallEdit->setValue(0.000028);
    paramLayout->addRow("Constant Rainfall Rate (m/s):", rainfallEdit);

    // Time-varying rainfall checkbox
    timeVaryingRainfallCheckbox = new QCheckBox("Use Time-Varying Rainfall", inputTab);
    timeVaryingRainfallCheckbox->setChecked(false);
    paramLayout->addRow("", timeVaryingRainfallCheckbox);

    // Outlet selection method
    outletMethodCombo = new QComboBox(inputTab);
    outletMethodCombo->addItem("Automatic (Elevation Based)");
    outletMethodCombo->addItem("Manual Selection");
    paramLayout->addRow("Outlet Method:", outletMethodCombo);

    // Outlet percentile for automatic method
    outletPercentileEdit = new QSpinBox(inputTab);
    outletPercentileEdit->setMinimum(1);
    outletPercentileEdit->setMaximum(100);
    outletPercentileEdit->setValue(10);
    outletPercentileEdit->setSuffix("%");
    paramLayout->addRow("Outlet Percentile:", outletPercentileEdit);

    // Manual outlet selection options (initially hidden)
    manualOutletCheckbox = new QCheckBox("Manual Outlet Selection Mode", inputTab);
    manualOutletCheckbox->setChecked(false);
    paramLayout->addRow("", manualOutletCheckbox);

    // Set the layout to the group
    simulationParamGroup->setLayout(paramLayout);

    // Add the group to the input tab
    static_cast<QVBoxLayout*>(inputTab->layout())->addWidget(simulationParamGroup);

    // Add vertical spacer to push everything up
    static_cast<QVBoxLayout*>(inputTab->layout())->addStretch();
}

void MainWindow::createRainfallPanel(QWidget* parent)
{
    // Use the provided parent or default to inputTab if none is provided
    QWidget* parentWidget = parent ? parent : inputTab;

    // Create rainfall schedule group
    QGroupBox *rainfallGroup = new QGroupBox("Time-varying Rainfall Schedule", parentWidget);
    QVBoxLayout *rainfallLayout = new QVBoxLayout();

    // Add checkbox for enabling time-varying rainfall
    timeVaryingRainfallCheckbox = new QCheckBox("Enable Time-varying Rainfall", parentWidget);

    // Add configuration for constant rainfall as a reference
    QGroupBox *constantRainfallGroup = new QGroupBox("Constant Rainfall Reference", parentWidget);
    QFormLayout *constantRainfallLayout = new QFormLayout();

    // Rainfall rate (moved from input panel)
    rainfallEdit = new QDoubleSpinBox(parentWidget);
    rainfallEdit->setMinimum(0.0);
    rainfallEdit->setMaximum(0.001);
    rainfallEdit->setValue(0.00001);
    rainfallEdit->setDecimals(8);  // Increased from previous value to allow for very small values
    rainfallEdit->setSingleStep(0.0000001);  // Smaller step for precise adjustment
    constantRainfallLayout->addRow("Constant Rainfall Rate:", rainfallEdit);
    constantRainfallGroup->setLayout(constantRainfallLayout);

    // Rainfall table with improved styling to match outlet table
    rainfallTableWidget = new QTableWidget(0, 2, parentWidget);
    rainfallTableWidget->setHorizontalHeaderLabels(QStringList() << "Time (seconds)" << "Rainfall Rate (m/s)");
    rainfallTableWidget->horizontalHeader()->setStretchLastSection(true);
    rainfallTableWidget->setMinimumHeight(250); // Increased height for better visibility
    // Set alternating row colors and selection behavior to match outlet table
    rainfallTableWidget->setAlternatingRowColors(true);
    rainfallTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    rainfallTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Buttons for table control - in their own layout with margin at bottom
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    addRainfallRowButton = new QPushButton("Add Row", parentWidget);
    removeRainfallRowButton = new QPushButton("Remove Selected", parentWidget);
    clearRainfallRowsButton = new QPushButton("Clear All", parentWidget);

    buttonLayout->addWidget(addRainfallRowButton);
    buttonLayout->addWidget(removeRainfallRowButton);
    buttonLayout->addWidget(clearRainfallRowsButton);

    // Add explanation label with more detailed instructions
    QLabel *explanationLabel = new QLabel(
        "Define rainfall intensity at different time points during the simulation. "
        "The simulation will linearly interpolate between time points to calculate "
        "the rainfall rate at any given moment.\n\n"
        "The first row should generally start at time 0. The last time point should "
        "not exceed the total simulation time defined in the Input Parameters tab.\n\n"
        "You can create patterns like:\n"
        "- Constant rainfall: Single entry or same intensity at different times\n"
        "- Increasing/decreasing rainfall: Gradually change intensity over time\n"
        "- Storm patterns: High intensity for a short period followed by lower rates",
        parentWidget);
    explanationLabel->setWordWrap(true);

    // Add visual rainfall pattern examples
    QLabel *patternExamplesLabel = new QLabel("Example Patterns:", parentWidget);

    // Create a chart-like visual to show example patterns
    QLabel *patternsChart = new QLabel(parentWidget);
    patternsChart->setMinimumHeight(120);
    patternsChart->setFrameStyle(QFrame::Box | QFrame::Sunken);

    // Create a QPixmap for drawing the examples
    QPixmap chartPixmap(600, 120);
    chartPixmap.fill(Qt::white);
    QPainter painter(&chartPixmap);

    // Draw axes
    painter.setPen(QPen(Qt::black, 1));
    painter.drawLine(50, 100, 550, 100);  // x-axis (time)
    painter.drawLine(50, 20, 50, 100);    // y-axis (intensity)

    // Draw axis labels
    painter.drawText(20, 60, "Intensity");
    painter.drawText(300, 115, "Time");

    // Draw example patterns
    // 1. Constant rainfall
    painter.setPen(QPen(Qt::blue, 2));
    painter.drawLine(50, 50, 175, 50);
    painter.drawText(90, 30, "Constant");

    // 2. Increasing rainfall
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(175, 80, 300, 30);
    painter.drawText(220, 20, "Increasing");

    // 3. Storm pattern
    painter.setPen(QPen(Qt::green, 2));
    QPolygon stormPattern;
    stormPattern << QPoint(300, 80) << QPoint(325, 30)
                 << QPoint(375, 30) << QPoint(400, 60)
                 << QPoint(450, 80) << QPoint(500, 90);
    painter.drawPolyline(stormPattern);
    painter.drawText(350, 20, "Storm Pattern");

    patternsChart->setPixmap(chartPixmap);

    // Create a scroll area for the table to handle many rows
    QScrollArea* tableScrollArea = new QScrollArea(parentWidget);
    tableScrollArea->setWidgetResizable(true);
    QWidget* tableContainer = new QWidget(tableScrollArea);
    QVBoxLayout* tableContainerLayout = new QVBoxLayout(tableContainer);
    tableContainerLayout->addWidget(rainfallTableWidget);
    tableScrollArea->setWidget(tableContainer);

    // Add widgets to layout with proper spacing
    rainfallLayout->addWidget(explanationLabel);
    rainfallLayout->addWidget(patternExamplesLabel);
    rainfallLayout->addWidget(patternsChart);
    rainfallLayout->addWidget(tableScrollArea, 1);  // Give table scroll area stretch factor
    rainfallLayout->addSpacing(10);  // Add space between table and buttons
    rainfallLayout->addLayout(buttonLayout);
    rainfallLayout->addSpacing(10);  // Add space at bottom

    rainfallGroup->setLayout(rainfallLayout);

    // Add the components to the parent widget layout
    QVBoxLayout* parentLayout = qobject_cast<QVBoxLayout*>(parentWidget->layout());
    if (parentLayout) {
        parentLayout->addWidget(timeVaryingRainfallCheckbox);
        parentLayout->addWidget(constantRainfallGroup);
        parentLayout->addWidget(rainfallGroup);
        parentLayout->addStretch();
    }

    // Initially disable the time-varying controls
    rainfallGroup->setEnabled(false);

    // Connect the checkbox to enable/disable time-varying controls
    connect(timeVaryingRainfallCheckbox, &QCheckBox::toggled, [this, rainfallGroup](bool checked) {
        rainfallGroup->setEnabled(checked);
        rainfallEdit->setEnabled(!checked);
        onTimeVaryingRainfallToggled(checked);
    });

    // Add a default row with time 0
    onAddRainfallRow();
}

void MainWindow::createVisualizationPanel()
{
    // Create layout for visualization tab
    QVBoxLayout *visLayout = new QVBoxLayout(visualizationTab);

    // Create a top controls group box
    QGroupBox *visControlsGroup = new QGroupBox("Simulation Controls", visualizationTab);
    QHBoxLayout *controlsLayout = new QHBoxLayout();

    // Add visualization controls (buttons)
    startButton = new QPushButton("Start", visualizationTab);
    pauseButton = new QPushButton("Pause", visualizationTab);
    stopButton = new QPushButton("Stop", visualizationTab);
    saveResultsButton = new QPushButton("Save Results", visualizationTab);

    // Add navigation button to return to previous tab
    QPushButton *returnButton = new QPushButton("Return to Previous Tab", visualizationTab);
    returnButton->setToolTip("Click to return to the previous tab you were viewing");
    connect(returnButton, &QPushButton::clicked, this, &MainWindow::returnToPreviousTab);

    controlsLayout->addWidget(startButton);
    controlsLayout->addWidget(pauseButton);
    controlsLayout->addWidget(stopButton);
    controlsLayout->addWidget(saveResultsButton);
    controlsLayout->addWidget(returnButton);
    visControlsGroup->setLayout(controlsLayout);

    // Progress indicators
    QGroupBox *progressGroup = new QGroupBox("Simulation Progress", visualizationTab);
    QHBoxLayout *progressLayout = new QHBoxLayout();

    simulationProgress = new QProgressBar(visualizationTab);
    simulationProgress->setRange(0, 100);
    simulationProgress->setValue(0);
    timeElapsedLabel = new QLabel("Time: 0 s", visualizationTab);
    drainageVolumeLabel = new QLabel("Drainage: 0 m³", visualizationTab);

    progressLayout->addWidget(simulationProgress, 1);
    progressLayout->addWidget(timeElapsedLabel);
    progressLayout->addWidget(drainageVolumeLabel);
    progressGroup->setLayout(progressLayout);

    // Add to main layout
    visLayout->addWidget(visControlsGroup);
    visLayout->addWidget(progressGroup);

    // Create a horizontal split for the visualization area and outlet table
    QHBoxLayout *splitLayout = new QHBoxLayout();

    // Left side: visualization display in a scroll area
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setAlignment(Qt::AlignCenter);

    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);

    QLabel *titleLabel = new QLabel("Water Depth Visualization");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    // Create a ClickableLabel for the result display with interaction support
    resultDisplayLabel = new ClickableLabel(scrollContent);
    resultDisplayLabel->setText("Simulation not started");
    resultDisplayLabel->setAlignment(Qt::AlignCenter);
    resultDisplayLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    resultDisplayLabel->setMinimumSize(400, 300);

    scrollLayout->addWidget(titleLabel);
    scrollLayout->addWidget(resultDisplayLabel);
    scrollArea->setWidget(scrollContent);

    // Right side: outlet table
    QVBoxLayout *tableLayout = new QVBoxLayout();
    QLabel *tableTitleLabel = new QLabel("Outlet Cells");
    tableTitleLabel->setAlignment(Qt::AlignCenter);
    tableTitleLabel->setFont(titleFont);

    // Use the existing outletTableWidget initialized in setupUI
    tableLayout->addWidget(tableTitleLabel);
    tableLayout->addWidget(outletTableWidget);

    // Add both sides to split layout
    splitLayout->addWidget(scrollArea, 2); // 2:1 ratio
    splitLayout->addLayout(tableLayout, 1);

    // Control buttons for zooming
    QHBoxLayout *zoomLayout = new QHBoxLayout();
    QPushButton *zoomInButton = new QPushButton("Zoom In");
    QPushButton *zoomOutButton = new QPushButton("Zoom Out");
    QPushButton *resetViewButton = new QPushButton("Reset View");

    zoomLayout->addWidget(zoomInButton);
    zoomLayout->addWidget(zoomOutButton);
    zoomLayout->addWidget(resetViewButton);

    // Connect zoom buttons
    connect(zoomInButton, &QPushButton::clicked, this, [this]() {
        zoomVisualization(120); // Zoom in
    });

    connect(zoomOutButton, &QPushButton::clicked, this, [this]() {
        zoomVisualization(-120); // Zoom out
    });

    connect(resetViewButton, &QPushButton::clicked, this, &MainWindow::resetVisualizationView);

    // Remove redundant connections as they'll be handled in setupConnections()

    // Status label for output messages
    resultsOutputLabel = new QLabel("No simulation results available");
    resultsOutputLabel->setWordWrap(true);
    resultsOutputLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Create a GroupBox for display options
    QGroupBox *displayOptionsGroup = new QGroupBox("Display Options");
    QVBoxLayout *displayOptionsLayout = new QVBoxLayout();

    // Add checkboxes for grid and ruler toggling
    showGridCheckbox = new QCheckBox("Show Grid");
    showGridCheckbox->setChecked(true);
    showRulersCheckbox = new QCheckBox("Show Rulers");
    showRulersCheckbox->setChecked(false); // Set to unchecked initially

    // Add a spinbox for grid interval
    QHBoxLayout *gridIntervalLayout = new QHBoxLayout();
    QLabel *gridIntervalLabel = new QLabel("Grid Interval:");
    gridIntervalSpinBox = new QSpinBox();
    gridIntervalSpinBox->setRange(5, 50);
    gridIntervalSpinBox->setValue(10);
    gridIntervalSpinBox->setSingleStep(5);
    gridIntervalLayout->addWidget(gridIntervalLabel);
    gridIntervalLayout->addWidget(gridIntervalSpinBox);

    // Add the controls to the layout
    displayOptionsLayout->addWidget(showGridCheckbox);
    displayOptionsLayout->addWidget(showRulersCheckbox);
    displayOptionsLayout->addLayout(gridIntervalLayout);
    displayOptionsGroup->setLayout(displayOptionsLayout);

    // Connect the signals to slots
    connect(showGridCheckbox, &QCheckBox::toggled, this, &MainWindow::onToggleGrid);
    connect(showRulersCheckbox, &QCheckBox::toggled, this, &MainWindow::onToggleRulers);
    connect(gridIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onGridIntervalChanged);

    // Add components to layout
    visLayout->addLayout(splitLayout);
    visLayout->addLayout(zoomLayout);
    visLayout->addWidget(resultsOutputLabel);
    visLayout->addWidget(displayOptionsGroup);
}

void MainWindow::createDEMPreviewPanel()
{
    demPreviewTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(demPreviewTab);

    // Create a splitter to allow resizing between DEM and outlet table
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);

    // Left side - DEM preview
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);

    // Control buttons
    QHBoxLayout* controlLayout = new QHBoxLayout();

    // Add zoom controls
    QPushButton* zoomInBtn = new QPushButton("+");
    QPushButton* zoomOutBtn = new QPushButton("-");
    QPushButton* resetViewBtn = new QPushButton("Reset View");
    zoomInBtn->setToolTip("Zoom In");
    zoomOutBtn->setToolTip("Zoom Out");
    resetViewBtn->setToolTip("Reset View");

    controlLayout->addWidget(zoomInBtn);
    controlLayout->addWidget(zoomOutBtn);
    controlLayout->addWidget(resetViewBtn);
    controlLayout->addStretch();

    // Add outlet controls to the same row
    selectOutletButton = new QPushButton("Select Outlets");
    clearOutletsButton = new QPushButton("Clear Outlets");
    controlLayout->addWidget(selectOutletButton);
    controlLayout->addWidget(clearOutletsButton);

    // Connect the buttons
    connect(zoomInBtn, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &MainWindow::zoomOut);
    connect(resetViewBtn, &QPushButton::clicked, this, &MainWindow::resetView);
    connect(selectOutletButton, &QPushButton::clicked, this, &MainWindow::onSelectOutlet);
    connect(clearOutletsButton, &QPushButton::clicked, this, &MainWindow::onClearOutlets);

    leftLayout->addLayout(controlLayout);

    // Create a scrollable area for the DEM preview
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setAlignment(Qt::AlignCenter);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Container widget to hold the label
    QWidget* container = new QWidget();
    QVBoxLayout* containerLayout = new QVBoxLayout(container);
    container->setMinimumSize(800, 600); // Set a larger minimum size

    // Create a clickable label for the DEM display
    simDisplayLabel = new ClickableLabel(container);
    simDisplayLabel->setAlignment(Qt::AlignCenter);
    simDisplayLabel->setText("DEM Preview");
    simDisplayLabel->setMinimumSize(700, 500); // Set a larger minimum size
    containerLayout->addWidget(simDisplayLabel);

    // Add the container to the scroll area
    scrollArea->setWidget(container);

    leftLayout->addWidget(scrollArea, 1);

    // Status label at the bottom
    outputLabel = new QLabel("Load a DEM file and select manual outlet mode to begin selecting outlets.");
    outputLabel->setWordWrap(true);
    leftLayout->addWidget(outputLabel);

    // Right side - Outlet table
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* tableLayout = new QVBoxLayout(rightPanel);

    QLabel* tableTitle = new QLabel("Outlet Cells", rightPanel);
    tableTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    tableLayout->addWidget(tableTitle);

    // Use the existing outlet table widget instead of creating a new one
    // outletTableWidget is now initialized in setupUI()
    tableLayout->addWidget(outletTableWidget);

    // Add panels to splitter
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);

    // Set initial sizes (70% for DEM, 30% for table)
    splitter->setSizes(QList<int>() << 700 << 300);

    // Add splitter to main layout
    mainLayout->addWidget(splitter);

    // Connect the simDisplayLabel click signal
    connect(simDisplayLabel, &ClickableLabel::clicked, this, &MainWindow::onSimDisplayClicked);

    // Use lambdas to connect signals with different parameter types
    connect(simDisplayLabel, &ClickableLabel::mouseWheelScrolled, this,
            [this](int delta) {
                // Create a wheel event with the delta or call directly
                zoomDisplay(simDisplayLabel, delta > 0);
            });

    connect(simDisplayLabel, &ClickableLabel::mouseDragged, this,
            [this](QPoint delta) {
                // Update pan offset and display
                panOffset += delta;
                updateDEMDisplay();
            });

    connect(simDisplayLabel, &ClickableLabel::dragStarted, this,
            [this](QPoint pos) {
                // Handle drag start if needed
                // For now just logging without creating a MouseEvent
                qDebug() << "Drag started at:" << pos;
            });

    connect(simDisplayLabel, &ClickableLabel::dragEnded, this,
            [this]() {
                // Handle drag end if needed
                // For now just logging without creating a MouseEvent
                qDebug() << "Drag ended";
            });

    connect(simDisplayLabel, &ClickableLabel::doubleClicked, this,
            [this]() {
                // Reset view on double click
                resetDisplayView(simDisplayLabel, outputLabel, "View reset to default (100% zoom)");
            });
}

void MainWindow::setupConnections()
{
    // Setup button connections
    connect(selectDEMButton, &QPushButton::clicked, this, &MainWindow::onSelectDEM);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartSimulation);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseSimulation);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopSimulation);
    connect(saveResultsButton, &QPushButton::clicked, this, &MainWindow::onSaveResults);
    connect(clearOutletsButton, &QPushButton::clicked, this, &MainWindow::onClearOutlets);
    connect(selectOutletButton, &QPushButton::clicked, this, &MainWindow::onSelectOutlet);

    // Connect timers
    connect(simTimer, &QTimer::timeout, this, &MainWindow::onSimulationStep);
    connect(uiUpdateTimer, &QTimer::timeout, this, &MainWindow::updateUIState);

    // Connect outlet method combo box
    connect(outletMethodCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onOutletMethodChanged);

    // Connect manual outlet selection mode checkbox
    connect(manualOutletCheckbox, &QCheckBox::toggled, this, &MainWindow::onManualOutletCellsToggled);

    // Connect the result display for mouse interaction
    if (resultDisplayLabel) {
        connect(resultDisplayLabel, &ClickableLabel::clicked, this, &MainWindow::onResultDisplayClicked);
        connect(resultDisplayLabel, &ClickableLabel::mouseDragged, this, &MainWindow::panVisualization);
        connect(resultDisplayLabel, &ClickableLabel::mouseWheelScrolled, this, &MainWindow::zoomVisualization);
        connect(resultDisplayLabel, &ClickableLabel::doubleClicked, this, &MainWindow::resetVisualizationView);
    }

    // Connect tab changes
    connect(mainTabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        // Check which tab is selected
        if (index == mainTabWidget->indexOf(demPreviewTab)) { // Use indexOf instead of hardcoded 2
            // We're switching to the DEM Preview tab
            if (demFileLabel->text() != "No file selected") {
                // Only update the preview if a DEM file is loaded
                showDEMPreview();
            }
        }
    });

    // Connect display toggle options
    connect(showGridCheckbox, &QCheckBox::toggled, this, &MainWindow::onToggleGrid);
    connect(showRulersCheckbox, &QCheckBox::toggled, this, &MainWindow::onToggleRulers);
    connect(gridIntervalSpinBox, &QSpinBox::valueChanged, this, &MainWindow::onGridIntervalChanged);

    // Connect time-varying rainfall checkbox
    connect(timeVaryingRainfallCheckbox, &QCheckBox::toggled, this, &MainWindow::onTimeVaryingRainfallToggled);

    // Connect rainfall table buttons
    connect(addRainfallRowButton, &QPushButton::clicked, this, &MainWindow::onAddRainfallRow);
    connect(removeRainfallRowButton, &QPushButton::clicked, this, &MainWindow::onRemoveRainfallRow);

    // Connect resolution change to immediately update display and simulation
    connect(resolutionEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onResolutionChanged);

    // Connect to simulation engine signals when available
    setupSimulationEngineConnections();
}

void MainWindow::onSelectDEM()
{
    // Open a file dialog to select a DEM file
    QString fileName = QFileDialog::getOpenFileName(this, "Open DEM File",
        QString(), "GeoTIFF Files (*.tif *.tiff);;CSV Files (*.csv);;All Files (*)");

    if (fileName.isEmpty()) {
        return;
    }

    // Update the file label
    demFileLabel->setText(fileName);

    // Load DEM file into simulation engine
    if (simEngine->loadDEM(fileName)) {
        // Show the DEM preview
                showDEMPreview();

        // Initialize the VTK visualizers with the simulation engine if enabled
        if (vtkVisualizationEnabled) {
            if (vtkTerrainVisualizer) {
                vtkTerrainVisualizer->initializeTerrain(simEngine);
            }

            if (vtkWaterVisualizer && vtkTerrainVisualizer) {
                vtkWaterVisualizer->initialize(simEngine, vtkTerrainVisualizer);
            }
        }

        // Enable simulation controls
        startButton->setEnabled(true);
        saveResultsButton->setEnabled(false);

        // Show success message
        QMessageBox::information(this, "DEM Loaded", "The DEM file has been loaded successfully.");
        } else {
        QMessageBox::critical(this, "Error", "Failed to load the DEM file.");
    }
}

void MainWindow::updateUIState()
{
    bool hasDEM = !currentDEMImage.isNull();

    // Update button states based on simulation state and DEM availability
    selectDEMButton->setEnabled(!simulationRunning);
    startButton->setEnabled(hasDEM && (!simulationRunning || simulationPaused));
    pauseButton->setEnabled(simulationRunning && !simulationPaused);
    stopButton->setEnabled(simulationRunning);
    saveResultsButton->setEnabled(hasDEM && simEngine && simEngine->getTotalDrainage() > 0);

    // Disable parameter editing during simulation
    bool canEditParams = !simulationRunning && hasDEM;
    rainfallEdit->setEnabled(canEditParams && !timeVaryingRainfallCheckbox->isChecked());
    timeVaryingRainfallCheckbox->setEnabled(canEditParams);
    manningCoeffEdit->setEnabled(canEditParams);
    infiltrationEdit->setEnabled(canEditParams);
    totalTimeEdit->setEnabled(canEditParams);
    minDepthEdit->setEnabled(canEditParams);
    resolutionEdit->setEnabled(canEditParams);

    // Update outlet controls
    bool isManualMethod = (outletMethodCombo->currentIndex() == 1);
    outletMethodCombo->setEnabled(canEditParams);
    outletPercentileEdit->setEnabled(canEditParams && !isManualMethod);
    manualOutletCheckbox->setEnabled(canEditParams && isManualMethod);
    selectOutletButton->setEnabled(canEditParams && isManualMethod && manualOutletCheckbox->isChecked());
    outletMethodCombo->setEnabled(canEditParams);
    outletPercentileEdit->setEnabled(canEditParams && outletMethodCombo->currentIndex() == 0);
    manualOutletCheckbox->setEnabled(canEditParams && outletMethodCombo->currentIndex() == 1);
    selectOutletButton->setEnabled(canEditParams && manualOutletCheckbox->isChecked());
    clearOutletsButton->setEnabled(canEditParams && !manualOutletCells.isEmpty());

    // Update simulation progress display
    if (simulationRunning && !simulationPaused) {
        double currentTime = simEngine->getCurrentTime();
        double totalTime = simEngine->getTotalTime();
        int progress = static_cast<int>((currentTime / totalTime) * 100);
        simulationProgress->setValue(progress);
    }

    // Update the simulation label text if not running
    if (!simulationRunning && !simulationPaused && resultDisplayLabel->pixmap().isNull()) {
        if (demFileLabel->text() == "No file selected") {
            resultDisplayLabel->setText("Please load a DEM file in the Settings tab");
        } else {
            resultDisplayLabel->setText("DEM loaded. Configure parameters and press Start to begin simulation.");
        }
    }

    // Update DEM preview tab text if DEM not loaded yet
    if (demFileLabel->text() == "No file selected") {
        if (!currentDEMImage.isNull()) {
            simDisplayLabel->setText("Please load a DEM file in the Settings tab");
        }
    }
}

void MainWindow::onOutletMethodChanged(int index)
{
    // Enable/disable controls based on outlet method
    bool isManualMethod = (index == 1);
    manualOutletCheckbox->setEnabled(isManualMethod);
    outletPercentileEdit->setEnabled(!isManualMethod);

    if (isManualMethod) {
        selectOutletButton->setEnabled(manualOutletCheckbox->isChecked());
        clearOutletsButton->setEnabled(!manualOutletCells.isEmpty());
    } else {
        selectOutletButton->setEnabled(false);
        clearOutletsButton->setEnabled(false);

        // Update the simulation with automatic outlets if we have a valid engine
        if (simEngine) {
            double percentile = outletPercentileEdit->value() / 100.0; // Convert to 0-1 scale
            simEngine->configureOutletsByPercentile(percentile);
            qDebug() << "Switching to automatic outlets with percentile:" << percentile;
        }
    }
}

void MainWindow::onManualOutletCellsToggled(bool checked)
{
    selectOutletButton->setEnabled(checked);
    manualOutletSelectionMode = checked;

    if (checked) {
        QMessageBox::information(this, "Manual Outlet Selection",
            "Click 'Select Outlets' button to open the DEM visualization where you can\n"
            "select outlet cells by clicking anywhere on the DEM. Selected cells will be marked with red squares.");
    } else {
        manualOutletSelectionMode = false;
    }
}

void MainWindow::onSelectOutlet()
{
    // Check if we have a DEM loaded
    if (currentDEMImage.isNull()) {
        QMessageBox::warning(this, "Error", "No DEM loaded. Please load a DEM file first.");
        return;
    }

    // Set outlet selection mode
    manualOutletSelectionMode = true;

    // Make sure the correct outlet method is selected
    if (outletMethodCombo->currentIndex() != 1) {
        outletMethodCombo->setCurrentIndex(1); // Switch to manual selection mode
    }

    // Ensure manual outlet checkbox is checked
    manualOutletCheckbox->setChecked(true);

    // Save the current tab index before switching
    previousTabIndex = mainTabWidget->currentIndex();

    // Switch to the outlet selection tab with animation
    int outletTabIndex = mainTabWidget->indexOf(demPreviewTab);
    if (outletTabIndex >= 0) {
        mainTabWidget->setCurrentIndex(outletTabIndex);

        // Flash the tab text briefly to indicate the switch
        QFont font = mainTabWidget->tabBar()->font();
        font.setBold(true);
        mainTabWidget->tabBar()->setTabTextColor(outletTabIndex, Qt::blue);
        mainTabWidget->tabBar()->setFont(font);

        // Reset the tab appearance after a short delay
        QTimer::singleShot(1000, this, [this, outletTabIndex]() {
            QFont normalFont = mainTabWidget->tabBar()->font();
            normalFont.setBold(false);
            mainTabWidget->tabBar()->setFont(normalFont);
            mainTabWidget->tabBar()->setTabTextColor(outletTabIndex, palette().text().color());
        });
    }

    // Load the DEM preview with current outlet selections
    showDEMPreview();

    // Show instructions for outlet selection
    outputLabel->setText("Click on the DEM to select outlet cells. Click on a selected cell again to deselect it. Drag to pan, scroll to zoom.");
}

void MainWindow::showDEMPreview()
{
    if (!simEngine) {
        qDebug() << "Cannot show DEM preview - simEngine is null";
        return;
    }

    if (currentDEMImage.isNull()) {
        qDebug() << "Cannot show DEM preview - currentDEMImage is null";
        return;
    }

    // Store current zoom and pan values to restore after updating
    float currentZoom = zoomLevel;
    QPoint currentPan = panOffset;

    // Make sure the SimulationEngine has the latest outlet cells
    qDebug() << "Updating SimulationEngine with" << manualOutletCells.size() << "outlet cells";
    for (const QPoint &p : manualOutletCells) {
        qDebug() << "  Cell at:" << p.x() << p.y();
    }

    // Update the simulation engine with current outlet cells
    if (manualOutletSelectionMode) {
    simEngine->setManualOutletCells(manualOutletCells);
    }

    // Always get a fresh DEM preview image to ensure outlet markings are up-to-date
    QImage img = simEngine->getDEMPreviewImage();

    if (!img.isNull()) {
        // Store the DEM image for zooming and panning
        currentDEMImage = img;

        // Restore zoom and pan settings
        zoomLevel = currentZoom;
        panOffset = currentPan;

        // Update the display using the pan/zoom settings
        updateDEMDisplay();

        // Update outlet table with current selections
        updateOutletTable();

        // Update status message
        if (manualOutletSelectionMode) {
        if (manualOutletCells.isEmpty()) {
                outputLabel->setText("Manual outlet selection mode: No outlet cells selected. Click anywhere on the DEM to select outlets.");
        } else {
                outputLabel->setText(QString("Manual outlet selection mode: %1 outlet cell(s) selected. Click to add/remove outlets.")
                              .arg(manualOutletCells.size()));
            }
        } else {
            outputLabel->setText("Using automatic outlet selection. Switch to manual mode to select outlets.");
        }
    } else {
        qDebug() << "getDEMPreviewImage returned a null image";
        outputLabel->setText("Error: Could not generate DEM preview. Please check that a valid DEM file is loaded.");
    }
}

void MainWindow::onSimDisplayClicked(QPoint pos)
{
    // Only process clicks if in manual outlet selection mode
    if (!manualOutletSelectionMode || !simEngine || currentDEMImage.isNull()) {
        return;
    }

    // Debug the click position
    qDebug() << "Click at raw position:" << pos;

    // Get the label and pixmap dimensions
    QSize labelSize = simDisplayLabel->size();
    QSize pixmapSize = currentPixmapSize; // This should be the size of the scaled *DEM* area, not the whole image

    // Get scale and margins used in DEM preview generation
    int rulerMargin = 30;
    int topMargin = 40;
    int scale = 4; // This MUST match the scale calculation in getDEMPreviewImage
    if (simEngine->getCellResolution() <= 0.5) scale = 6;
    else if (simEngine->getCellResolution() <= 1.0) scale = 5;
    else if (simEngine->getCellResolution() <= 5.0) scale = 4;
    else scale = 3;
    if (simEngine->getGridHeight() > 300 || simEngine->getGridWidth() > 300) scale = 2;

    int demWidth = simEngine->getGridWidth() * scale;
    int demHeight = simEngine->getGridHeight() * scale;

    // Calculate the actual rectangle where the DEM is drawn within the label
    // This needs to account for the label's alignment and the pixmap size relative to the label size
    // We use the scaled DEM size (demWidth, demHeight) and the pan/zoom settings
    QSize scaledDemSize = QSize(demWidth, demHeight) * zoomLevel;

    // Calculate the top-left corner of the *entire displayed image* within the label widget
    // (this depends on label size, image size, and alignment)
    QPoint imageTopLeft = QPoint(0, 0);
    if (simDisplayLabel->alignment() & Qt::AlignHCenter) {
        imageTopLeft.setX((labelSize.width() - currentDEMImage.width()) / 2);
    }
    if (simDisplayLabel->alignment() & Qt::AlignVCenter) {
        imageTopLeft.setY((labelSize.height() - currentDEMImage.height()) / 2);
    }

    // Calculate the top-left corner of the actual DEM area within the displayed image
    // This uses the margins defined in getDEMPreviewImage
    QPoint demAreaInImageTopLeft = QPoint(rulerMargin, topMargin);

    // Calculate the top-left corner of the visible, zoomed/panned DEM rectangle relative to the label's origin
    QPoint visibleDemTopLeft = imageTopLeft + demAreaInImageTopLeft + panOffset;

    // Adjust the click position relative to the top-left of the visible DEM area
    QPoint clickRelativeToDem = pos - visibleDemTopLeft;
    qDebug() << "Click relative to visible DEM top-left:" << clickRelativeToDem;

    // Check if the click is within the visible *scaled* DEM area
    if (clickRelativeToDem.x() < 0 || clickRelativeToDem.x() >= scaledDemSize.width() ||
        clickRelativeToDem.y() < 0 || clickRelativeToDem.y() >= scaledDemSize.height()) {
        qDebug() << "Click outside the scaled DEM image area";
        return;
    }

    // Convert the click position to coordinates within the original, unscaled DEM area
    double unscaledX = clickRelativeToDem.x() / zoomLevel;
    double unscaledY = clickRelativeToDem.y() / zoomLevel;
    qDebug() << "Click relative to unscaled DEM top-left:" << unscaledX << unscaledY;

    // Convert to DEM grid coordinates (i,j) accounting for scale
    int gridI = int(unscaledY / scale);
    int gridJ = int(unscaledX / scale);

    qDebug() << "Calculated Grid coordinates (i,j):" << gridI << gridJ;
    qDebug() << "Grid dimensions (nx,ny):" << simEngine->getGridHeight() << simEngine->getGridWidth();

    // Ensure grid coordinates are valid
    if (gridI < 0 || gridI >= simEngine->getGridHeight() || gridJ < 0 || gridJ >= simEngine->getGridWidth()) {
        qDebug() << "Grid coordinates out of bounds";
        return;
    }

    // Check if cell is already selected
    QPoint newCell(gridI, gridJ);
    bool alreadySelected = false;

    qDebug() << "Current manualOutletCells count before update:" << manualOutletCells.size();

    // Use QVector::indexOf to find the cell more efficiently
    int existingIndex = manualOutletCells.indexOf(newCell);

    if (existingIndex != -1) {
        // Remove the cell if already selected
        qDebug() << "Cell" << newCell << "already selected at index" << existingIndex << ", removing";
        manualOutletCells.remove(existingIndex);
        alreadySelected = true;
    }

    // Add the cell if not already selected
    if (!alreadySelected) {
        qDebug() << "Adding new cell at:" << gridI << gridJ;
        manualOutletCells.append(newCell);
    }

    qDebug() << "manualOutletCells count after update:" << manualOutletCells.size();

    // Update engine with the new outlets
    simEngine->setManualOutletCells(manualOutletCells);

    // Update the display
    showDEMPreview(); // This regenerates the image with updated outlets
}

void MainWindow::onClearOutlets()
{
    manualOutletCells.clear();
    clearOutletsButton->setEnabled(false);

    QMessageBox::information(this, "Outlets Cleared",
                           "All manually selected outlet cells have been cleared.");

    // Update engine with empty outlets
    simEngine->setManualOutletCells(manualOutletCells);

    // We need a fresh DEM preview image without any markers
    // This is only called when explicitly clearing all outlets, so a refresh is acceptable
    QImage freshImg = simEngine->getDEMPreviewImage();
    if (!freshImg.isNull()) {
        // Save current zoom and pan
        float currentZoom = zoomLevel;
        QPoint currentPan = panOffset;

        // Update the image
        currentDEMImage = freshImg;

        // Restore zoom and pan
        zoomLevel = currentZoom;
        panOffset = currentPan;

        // Update display
        updateDEMDisplay();

        // Update status
        outputLabel->setText("No outlet cells selected. Click anywhere on the DEM to select outlets. Drag to pan, scroll to zoom.");
    }
}

void MainWindow::onSaveResults()
{
    // Only check if there's no simulation engine
    if (!simEngine)
        return;

    QString fileName = QFileDialog::getSaveFileName(this, "Save Simulation Results",
                                                   QDir::homePath() + "/simulation_results.csv",
                                                   "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)");
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);

        // Determine if we're using CSV format
        bool useCsvFormat = fileName.toLower().endsWith(".csv");
        QString separator = useCsvFormat ? "," : "\t";

        // Write simulation parameters
        out << "# SIMULATION PARAMETERS" << Qt::endl;

        // For CSV, include headers
        if (useCsvFormat) {
            out << "Parameter" << separator << "Value" << separator << "Unit" << Qt::endl;
        }

        out << "Simulation Time" << separator << totalTimeEdit->value() << separator << "seconds" << Qt::endl;
        out << "Cell Resolution" << separator << resolutionEdit->value() << separator << "m" << Qt::endl;
        out << "Manning's Coefficient" << separator << manningCoeffEdit->value() << separator << "" << Qt::endl;
        out << "Infiltration Rate" << separator << infiltrationEdit->value() << separator << "m/s" << Qt::endl;
        out << "Min Water Depth Threshold" << separator << minDepthEdit->value() << separator << "m" << Qt::endl;

        // Add info about rainfall mode
        if (timeVaryingRainfallCheckbox->isChecked()) {
            out << "Rainfall Mode" << separator << "Time-varying" << separator << "" << Qt::endl;

            // Write the rainfall schedule
            out << Qt::endl << "# RAINFALL SCHEDULE" << Qt::endl;
            if (useCsvFormat) {
                out << "Time (s)" << separator << "Rainfall Rate (m/s)" << Qt::endl;
            }

            QVector<QPair<double, double>> schedule = simEngine->getRainfallSchedule();
            for (const auto& entry : schedule) {
                out << entry.first << separator << entry.second << Qt::endl;
            }
        } else {
            out << "Rainfall Mode" << separator << "Constant" << separator << "" << Qt::endl;
            out << "Rainfall Rate" << separator << rainfallEdit->value() << separator << "m/s" << Qt::endl;
        }

        // Write total drainage volume
        out << Qt::endl << "# TOTAL DRAINAGE VOLUME" << Qt::endl;
        if (useCsvFormat) {
            out << "Total Drainage (m³)" << Qt::endl;
        }
        out << simEngine->getTotalDrainage() << Qt::endl;

        // Write time series data
        out << Qt::endl << "# TIME SERIES DATA" << Qt::endl;
        if (useCsvFormat) {
            out << "Time (s)" << separator << "Cumulative Drainage (m³)" << Qt::endl;
        }

        QVector<QPair<double, double>> timeSeries = simEngine->getDrainageTimeSeries();
        for (const auto& entry : timeSeries) {
            out << entry.first << separator << entry.second << Qt::endl;
        }

        // Get per-outlet drainage data
        QMap<QPoint, double> outletDrainage = simEngine->getPerOutletDrainage();

        // Write per-outlet drainage analysis
        out << Qt::endl << "# PER-OUTLET DRAINAGE DATA" << Qt::endl;
        if (useCsvFormat) {
            out << "Row (i)" << separator << "Column (j)" << separator << "Drainage Volume (m³)" << Qt::endl;
        }

        // Sort outlets by drainage volume (descending)
        QVector<QPair<QPoint, double>> sortedOutlets;

        // Manually iterate through the map
        for (auto it = outletDrainage.begin(); it != outletDrainage.end(); ++it) {
            if (it.value() > 0.0) { // Only include outlets with actual drainage
                sortedOutlets.append(qMakePair(it.key(), it.value()));
            }
        }

        // Sort outlets by drainage volume (descending)
        std::sort(sortedOutlets.begin(), sortedOutlets.end(),
                 [](const QPair<QPoint, double>& a, const QPair<QPoint, double>& b) {
                     return a.second > b.second; // Descending order
                 });

        // Write sorted outlets
        for (const auto& outlet : sortedOutlets) {
            out << outlet.first.x() << separator << outlet.first.y() << separator << outlet.second << Qt::endl;
        }

        file.close();
        QMessageBox::information(this, "Save Complete",
                               QString("Results saved to %1").arg(fileName));
    }
    else
    {
        QMessageBox::warning(this, "Save Error",
                           QString("Failed to save results to %1: %2").arg(fileName, file.errorString()));
    }
}

void MainWindow::zoomIn()
{
    zoomDisplay(simDisplayLabel, true);
}

void MainWindow::zoomOut()
{
    zoomDisplay(simDisplayLabel, false);
}

void MainWindow::resetView()
{
    if (!currentDEMImage.isNull()) {
        resetDisplayView(simDisplayLabel, outputLabel, "View reset to default (100% zoom)");
    }
}

void MainWindow::resetVisualizationView()
{
    if (!currentSimulationImage.isNull()) {
        resetDisplayView(resultDisplayLabel, resultsOutputLabel, "View reset to default (100% zoom)");
    }
}

void MainWindow::zoomDisplay(QLabel* displayLabel, bool zoomIn)
{
    QImage* targetImage = (displayLabel == simDisplayLabel) ? &currentDEMImage : &currentSimulationImage;

    if (!targetImage->isNull()) {
        if (zoomIn) {
            if (zoomLevel < 5.0f) {
                zoomLevel *= 1.2f;
            }
        } else {
            if (zoomLevel > 0.2f) {
                zoomLevel /= 1.2f;
            }
        }

        // Update the appropriate display
        if (displayLabel == simDisplayLabel) {
            updateDEMDisplay();
        } else if (displayLabel == resultDisplayLabel) {
            updateVisualization();
        }
    }
}

void MainWindow::zoomVisualization(int delta)
{
    if (delta > 0) {
        zoomDisplay(resultDisplayLabel, true);
    } else {
        zoomDisplay(resultDisplayLabel, false);
    }
}

void MainWindow::panDisplay(const QPoint& delta)
{
    panOffset += delta;

    // Determine which display to update based on the current tab
    int currentIndex = mainTabWidget->currentIndex();
    if (currentIndex == 1) { // DEM Preview tab
        updateDEMDisplay();
    } else if (currentIndex == 2) { // Visualization tab
        updateVisualization();
    }
}

void MainWindow::panView(const QPoint& delta)
{
    panDisplay(delta);
}

void MainWindow::panVisualization(QPoint delta)
{
    panDisplay(delta);
}

void MainWindow::onResultDisplayClicked(QPoint pos)
{
    // Handle clicks on the result display
    // This could be used to show information about the clicked area
    if (!currentSimulationImage.isNull()) {
        // For now, just show a message in the results output label
        resultsOutputLabel->setText(QString("Clicked at position: (%1, %2)")
                                   .arg(pos.x()).arg(pos.y()));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Only update visualization if we're on the visualization tab and have simulation results
    if (mainTabWidget->currentIndex() == 2 && !currentSimulationImage.isNull()) {
        updateVisualization();
    }
}

void MainWindow::updateDisplay(QLabel* displayLabel, const QImage& image, QLabel* statusLabel, const QString& statusPrefix, QPainter* customPainter)
{
    if (image.isNull())
        return;

    // Store the current scrollbar positions
    QScrollArea* scrollArea = qobject_cast<QScrollArea*>(displayLabel->parent()->parent());
    QWidget* container = displayLabel->parentWidget();
    QPoint scrollPos;

    // Get scroll position before any changes
    if (scrollArea) {
        scrollPos = QPoint(scrollArea->horizontalScrollBar()->value(),
                         scrollArea->verticalScrollBar()->value());

        // First, ensure scrollbars are always visible to prevent flickering
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }

    // Create a pixmap to display the image with zoom and pan
    QSize displaySize = image.size();
    if (zoomLevel != 1.0f) {
        displaySize = QSize(int(image.width() * zoomLevel), int(image.height() * zoomLevel));
    }

    // Scale the image according to zoom level
    QPixmap scaledPixmap = QPixmap::fromImage(image).scaled(
        displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Create a display pixmap large enough to hold the scaled image plus pan offset
    QPixmap displayPixmap(scaledPixmap.size());
    displayPixmap.fill(Qt::lightGray); // Fill with background color

    QPainter painter(&displayPixmap);

    // Apply pan offset to the drawing
    QPoint drawPos = QPoint(0, 0) + panOffset;
    painter.drawPixmap(drawPos, scaledPixmap);

    // Allow custom painting on top of the image
    if (customPainter) {
        customPainter->begin(&displayPixmap);

        // Custom painter can draw additional elements here

        customPainter->end();
    }

    // If this is the DEM preview, draw manual outlet markers
    if (displayLabel == simDisplayLabel && !manualOutletCells.isEmpty()) {
        // Draw selected cells with a marker (red circle)
        painter.setPen(QPen(Qt::red, 2));
        painter.setBrush(QBrush(QColor(255, 0, 0, 100)));  // Semi-transparent red

        for (const QPoint& cell : manualOutletCells) {
            // Convert cell coordinates to pixel coordinates based on zoom level
            int x = int(cell.y() * zoomLevel) + panOffset.x();  // Note: y coordinate is horizontal in the grid
            int y = int(cell.x() * zoomLevel) + panOffset.y();  // Note: x coordinate is vertical in the grid

            // Only draw if the point is visible in the viewport
            if (x >= 0 && x < displayPixmap.width() && y >= 0 && y < displayPixmap.height()) {
                painter.drawEllipse(QPoint(x, y), 6, 6);  // 12px diameter circle
            }
        }
    }

    painter.end();

    // Set the pixmap to the label
    displayLabel->setAlignment(Qt::AlignCenter);
    displayLabel->setPixmap(displayPixmap);

    // Store current pixmap size for DEM preview
    if (displayLabel == simDisplayLabel) {
        currentPixmapSize = scaledPixmap.size();
    }

    // Use static QSize to track container sizes
    static QSize demContainerSize;
    static QSize vizContainerSize;
    QSize& lastContainerSize = (displayLabel == simDisplayLabel) ? demContainerSize : vizContainerSize;

    if (container) {
        // Only resize if the container size has changed significantly
        // Use a large threshold (50px) to prevent frequent resizing
        if (abs(lastContainerSize.width() - displaySize.width()) > 50 ||
            abs(lastContainerSize.height() - displaySize.height()) > 50)
        {
            // Disable automatic updates during resizing
            container->setUpdatesEnabled(false);

            // Set a minimum size that accommodates the image
            container->setMinimumSize(displaySize);
            container->resize(displaySize);

            lastContainerSize = displaySize;

            // Manually force an update of the scroll area layout
            if (scrollArea) {
                scrollArea->updateGeometry();

                // Ensure the container is properly positioned
                container->updateGeometry();
            }

            // Re-enable updates
            container->setUpdatesEnabled(true);
        }
    }

    // Restore scroll position
    if (scrollArea) {
        QApplication::processEvents(); // Process pending layout changes

        // Restore the scrollbar positions
        scrollArea->horizontalScrollBar()->setValue(scrollPos.x());
        scrollArea->verticalScrollBar()->setValue(scrollPos.y());
    }

    // Update status text
    QString statusText = QString("%1 | Zoom: %2% | Pan: (%3, %4)")
                            .arg(statusPrefix)
                            .arg(int(zoomLevel * 100))
                            .arg(panOffset.x())
                            .arg(panOffset.y());

    // Add outlet count for DEM preview tab
    if (displayLabel == simDisplayLabel && !manualOutletCells.isEmpty()) {
        statusText += QString(" | %1 outlet(s) selected").arg(manualOutletCells.size());
    }

    if (statusLabel) {
        statusLabel->setText(statusText);
    }
}

void MainWindow::resetDisplayView(QLabel* displayLabel, QLabel* statusLabel, const QString& statusMessage)
{
    QScrollArea* scrollArea = qobject_cast<QScrollArea*>(displayLabel->parent()->parent());
    QWidget* container = displayLabel->parentWidget();

    // Block signals and updates to prevent flicker
    if (scrollArea) {
        scrollArea->setUpdatesEnabled(false);
    }
    if (container) {
        container->setUpdatesEnabled(false);
    }

    // Reset zoom level to 1.0 (100%)
    zoomLevel = 1.0f;

    // Reset pan offset to center
    panOffset = QPoint(0, 0);

    // Ensure scrollbars are always visible to prevent flickering
    // when they appear/disappear
    if (scrollArea) {
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        // Reset scroll positions to top-left
        scrollArea->horizontalScrollBar()->setValue(0);
        scrollArea->verticalScrollBar()->setValue(0);
    }

    // Now update the display with the reset settings
    if (displayLabel == simDisplayLabel) {
        updateDEMDisplay();
    } else if (displayLabel == resultDisplayLabel) {
        updateVisualization();
    }

    // Re-enable updates after the display has been updated
    if (container) {
        container->setUpdatesEnabled(true);
    }
    if (scrollArea) {
        scrollArea->setUpdatesEnabled(true);

        // Process any pending layout events before continuing
        QApplication::processEvents();
    }

    // Provide feedback
    if (statusLabel) {
        statusLabel->setText(statusMessage);
    }
}

void MainWindow::updateDEMDisplay()
{
    updateDisplay(simDisplayLabel, currentDEMImage, outputLabel, "DEM visualization", nullptr);
}

void MainWindow::updateVisualization()
{
    if (!simEngine) {
        qDebug() << "ERROR: Cannot update visualization - simEngine is null";
        return;
    }

    // If we already have a valid current simulation image, use it
    if (currentSimulationImage.isNull()) {
        qDebug() << "Getting water depth image from simulation engine";
        // Get the water depth image from the simulation engine
        QImage img = simEngine->getWaterDepthImage();

        if (img.isNull()) {
            qDebug() << "ERROR: Water depth image is NULL in updateVisualization()";

            // If we still don't have an image, try to get the DEM preview image as a fallback
            img = simEngine->getDEMPreviewImage();
            if (!img.isNull()) {
                qDebug() << "Using DEM preview image as fallback, size:" << img.size();
                currentSimulationImage = img;
            } else {
                qDebug() << "Failed to get any valid image for visualization";
                resultsOutputLabel->setText("Error: Could not generate any visualization image");
        return;
            }
        } else {
            qDebug() << "Got valid water depth image, size:" << img.size();
            // Store the image for future reference
            currentSimulationImage = img;
        }
    }

    // Display information
    double drainageVol = simEngine->getTotalDrainage();
    double elapsedTime = simEngine->getCurrentTime();
    double totalTime = simEngine->getTotalTime();

    QString statusPrefix = QString("Simulation Progress: %1 / %2 s, Total Drainage: %3 m^3")
                         .arg(elapsedTime, 0, 'f', 1)
                         .arg(totalTime, 0, 'f', 1)
                         .arg(drainageVol, 0, 'f', 3);

    qDebug() << "Updating display with image size:" << currentSimulationImage.size();

    // Update the display with the image
    updateDisplay(resultDisplayLabel, currentSimulationImage, resultsOutputLabel, statusPrefix, nullptr);

    // Update the outlet table with current drainage data
    updateOutletTable();
}

void MainWindow::updateOutletTable()
{
    if (!simEngine)
        return;

    qDebug() << "Updating outlet table";

    // Clear the existing table
    outletTableWidget->setRowCount(0);

    // Get outlet cells and per-outlet drainage data
    QVector<QPoint> outletCells;
    if (manualOutletSelectionMode) {
        outletCells = simEngine->getManualOutletCells();
        qDebug() << "Using manual outlet cells:" << outletCells.size();
    } else {
        // For automatic outlets, we need to get the complete list from the engine
        outletCells = simEngine->getAutomaticOutletCells(); // Get automatic outlet cells
        qDebug() << "Using automatic outlet cells:" << outletCells.size();

        // In the future, we might want to expose automatic outlet cells as well
        // For now, we'll just show manual outlets in the table
    }

    // Get the drainage data for each outlet
    QMap<QPoint, double> outletDrainage = simEngine->getPerOutletDrainage();
    qDebug() << "Outlet drainage data contains" << outletDrainage.size() << "entries";

    // Populate the table with outlet data
    int row = 0;
    double totalDrainage = 0.0;

    // First add all outlets with non-zero drainage
    for (const QPoint &point : outletCells) {
        double drainage = outletDrainage.value(point, 0.0);
        totalDrainage += drainage;

        if (drainage > 0.0) {
            qDebug() << "Outlet at" << point.x() << point.y() << "has drainage:" << drainage;

            outletTableWidget->insertRow(row);
            outletTableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(point.x())));
            outletTableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(point.y())));
            outletTableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(drainage, 'f', 4)));

            // Color the row based on drainage volume (more drainage = more intense blue)
            double maxDrainage = simEngine->getTotalDrainage();
            if (maxDrainage > 0.0) {
                double ratio = drainage / maxDrainage;
                int blue = 128 + int(127 * ratio); // Scale from 128-255 for better visibility
                for (int col = 0; col < outletTableWidget->columnCount(); col++) {
                    outletTableWidget->item(row, col)->setBackground(QColor(240, 240, blue));
                    outletTableWidget->item(row, col)->setForeground(Qt::black); // Set text color to black
                }
            }

            row++;
        }
    }

    // Then add outlets with zero drainage
    for (const QPoint &point : outletCells) {
        double drainage = outletDrainage.value(point, 0.0);
        if (drainage <= 0.0) {
            qDebug() << "Outlet at" << point.x() << point.y() << "has zero drainage";

            outletTableWidget->insertRow(row);
            outletTableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(point.x())));
            outletTableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(point.y())));
            outletTableWidget->setItem(row, 2, new QTableWidgetItem("0.0000"));

            // Set black text color for zero drainage cells
            for (int col = 0; col < outletTableWidget->columnCount(); col++) {
                outletTableWidget->item(row, col)->setForeground(Qt::black);
            }

            row++;
        }
    }

    qDebug() << "Total drainage from all outlets:" << totalDrainage;

    // Sort the table by drainage (descending)
    outletTableWidget->sortItems(2, Qt::DescendingOrder);

    // Resize columns to contents
    outletTableWidget->resizeColumnsToContents();
}

void MainWindow::onToggleGrid(bool checked)
{
    // First handle the 2D visualization (existing implementation)
    if (simEngine) {
        simEngine->setShowGrid(checked);

        // Update both the DEM preview and the simulation display
        updateVisualization();

        // If in manual outlet selection mode, also update the DEM preview
        if (manualOutletSelectionMode) {
            showDEMPreview();
        }
    }

    // Now also handle the 3D VTK visualization
    if (vtkTerrainVisualizer) {
        vtkTerrainVisualizer->setGridVisible(checked);
    }
}

void MainWindow::onToggleRulers(bool checked)
{
    // First handle the 2D visualization (existing implementation)
    if (simEngine) {
        simEngine->setShowRulers(checked);

        // Update both the DEM preview and the simulation display
        updateVisualization();

        // If in manual outlet selection mode, also update the DEM preview
        if (manualOutletSelectionMode) {
            showDEMPreview();
        }
    }

    // Now also handle the 3D VTK visualization
    if (vtkTerrainVisualizer) {
        vtkTerrainVisualizer->setRulersVisible(checked);
    }
}

void MainWindow::onGridIntervalChanged(int value)
{
    // First handle the 2D visualization (existing implementation)
    if (simEngine) {
        simEngine->setGridInterval(value);

        // Update both the DEM preview and the simulation display
        updateVisualization();

        // If in manual outlet selection mode, also update the DEM preview
        if (manualOutletSelectionMode) {
            showDEMPreview();
        }
    }

    // Now also handle the 3D VTK visualization
    if (vtkTerrainVisualizer) {
        vtkTerrainVisualizer->setGridInterval(value);
    }
}

void MainWindow::onTimeVaryingRainfallToggled(bool checked)
{
    // Enable/disable constant rainfall field
    rainfallEdit->setEnabled(!checked);

    // If turning on time-varying rainfall, populate the first row with current constant value
    if (checked && rainfallTableWidget->rowCount() == 0) {
        onAddRainfallRow();
    }

    // Update the simulation engine
    if (simEngine) {
        simEngine->setTimeVaryingRainfall(checked);
        updateRainfallSchedule();
    }

    // Switch to the rainfall configuration tab when enabled
    if (checked) {
        // Find the index of the rainfall configuration tab
        int rainfallTabIndex = -1;
        for (int i = 0; i < mainTabWidget->count(); i++) {
            if (mainTabWidget->tabText(i).contains("Rainfall", Qt::CaseInsensitive)) {
                rainfallTabIndex = i;
                break;
            }
        }

        if (rainfallTabIndex >= 0) {
            mainTabWidget->setCurrentIndex(rainfallTabIndex);
        }
    }
}

void MainWindow::onAddRainfallRow()
{
    int row = rainfallTableWidget->rowCount();
    rainfallTableWidget->insertRow(row);

    // Time cell - either 0 for first row or previous row time + interval for subsequent rows
    QTableWidgetItem *timeItem = new QTableWidgetItem();
    if (row == 0) {
        // First row always at time 0
        timeItem->setData(Qt::EditRole, 0);
    } else {
        // Calculate a reasonable time for the new row (e.g., increment by 10% of total time)
        double prevTime = rainfallTableWidget->item(row-1, 0)->text().toDouble();
        double increment = totalTimeEdit->value() * 0.1; // 10% of total time
        timeItem->setData(Qt::EditRole, prevTime + increment);
    }

    // Rainfall intensity cell - default to the current constant rate
    QTableWidgetItem *intensityItem = new QTableWidgetItem();
    intensityItem->setData(Qt::EditRole, rainfallEdit->value());

    // Set text alignment for better readability
    timeItem->setTextAlignment(Qt::AlignCenter);
    intensityItem->setTextAlignment(Qt::AlignCenter);

    rainfallTableWidget->setItem(row, 0, timeItem);
    rainfallTableWidget->setItem(row, 1, intensityItem);

    // Create special editors for the cells to ensure proper decimal precision
    QDoubleSpinBox* timeSpinBox = new QDoubleSpinBox(rainfallTableWidget);
    timeSpinBox->setDecimals(2);
    timeSpinBox->setMinimum(0.0);
    timeSpinBox->setMaximum(totalTimeEdit->value());
    timeSpinBox->setSingleStep(10.0);
    rainfallTableWidget->setCellWidget(row, 0, timeSpinBox);
    timeSpinBox->setValue(timeItem->data(Qt::EditRole).toDouble());

    QDoubleSpinBox* rainfallSpinBox = new QDoubleSpinBox(rainfallTableWidget);
    rainfallSpinBox->setDecimals(8);  // Allow very small values
    rainfallSpinBox->setMinimum(0.0);
    rainfallSpinBox->setMaximum(0.001);
    rainfallSpinBox->setSingleStep(0.0000001);
    rainfallTableWidget->setCellWidget(row, 1, rainfallSpinBox);
    rainfallSpinBox->setValue(intensityItem->data(Qt::EditRole).toDouble());

    // Connect the spinbox value changed signals to update the rainfall schedule
    connect(timeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateRainfallSchedule);
    connect(rainfallSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateRainfallSchedule);

    // Resize columns to content
    rainfallTableWidget->resizeColumnsToContents();

    updateRainfallSchedule();
}

void MainWindow::onRemoveRainfallRow()
{
    QList<QTableWidgetItem*> selectedItems = rainfallTableWidget->selectedItems();
    if (selectedItems.isEmpty()) return;

    // Get unique rows (avoiding duplicates when multiple cells in a row are selected)
    QSet<int> rowsToRemove;
    for (QTableWidgetItem* item : selectedItems) {
        rowsToRemove.insert(item->row());
    }

    // Convert QSet to std::vector first, then to QList for Qt 6 compatibility
    std::vector<int> rowsVector;
    for (int row : rowsToRemove) {
        rowsVector.push_back(row);
    }

    // Sort in descending order to remove from bottom to top
    std::sort(rowsVector.begin(), rowsVector.end(), std::greater<int>());

    // Remove the rows
    for (int row : rowsVector) {
        rainfallTableWidget->removeRow(row);
    }

    // Ensure at least one row remains
    if (rainfallTableWidget->rowCount() == 0) {
        onAddRainfallRow();
    }

    updateRainfallSchedule();
}

void MainWindow::updateRainfallSchedule()
{
    if (!simEngine) return;

    QVector<QPair<double, double>> schedule;

    for (int row = 0; row < rainfallTableWidget->rowCount(); row++) {
        // Get spinboxes from cell widgets
        QDoubleSpinBox* timeSpinBox = qobject_cast<QDoubleSpinBox*>(rainfallTableWidget->cellWidget(row, 0));
        QDoubleSpinBox* rateSpinBox = qobject_cast<QDoubleSpinBox*>(rainfallTableWidget->cellWidget(row, 1));

        if (timeSpinBox && rateSpinBox) {
            double time = timeSpinBox->value();
            double rate = rateSpinBox->value();

            schedule.append(qMakePair(time, rate));
        } else {
            // Fallback to table item if spinbox not found (shouldn't happen)
            QTableWidgetItem *timeItem = rainfallTableWidget->item(row, 0);
            QTableWidgetItem *rateItem = rainfallTableWidget->item(row, 1);

            if (timeItem && rateItem) {
                double time = timeItem->text().toDouble();
                double rate = rateItem->text().toDouble();

                schedule.append(qMakePair(time, rate));
            }
        }
    }

    // Sort by time
    std::sort(schedule.begin(), schedule.end(),
              [](const QPair<double, double>& a, const QPair<double, double>& b) {
                  return a.first < b.first;
              });

    // Update simulation engine
    simEngine->setRainfallSchedule(schedule);
}

void MainWindow::onResolutionChanged(double newResolution)
{
    // This function might need adjustment depending on whether resolution
    // is primarily driven by the UI or the loaded TIF file.
    // If TIF dictates resolution, maybe this spinbox should be read-only
    // or only used as an override/fallback.

    // Update the simulation engine with the new resolution
    if (simEngine) {
        simEngine->setCellResolution(newResolution);

        // If we have a loaded DEM, refresh the preview
        if (demFileLabel->text() != "No file selected" && !currentDEMImage.isNull()) {
            // Refresh the DEM preview with new resolution
            // Re-getting the image implicitly uses the new resolution set in simEngine
            showDEMPreview(); // This re-calls getDEMPreviewImage which uses the new resolution

            // Add resolution info to status
            QString message = QString("Resolution manually set to %1 m. Display adjusted.").arg(newResolution, 0, 'f', 3);
            outputLabel->setText(message);
        }
    }
}

// Connect to simulation engine signals
void MainWindow::setupSimulationEngineConnections()
{
    if (!simEngine) {
        return;
    }

    // Disconnect any existing connections first to avoid duplicates
    disconnect(simEngine, &SimulationEngine::simulationTimeUpdated, this, nullptr);
    disconnect(simEngine, &SimulationEngine::simulationStepCompleted, this, nullptr);

    // Connect time update signal
    connect(simEngine, &SimulationEngine::simulationTimeUpdated, this,
            [this](double currentTime, double totalTime) {
                timeElapsedLabel->setText(QString("Time: %1 / %2 s").arg(currentTime, 0, 'f', 1).arg(totalTime, 0, 'f', 1));
                int progress = int((currentTime / totalTime) * 100.0);
                simulationProgress->setValue(progress);
            });

    // Connect step completed signal
    connect(simEngine, &SimulationEngine::simulationStepCompleted, this,
            [this](const QImage& image) {
                if (!image.isNull()) {
                    updateDisplay(resultDisplayLabel, image, resultsOutputLabel, "Water visualization", nullptr);
                }
            });
}

void MainWindow::onSimulationStep()
{
    if (!simEngine || !simulationRunning) {
        return;
    }

    // Check if simulation is complete
    if (simEngine->getCurrentTime() >= simEngine->getTotalTime()) {
        simTimer->stop();
        simulationRunning = false;
        simulationPaused = false;
        updateUIState();

        // Show message that simulation is finished
        QMessageBox::information(this, "Simulation Complete",
            "The simulation has completed. You can now view the results or save them.");

        return;
    }

    // Step the simulation
    simEngine->stepSimulation();

    // Update the visualization
    updateVisualization();

    // Update progress bar
    double progress = simEngine->getCurrentTime() / simEngine->getTotalTime() * 100.0;
    simulationProgress->setValue(static_cast<int>(progress));

    // Update time elapsed label
    timeElapsedLabel->setText(QString("Time: %1 / %2 seconds")
                             .arg(simEngine->getCurrentTime(), 0, 'f', 1)
                             .arg(simEngine->getTotalTime(), 0, 'f', 1));

    // Update drainage volume label
    drainageVolumeLabel->setText(QString("Total Drainage: %1 m³")
                                .arg(simEngine->getTotalDrainage(), 0, 'f', 3));

    // Update the VTK water visualization
    if (vtkWaterVisualizer) {
        vtkWaterVisualizer->updateWaterDepth();

        // If camera sync is enabled, update the water camera
        if (syncCamerasCheckbox && syncCamerasCheckbox->isChecked()) {
            vtkWaterVisualizer->syncCameraWithTerrain();
        }
    }
}

void MainWindow::onStartSimulation()
{
    // Make sure simulation engine exists
    if (!simEngine) {
        QMessageBox::warning(this, "Error", "Simulation engine not initialized. Please load a DEM file first.");
        return;
    }

    // Check if we have a DEM loaded
    if (currentDEMImage.isNull()) {
        QMessageBox::warning(this, "Error", "No Digital Elevation Model loaded. Please load a DEM file first.");
        return;
    }

    // Set simulation state flags
    simulationRunning = true;
    simulationPaused = false;

    // Initialize simulation with current parameters
    // Set the simulation parameters from UI
    simEngine->setManningCoefficient(manningCoeffEdit->value());
    simEngine->setInfiltrationRate(infiltrationEdit->value());
    simEngine->setMinWaterDepth(minDepthEdit->value());
    simEngine->setTotalTime(totalTimeEdit->value());

    // Handle rainfall configuration - either constant or time-varying
    if (timeVaryingRainfallCheckbox->isChecked()) {
        simEngine->setTimeVaryingRainfall(true);
        // Make sure the rainfall schedule is updated with the latest values
        updateRainfallSchedule();
    } else {
        simEngine->setTimeVaryingRainfall(false);
        simEngine->setRainfall(rainfallEdit->value());
    }

    // Handle outlet configuration
    if (outletMethodCombo->currentIndex() == 0) {
        // Automatic outlets based on percentile
        double percentile = outletPercentileEdit->value() / 100.0;
        simEngine->configureOutletsByPercentile(percentile);
    } else {
        // Manual outlets
        simEngine->setManualOutletCells(manualOutletCells);
    }

    // Initialize the simulation
    bool initSuccess = simEngine->initSimulation();

    if (!initSuccess) {
        QMessageBox::warning(this, "Simulation Error", "Failed to initialize the simulation. Please check your parameters.");
        simulationRunning = false;
        updateUIState();
        return;
    }

    // Update display options
    simEngine->setShowGrid(showGridCheckbox->isChecked());
    simEngine->setShowRulers(showRulersCheckbox->isChecked());
    simEngine->setGridInterval(gridIntervalSpinBox->value());

    // Save the current tab index before switching
    previousTabIndex = mainTabWidget->currentIndex();

    // Switch to the simulation results tab with animation
    int resultsTabIndex = mainTabWidget->indexOf(visualizationTab);
    if (resultsTabIndex >= 0) {
        mainTabWidget->setCurrentIndex(resultsTabIndex);

        // Flash the tab text briefly to indicate the switch
        QFont font = mainTabWidget->tabBar()->font();
        font.setBold(true);
        mainTabWidget->tabBar()->setTabTextColor(resultsTabIndex, Qt::blue);
        mainTabWidget->tabBar()->setFont(font);

        // Reset the tab appearance after a short delay
        QTimer::singleShot(1000, this, [this, resultsTabIndex]() {
            QFont normalFont = mainTabWidget->tabBar()->font();
            normalFont.setBold(false);
            mainTabWidget->tabBar()->setFont(normalFont);
            mainTabWidget->tabBar()->setTabTextColor(resultsTabIndex, palette().text().color());
        });
    }

    // Clear any existing simulation image
    currentSimulationImage = QImage();

    // Get initial water depth image
    QImage initialImage = simEngine->getWaterDepthImage();
    if (!initialImage.isNull()) {
        qDebug() << "Initial water depth image is valid, dimensions:" << initialImage.width() << "x" << initialImage.height();
        currentSimulationImage = initialImage;

        // Reset zoom and pan settings for visualization
        zoomLevel = 1.0f;
        panOffset = QPoint(0, 0);

        // Update the visualization display
        updateVisualization();
    } else {
        qDebug() << "ERROR: Initial water depth image is NULL!";
        QMessageBox::warning(this, "Visualization Error", "Failed to generate initial water depth visualization image.");
        simulationRunning = false;
        return;
    }

    // Update UI state after initialization
    updateUIState();

    // Start the simulation timers
    simTimer->start(50); // 50 ms interval
    uiUpdateTimer->start(200); // 200 ms for UI updates

    // Update progress bar range
    simulationProgress->setMaximum(100);
    simulationProgress->setValue(0);

    // Update labels
    timeElapsedLabel->setText(QString("Time: %1 / %2 s").arg(0.0, 0, 'f', 1).arg(simEngine->getTotalTime(), 0, 'f', 1));
    drainageVolumeLabel->setText(QString("Drainage: %1 m³").arg(0.0, 0, 'f', 3));

    // Update outputs
    outputLabel->setText("Simulation started and running.");
    resultsOutputLabel->setText("Simulation running - water depth visualization shown above");
}

void MainWindow::onPauseSimulation()
{
    if (!simulationRunning || simulationPaused)
        return;

    simulationPaused = true;
    simTimer->stop();
    updateUIState();

    // Update UI to show paused state
    pauseButton->setEnabled(false);
    startButton->setEnabled(true);
    resultsOutputLabel->setText("Simulation paused");
}

void MainWindow::onStopSimulation()
{
    if (!simulationRunning && !simulationPaused)
        return;

    simulationRunning = false;
    simulationPaused = false;
    simTimer->stop();

    // Reset simulation state
    bool resetSuccess = simEngine->initSimulation();
    if (!resetSuccess) {
        qDebug() << "Warning: Failed to fully reset simulation state";
    }

    currentSimulationImage = QImage();

    // Update UI
    updateUIState();
    resultsOutputLabel->setText("Simulation stopped");
    simulationProgress->setValue(0);
    timeElapsedLabel->setText("Time: 0 s");
    drainageVolumeLabel->setText("Drainage: 0 m³");

    // Reset the visualization
    resetVisualizationView();

    // Give the option to return to previous tab
    QTimer::singleShot(500, this, &MainWindow::returnToPreviousTab);
}

void MainWindow::returnToPreviousTab()
{
    // Only return if we have a valid previous tab
    if (previousTabIndex >= 0 && previousTabIndex < mainTabWidget->count() &&
        previousTabIndex != mainTabWidget->currentIndex()) {

        // Ask the user if they want to return to the previous tab
        QMessageBox msgBox;
        msgBox.setWindowTitle("Return to Previous Tab");
        msgBox.setText("Do you want to return to the previous tab?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        if (msgBox.exec() == QMessageBox::Yes) {
            // Flash the tab briefly before switching
            QFont font = mainTabWidget->tabBar()->font();
            font.setBold(true);
            mainTabWidget->tabBar()->setTabTextColor(previousTabIndex, Qt::blue);
            mainTabWidget->tabBar()->setFont(font);

            // Switch to previous tab
            mainTabWidget->setCurrentIndex(previousTabIndex);

            // Reset the tab appearance after a short delay
            QTimer::singleShot(1000, this, [this]() {
                for (int i = 0; i < mainTabWidget->count(); i++) {
                    QFont normalFont = mainTabWidget->tabBar()->font();
                    normalFont.setBold(false);
                    mainTabWidget->tabBar()->setFont(normalFont);
                    mainTabWidget->tabBar()->setTabTextColor(i, palette().text().color());
                }
            });
        }
    }
}

// Add the VTK panel creation methods after the existing creation methods

void MainWindow::createVTKTerrainPanel()
{
    qDebug() << "Creating VTK Terrain Panel - START";
    try {
        vtkTerrainTab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(vtkTerrainTab);

        qDebug() << "  Creating controls panel";
        // Create controls panel
        QGroupBox* controlsGroup = new QGroupBox("Terrain Visualization Controls");
        QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);

        qDebug() << "  Adding UI controls";
        // Add 3D view toggle
        view3DCheckbox = new QCheckBox("3D View");
        view3DCheckbox->setChecked(true);
        controlsLayout->addWidget(view3DCheckbox);

        // Add grid visibility toggle
        showGridCheckbox = new QCheckBox("Show Grid");
        showGridCheckbox->setChecked(true);
        controlsLayout->addWidget(showGridCheckbox);

        // Add rulers visibility toggle
        showRulersCheckbox = new QCheckBox("Show Rulers");
        showRulersCheckbox->setChecked(false);
        controlsLayout->addWidget(showRulersCheckbox);

        // Add grid interval spinner
        QHBoxLayout* gridIntervalLayout = new QHBoxLayout();
        QLabel* gridIntervalLabel = new QLabel("Grid Interval:");
        gridIntervalSpinBox = new QSpinBox();
        gridIntervalSpinBox->setRange(1, 100);
        gridIntervalSpinBox->setValue(10);
        gridIntervalSpinBox->setSuffix(" cells");
        gridIntervalLayout->addWidget(gridIntervalLabel);
        gridIntervalLayout->addWidget(gridIntervalSpinBox);
        controlsLayout->addLayout(gridIntervalLayout);

        // Camera sync checkbox
        syncCamerasCheckbox = new QCheckBox("Sync with Water View");
        syncCamerasCheckbox->setChecked(true);
        controlsLayout->addWidget(syncCamerasCheckbox);

        // Add a save view button
        saveVTKViewButton = new QPushButton("Save View as Image");
        controlsLayout->addWidget(saveVTKViewButton);

        // Add the controls to the main layout
        layout->addWidget(controlsGroup);

        // Create the VTK terrain visualizer
        qDebug() << "  Creating VTKTerrainVisualizer - this is where a crash might occur";
        vtkTerrainVisualizer = new VTKTerrainVisualizer();
        qDebug() << "  Successfully created VTKTerrainVisualizer";
        layout->addWidget(vtkTerrainVisualizer, 1);
        qDebug() << "  Added VTKTerrainVisualizer to layout";

        // Connect signals
        qDebug() << "  Connecting signals";
        connect(view3DCheckbox, &QCheckBox::toggled, this, &MainWindow::onToggle3DView);
        connect(showGridCheckbox, &QCheckBox::toggled, this, &MainWindow::onToggleGrid);
        connect(showRulersCheckbox, &QCheckBox::toggled, this, &MainWindow::onToggleRulers);
        connect(gridIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onGridIntervalChanged);
        connect(syncCamerasCheckbox, &QCheckBox::toggled, this, &MainWindow::onSyncCameras);
        connect(saveVTKViewButton, &QPushButton::clicked, this, &MainWindow::onSaveVTKView);

        qDebug() << "Creating VTK Terrain Panel - COMPLETE";
    } catch (const std::exception& e) {
        qDebug() << "Exception in createVTKTerrainPanel:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in createVTKTerrainPanel";
    }
}

void MainWindow::createVTKWaterPanel()
{
    qDebug() << "Creating VTK Water Panel - START";
    try {
        vtkWaterTab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(vtkWaterTab);

        qDebug() << "  Creating controls panel";
        // Create controls panel
        QGroupBox* controlsGroup = new QGroupBox("Water Visualization Controls");
        QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);

        qDebug() << "  Adding UI controls";
        // Water opacity slider
        QHBoxLayout* opacityLayout = new QHBoxLayout();
        QLabel* opacityLabel = new QLabel("Water Opacity:");
        waterOpacitySlider = new QSlider(Qt::Horizontal);
        waterOpacitySlider->setRange(0, 100);
        waterOpacitySlider->setValue(80);
        opacityLayout->addWidget(opacityLabel);
        opacityLayout->addWidget(waterOpacitySlider);
        controlsLayout->addLayout(opacityLayout);

        // Vertical exaggeration control
        QHBoxLayout* exaggerationLayout = new QHBoxLayout();
        QLabel* exaggerationLabel = new QLabel("Vertical Exaggeration:");
        verticalExaggerationSpinBox = new QDoubleSpinBox();
        verticalExaggerationSpinBox->setRange(0.1, 10.0);
        verticalExaggerationSpinBox->setValue(1.0);
        verticalExaggerationSpinBox->setSingleStep(0.1);
        exaggerationLayout->addWidget(exaggerationLabel);
        exaggerationLayout->addWidget(verticalExaggerationSpinBox);
        controlsLayout->addLayout(exaggerationLayout);

        // Flow vectors toggle
        showFlowVectorsCheckbox = new QCheckBox("Show Flow Vectors");
        showFlowVectorsCheckbox->setChecked(false);
        controlsLayout->addWidget(showFlowVectorsCheckbox);

        // Water surface smoothing toggle
        waterSmoothingCheckbox = new QCheckBox("Smooth Water Surface");
        waterSmoothingCheckbox->setChecked(true);
        controlsLayout->addWidget(waterSmoothingCheckbox);

        // Max water depth display
        QHBoxLayout* depthLayout = new QHBoxLayout();
        QLabel* depthLabel = new QLabel("Max Water Depth:");
        maxWaterDepthLabel = new QLabel("0.00 m");
        maxWaterDepthLabel->setAlignment(Qt::AlignRight);
        maxWaterDepthLabel->setMinimumWidth(80);
        depthLayout->addWidget(depthLabel);
        depthLayout->addWidget(maxWaterDepthLabel);
        controlsLayout->addLayout(depthLayout);

        // Add the controls to the main layout
        layout->addWidget(controlsGroup);

        // Create the VTK water visualizer
        qDebug() << "  Creating VTKWaterVisualizer - this is where a crash might occur";
        vtkWaterVisualizer = new VTKWaterVisualizer();
        qDebug() << "  Successfully created VTKWaterVisualizer";
        layout->addWidget(vtkWaterVisualizer, 1);
        qDebug() << "  Added VTKWaterVisualizer to layout";

        // Connect signals
        qDebug() << "  Connecting signals";
        connect(waterOpacitySlider, &QSlider::valueChanged, this, &MainWindow::onWaterOpacityChanged);
        connect(verticalExaggerationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onVerticalExaggerationChanged);
        connect(showFlowVectorsCheckbox, &QCheckBox::toggled, this, &MainWindow::onShowFlowVectorsToggled);
        connect(waterSmoothingCheckbox, &QCheckBox::toggled, this, &MainWindow::onWaterSurfaceSmoothingToggled);
        connect(vtkWaterVisualizer, &VTKWaterVisualizer::waterDepthUpdated, this, &MainWindow::onWaterDepthUpdated);

        qDebug() << "Creating VTK Water Panel - COMPLETE";
    } catch (const std::exception& e) {
        qDebug() << "Exception in createVTKWaterPanel:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in createVTKWaterPanel";
    }
}

// Implement the VTK-specific slot methods

void MainWindow::onToggle3DView(bool enabled)
{
    if (vtkTerrainVisualizer) {
        vtkTerrainVisualizer->set3DViewEnabled(enabled);
    }
}

void MainWindow::onWaterOpacityChanged(int value)
{
    if (vtkWaterVisualizer) {
        double opacity = value / 100.0; // Convert from percentage to 0-1 range
        vtkWaterVisualizer->setWaterOpacity(opacity);
    }
}

void MainWindow::onVerticalExaggerationChanged(double value)
{
    if (vtkWaterVisualizer) {
        vtkWaterVisualizer->setVerticalExaggeration(value);
    }

    // Check if terrain visualizer supports vertical exaggeration
    if (vtkTerrainVisualizer) {
        // If there's a method in VTKTerrainVisualizer called setVerticalExaggeration, uncomment this
        // vtkTerrainVisualizer->setVerticalExaggeration(value);
    }
}

void MainWindow::onShowFlowVectorsToggled(bool checked)
{
    if (vtkWaterVisualizer) {
        vtkWaterVisualizer->setShowFlowVectors(checked);
    }
}

void MainWindow::onWaterSurfaceSmoothingToggled(bool checked)
{
    if (vtkWaterVisualizer) {
        vtkWaterVisualizer->setWaterSurfaceSmoothing(checked);
    }
}

void MainWindow::onWaterDepthUpdated(double maxDepth)
{
    maxWaterDepthLabel->setText(QString("%1 m").arg(maxDepth, 0, 'f', 2));
}

void MainWindow::onSaveVTKView()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save 3D View", QDir::homePath(), "PNG Images (*.png);;JPEG Images (*.jpg)");

    if (fileName.isEmpty()) {
        return;
    }

    // Determine which visualizer to save based on current tab
    if (mainTabWidget->currentWidget() == vtkTerrainTab && vtkTerrainVisualizer) {
        // For terrain tab, grab the VTK render window
        vtkRenderWindow *renderWindow = vtkTerrainVisualizer->renderWindow();

        vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter =
            vtkSmartPointer<vtkWindowToImageFilter>::New();
        windowToImageFilter->SetInput(renderWindow);
        windowToImageFilter->SetInputBufferTypeToRGBA();
        windowToImageFilter->ReadFrontBufferOff();
        windowToImageFilter->Update();

        vtkSmartPointer<vtkPNGWriter> writer =
            vtkSmartPointer<vtkPNGWriter>::New();
        writer->SetFileName(fileName.toStdString().c_str());
        writer->SetInputConnection(windowToImageFilter->GetOutputPort());
        writer->Write();

        statusBar()->showMessage("Terrain view saved to " + fileName, 3000);
    }
    else if (mainTabWidget->currentWidget() == vtkWaterTab && vtkWaterVisualizer) {
        // For water tab, grab the VTK render window
        vtkRenderWindow *renderWindow = vtkWaterVisualizer->renderWindow();

        vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter =
            vtkSmartPointer<vtkWindowToImageFilter>::New();
        windowToImageFilter->SetInput(renderWindow);
        windowToImageFilter->SetInputBufferTypeToRGBA();
        windowToImageFilter->ReadFrontBufferOff();
        windowToImageFilter->Update();

        vtkSmartPointer<vtkPNGWriter> writer =
            vtkSmartPointer<vtkPNGWriter>::New();
        writer->SetFileName(fileName.toStdString().c_str());
        writer->SetInputConnection(windowToImageFilter->GetOutputPort());
        writer->Write();

        statusBar()->showMessage("Water view saved to " + fileName, 3000);
    }
}

void MainWindow::onSyncCameras(bool checked)
{
    if (checked && vtkWaterVisualizer && vtkTerrainVisualizer) {
        vtkWaterVisualizer->syncCameraWithTerrain();
    }
}

void MainWindow::onTerrainClicked(QPoint position)
{
    if (!simEngine) return;

    // Get grid coordinates
    int gridX = position.x();
    int gridY = position.y();

    // Calculate world coordinates (if needed)
    double worldX = gridX * simEngine->getCellResolution();
    double worldY = gridY * simEngine->getCellResolution();

    // Get elevation if possible
    double elevation = 0.0;
    if (simEngine) {
        // If there's a method to get elevation, use it
        // elevation = simEngine->getDEMValue(gridX, gridY);
    }

    // Update UI with information
    this->statusBar()->showMessage(QString("Selected point at grid (%1, %2), coordinates: (%3, %4) m")
        .arg(gridX).arg(gridY).arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2), 5000);

    // Update display if needed
    updateVisualization();
}

void MainWindow::createControlPanel()
{
    // Create a group box for simulation controls
    QGroupBox* controlGroup = new QGroupBox("Simulation Controls", inputTab);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);

    // Create buttons for simulation flow control
    startButton = new QPushButton("Start Simulation", controlGroup);
    pauseButton = new QPushButton("Pause", controlGroup);
    stopButton = new QPushButton("Stop", controlGroup);

    // Set initial button states
    startButton->setEnabled(true);
    pauseButton->setEnabled(false);
    stopButton->setEnabled(false);

    // Add buttons to a horizontal layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(stopButton);

    // Create progress bar and labels for simulation status
    simulationProgress = new QProgressBar(controlGroup);
    simulationProgress->setRange(0, 100);
    simulationProgress->setValue(0);

    timeElapsedLabel = new QLabel("Time Elapsed: 0.0 s / 0.0 s", controlGroup);
    drainageVolumeLabel = new QLabel("Total Drainage: 0.0 m³", controlGroup);

    // Add all controls to the layout
    controlLayout->addLayout(buttonLayout);
    controlLayout->addWidget(simulationProgress);
    controlLayout->addWidget(timeElapsedLabel);
    controlLayout->addWidget(drainageVolumeLabel);

    // Set up connections for the control buttons
    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartSimulation);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseSimulation);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopSimulation);

    // Add the control group to the main input layout
    QVBoxLayout* inputLayout = qobject_cast<QVBoxLayout*>(inputTab->layout());
    if (inputLayout) {
        inputLayout->addWidget(controlGroup);
    }
}
