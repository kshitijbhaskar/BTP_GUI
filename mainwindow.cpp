#include "mainwindow.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QImage>
#include <QPainter>
#include <QString>
#include <QDebug>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QGroupBox>
#include <QFrame>
#include <QFormLayout>
#include <QDateTime>
#include <QStyleFactory>
#include <QApplication>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>
#include <QSizePolicy>
#include <QDir>
#include <QHeaderView>
#include <algorithm>
#include <QStatusBar>

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

MainWindow::MainWindow(QWidget *parent)
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
      previousTabIndex(0)
{
    // Apply modern style to the application
    setStyleSheet("QMainWindow { background-color: #f0f0f0; }"
                 "QGroupBox { font-weight: bold; }");
    
    // Initialize UI components and layouts
    setupUI();
    
    // Create simulation engine and set up connections
    simEngine = new SimulationEngine(this);
    setupSimulationEngineConnections();
    
    // Configure timers for simulation and UI updates
    simTimer = new QTimer(this);
    simTimer->setInterval(100);  // 10 Hz simulation rate
    connect(simTimer, &QTimer::timeout, this, &MainWindow::onSimulationStep);
    
    uiUpdateTimer = new QTimer(this);
    uiUpdateTimer->setInterval(50);  // 20 Hz UI refresh
    connect(uiUpdateTimer, &QTimer::timeout, this, &MainWindow::updateUIState);
    
    // Initial UI state setup
    updateUIState();
}

MainWindow::~MainWindow()
{
    // Clean up any resources
}

/**
 * @brief Sets up the main UI layout and components
 * 
 * Creates:
 * 1. Tab-based interface with:
 *    - Input panel for simulation parameters
 *    - DEM preview for outlet selection
 *    - Visualization panel for results
 * 
 * 2. Control panels for:
 *    - Simulation control (Start/Pause/Stop)
 *    - Outlet configuration
 *    - Time-varying rainfall
 *    - Display options
 */
void MainWindow::setupUI()
{
    // Create main layout with tabs
    mainTabWidget = new QTabWidget(this);
    setCentralWidget(mainTabWidget);
    
    // Create and add input tab
    inputTab = new QWidget();
    QVBoxLayout* inputLayout = new QVBoxLayout(inputTab);
    createInputPanel();
    createControlPanel();
    mainTabWidget->addTab(inputTab, "Input Parameters");
    
    // Create and add DEM preview tab
    demPreviewTab = new QWidget();
    createDEMPreviewPanel();
    mainTabWidget->addTab(demPreviewTab, "DEM Preview");
    
    // Create and add visualization tab
    visualizationTab = new QWidget();
    createVisualizationPanel();
    mainTabWidget->addTab(visualizationTab, "Simulation Results");
    
    // Set default window properties
    setMinimumSize(1024, 768);
    setWindowTitle("Hydrological Simulation");
}

/**
 * @brief Creates the parameter input panel
 * 
 * Contains input fields for:
 * - DEM file selection
 * - Manning's coefficient
 * - Infiltration rate
 * - Rainfall configuration
 * - Simulation duration
 * - Resolution settings
 * - Water depth thresholds
 */
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

/**
 * @brief Creates the simulation control panel
 * 
 * Provides controls for:
 * - Starting/pausing/stopping simulation
 * - Progress monitoring
 * - Time elapsed display
 * - Drainage volume tracking
 */
void MainWindow::createControlPanel()
{
    // Use the provided parent or default to inputTab if none is provided
    QWidget* parentWidget = inputTab;
    
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

/**
 * @brief Creates the DEM preview panel
 * 
 * Features:
 * - Interactive DEM visualization
 * - Manual outlet selection interface
 * - Pan and zoom controls
 * - Grid and ruler overlays
 * - Coordinate display
 */
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

/**
 * @brief Creates the visualization panel for simulation results
 * 
 * Displays:
 * - Real-time water depth visualization
 * - Flow accumulation paths
 * - Outlet positions and drainage data
 * - Interactive view controls
 */
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

/**
 * @brief Shows the DEM preview in the display label
 * 
 * Performs:
 * - Gets preview image from simulation engine
 * - Updates display with current zoom and pan settings
 * - Updates outlet table and coordinates
 * - Updates UI status messages based on outlet selection mode
 */
void MainWindow::showDEMPreview()
{
    // ...existing code...
}

/**
 * @brief Updates the DEM display with current view settings
 * 
 * Applies:
 * - Current zoom level
 * - Pan offset
 * - Grid overlay (if enabled)
 * - Ruler overlay (if enabled)
 * - Outlet markers at selected positions
 */
void MainWindow::updateDEMDisplay()
{
    // ...existing code...
}

/**
 * @brief Handles zooming of the display
 * 
 * @param label The QLabel widget to zoom
 * @param zoomIn true to zoom in, false to zoom out
 * 
 * Features:
 * - Maintains aspect ratio
 * - Limits zoom range (0.1x to 10x)
 * - Updates pan offset for centered zoom
 * - Refreshes grid and ruler overlays
 */
void MainWindow::zoomDisplay(QLabel* label, bool zoomIn)
{
    // ...existing code...
}

/**
 * @brief Resets the display view to default settings
 * 
 * Resets:
 * - Zoom level to 1.0
 * - Pan offset to (0,0)
 * - Updates status message
 * - Refreshes display
 * 
 * @param label The display label to reset
 * @param outputLabel Label for status messages
 * @param message Status message to show
 */
void MainWindow::resetDisplayView(QLabel* label, QLabel* outputLabel, const QString& message)
{
    // ...existing code...
}

/**
 * @brief Handles mouse clicks on the simulation display
 * 
 * Processes:
 * - Manual outlet selection (if enabled)
 * - Coordinate conversion from screen to DEM space
 * - Updates outlet table with new selections
 * - Validates click position within DEM bounds
 * 
 * @param pos The clicked position in screen coordinates
 */
void MainWindow::onSimDisplayClicked(QPoint pos)
{
    // ...existing code...
}

/**
 * @brief Updates the outlet cell table with current selections
 * 
 * Features:
 * - Displays outlet coordinates
 * - Shows elevation at each outlet
 * - Maintains selection state
 * - Updates row colors based on validity
 * - Sorts outlets by elevation
 */
void MainWindow::updateOutletTable()
{
    // ...existing code...
}

/**
 * @brief Sets up connections between simulation engine and UI
 * 
 * Connects:
 * - Progress updates
 * - Time step notifications
 * - Error messages
 * - Visualization updates
 * - Status messages
 * - Parameter synchronization
 */
void MainWindow::setupSimulationEngineConnections()
{
    // ...existing code...
}

/**
 * @brief Updates the UI state based on current simulation status
 * 
 * Updates:
 * - Button states (enabled/disabled)
 * - Progress indicators
 * - Time display
 * - Drainage volume
 * - Tab visibility
 * - Control panel states
 * - Status messages
 */
void MainWindow::updateUIState()
{
    // ...existing code...
}

/**
 * @brief Handles the start simulation button click
 * 
 * Performs:
 * - Parameter validation
 * - Simulation initialization
 * - UI state updates
 * - Timer setup
 * - Progress reset
 * - Visualization preparation
 */
void MainWindow::onStartSimulation()
{
    // ...existing code...
}

/**
 * @brief Processes one simulation time step
 * 
 * Handles:
 * - Time advancement
 * - Water flow calculations
 * - Progress updates
 * - Visualization refresh
 * - Data collection
 * - Status updates
 */
void MainWindow::onSimulationStep()
{
    // ...existing code...
}
