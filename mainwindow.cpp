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
      isPanning(false)
{
    // Set application style for a modern look
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    
    // Set window title and size
    setWindowTitle("Advanced Hydrological Simulation");
    resize(1200, 800);
    
    // Set minimum size to ensure UI elements are properly displayed
    setMinimumSize(800, 600);
    
    // Initialize UI components - this will create simulation engine and timers
    setupUI();
    
    // Set up signal-slot connections
    setupConnections();
    
    // Update UI state
    updateUIState();
    
    // Configure sizing behavior
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

MainWindow::~MainWindow()
{
    // Clean up any resources
}

void MainWindow::setupUI()
{
    // Set window title and basic properties
    setWindowTitle("Drainage Basin Hydrological Model");
    resize(1200, 800);
    
    // Create main tab widget
    mainTabWidget = new QTabWidget(this);
    setCentralWidget(mainTabWidget);
    
    // Create input tab
    inputTab = new QWidget();
    inputTab->setLayout(new QVBoxLayout());
    
    // Create visualization tab
    visualizationTab = new QWidget();
    
    // Create DEM preview tab for manual outlet selection
    demPreviewTab = new QWidget();
    
    // Create a dedicated tab for rainfall configuration
    QWidget* rainfallTab = new QWidget();
    rainfallTab->setLayout(new QVBoxLayout());
    
    // Initialize shared table for outlet cells
    outletTableWidget = new QTableWidget(0, 3); // Rows will be added dynamically, 3 columns: i, j, drainage
    outletTableWidget->setHorizontalHeaderLabels(QStringList() << "Row (i)" << "Column (j)" << "Drainage (m^3)");
    outletTableWidget->horizontalHeader()->setStretchLastSection(true);
    outletTableWidget->setMinimumWidth(250);
    outletTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers); // Make read-only
    
    // Set up panels
    createInputPanel();
    createVisualizationPanel();
    createDEMPreviewPanel();
    createRainfallPanel(rainfallTab); // Pass the rainfall tab as the parent
    
    // Add tabs to main tab widget - explicit index assignment
    int tabIndex = 0;
    mainTabWidget->insertTab(tabIndex++, inputTab, "Input Parameters");
    mainTabWidget->insertTab(tabIndex++, rainfallTab, "Rainfall Configuration");
    mainTabWidget->insertTab(tabIndex++, demPreviewTab, "Outlet Selection");
    mainTabWidget->insertTab(tabIndex++, visualizationTab, "Simulation Results");
    
    // Note: setupConnections() will be called from the MainWindow constructor
    // Do not call it here to avoid duplicate connections
    
    // Create simulation engine
    simEngine = new SimulationEngine(this);
    
    // Initialize timers
    simTimer = new QTimer(this);
    uiUpdateTimer = new QTimer(this);
    
    // Initialize simulation state
    simulationRunning = false;
    simulationPaused = false;
    
    // Set up initial UI state
    updateUIState();
    
    // Initialize zoom and pan variables
    zoomLevel = 1.0f;
    panOffset = QPoint(0, 0);
    isPanning = false;
    manualOutletSelectionMode = false;
    
    // Print tab debugging info
    qDebug() << "Tab indices:";
    qDebug() << "Input Parameters tab index:" << mainTabWidget->indexOf(inputTab);
    qDebug() << "Rainfall Configuration tab index:" << mainTabWidget->indexOf(rainfallTab);
    qDebug() << "Outlet Selection tab index:" << mainTabWidget->indexOf(demPreviewTab);
    qDebug() << "Simulation Results tab index:" << mainTabWidget->indexOf(visualizationTab);
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
    
    // ======== SIMULATION CONTROL GROUP (Moved Here) ======== 
    QGroupBox *controlGroup = new QGroupBox("Simulation Control", inputTab);
    QVBoxLayout *controlLayout = new QVBoxLayout();
    
    // Control Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    startButton = new QPushButton("Start");
    pauseButton = new QPushButton("Pause");
    stopButton = new QPushButton("Stop");
    
    // Add buttons to layout
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(stopButton);
    controlLayout->addLayout(buttonLayout);
    
    // Add progress bar below buttons
    simulationProgress = new QProgressBar();
    simulationProgress->setRange(0, 100);
    simulationProgress->setValue(0);
    controlLayout->addWidget(simulationProgress);
    
    // Add time elapsed and drainage volume labels
    timeElapsedLabel = new QLabel("Time: 0.0 s");
    controlLayout->addWidget(timeElapsedLabel);
    
    drainageVolumeLabel = new QLabel("Drainage Volume: 0.0 m³");
    controlLayout->addWidget(drainageVolumeLabel);
    
    // Set the layout to the control group
    controlGroup->setLayout(controlLayout);
    
    // Add the control group to the input tab
    static_cast<QVBoxLayout*>(inputTab->layout())->addWidget(controlGroup);
    
    // ======== OUTLET CONTROL GROUP ======== 
    QGroupBox *outletControlGroup = new QGroupBox("Outlet Control", inputTab);
    QHBoxLayout *outletControlLayout = new QHBoxLayout();
    
    // Add buttons for outlet control
    selectOutletButton = new QPushButton("Select Outlets");
    clearOutletsButton = new QPushButton("Clear Outlets");
    
    outletControlLayout->addWidget(selectOutletButton);
    outletControlLayout->addWidget(clearOutletsButton);
    
    // Set the layout to the outlet control group
    outletControlGroup->setLayout(outletControlLayout);
    
    // Add the outlet control group to the input tab
    static_cast<QVBoxLayout*>(inputTab->layout())->addWidget(outletControlGroup);
    
    // Add a horizontal line to separate sections
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    static_cast<QVBoxLayout*>(inputTab->layout())->addWidget(line);
    
    // ======== RESULTS GROUP (Moved Save Button Here) ======== 
    QGroupBox *resultsGroup = new QGroupBox("Results", inputTab);
    QVBoxLayout *resultsLayout = new QVBoxLayout();
    
    // Add save button
    saveResultsButton = new QPushButton("Save Results");
    resultsLayout->addWidget(saveResultsButton);
    
    // Set the layout to the results group
    resultsGroup->setLayout(resultsLayout);
    
    // Add the results group to the input tab
    static_cast<QVBoxLayout*>(inputTab->layout())->addWidget(resultsGroup);

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
    QVBoxLayout *visualizationLayout = new QVBoxLayout(visualizationTab);
    
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
    
    // Change QLabel to ClickableLabel for interaction support
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
    visualizationLayout->addLayout(splitLayout);
    visualizationLayout->addLayout(zoomLayout);
    visualizationLayout->addWidget(resultsOutputLabel);
    visualizationLayout->addWidget(displayOptionsGroup);
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
    
    // Connect clickable result display in the visualization tab
    ClickableLabel *clickableResultDisplay = dynamic_cast<ClickableLabel*>(resultDisplayLabel);
    if (clickableResultDisplay) {
        connect(clickableResultDisplay, &ClickableLabel::clicked, this, &MainWindow::onResultDisplayClicked);
        connect(clickableResultDisplay, &ClickableLabel::mouseDragged, this, [this](QPoint delta) {
            panVisualization(delta);
        });
        connect(clickableResultDisplay, &ClickableLabel::mouseWheelScrolled, this, [this](int delta) {
            zoomVisualization(delta);
        });
        connect(clickableResultDisplay, &ClickableLabel::doubleClicked, this, 
                [this]() {
                    resetVisualizationView();
                });
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
}

void MainWindow::onSelectDEM()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Select DEM CSV File", "", "CSV Files (*.csv)");
    if (!fileName.isEmpty())
    {
        demFileLabel->setText(fileName);
        
        // Analyze file to suggest proper resolution
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // Try to determine if this is a high- or low-resolution DEM
            QTextStream in(&file);
            QString firstLine = in.readLine();
            QStringList values = firstLine.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
            
            // Check if the file contains no-data values (-999999)
            bool containsNoDataValues = firstLine.contains("-999999");
            
            // If the line has many values or contains no-data markers, likely a low-res DEM
            if (values.size() > 100 || containsNoDataValues) {
                // Suggest a higher resolution value (e.g., 10m for coarser DEM)
                double suggestedResolution = 10.0;
                
                // Ask user about suggested resolution
                QMessageBox resolutionPrompt;
                resolutionPrompt.setIcon(QMessageBox::Question);
                resolutionPrompt.setWindowTitle("DEM Resolution");
                resolutionPrompt.setText(QString("This appears to be a lower-resolution DEM file with larger coverage area."));
                resolutionPrompt.setInformativeText(QString("Would you like to set the resolution to %1 meters?").arg(suggestedResolution));
                resolutionPrompt.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                resolutionPrompt.setDefaultButton(QMessageBox::Yes);
                
                if (resolutionPrompt.exec() == QMessageBox::Yes) {
                    resolutionEdit->setValue(suggestedResolution);
                }
            }
            
            file.close();
        }
        
        // Set the current resolution in the simulation engine
        if (simEngine) {
            simEngine->setCellResolution(resolutionEdit->value());
        }
        
        // Try to load the DEM file
        if (!simEngine->loadDEM(fileName))
        {
            QMessageBox::warning(this, "Error", "Failed to load DEM file. Check the file format.");
            demFileLabel->setText("No file selected");
        }
        else
        {
            // Update the UI to show the DEM is loaded
            resultDisplayLabel->setText("DEM loaded successfully.\nConfigure parameters and press Start to begin simulation.");
            simDisplayLabel->setText("DEM loaded successfully.\nSelect 'Manual Selection' from Outlet Method dropdown and use 'Select Outlets' to choose outlet cells.");
            
            // Add resolution info to status
            double resolution = resolutionEdit->value();
            QString resInfo = QString("Resolution: %1 m").arg(resolution, 0, 'f', 2);
            outputLabel->setText(resInfo + " - " + simDisplayLabel->text());
            
            // If in manual outlet selection mode, show the DEM preview
            if (manualOutletSelectionMode && outletMethodCombo->currentIndex() == 1) {
                showDEMPreview();
            }
        }
    }
}

void MainWindow::onStartSimulation()
{
    if (!simEngine) {
        outputLabel->setText("Please load a DEM file first.");
        return;
    }

    // Reset UI and simulation
    simulationRunning = true;
    simulationPaused = false;
    updateUIState();

    // Configure simulation parameters from UI
    simEngine->setCellResolution(resolutionEdit->value());
    simEngine->setRainfall(rainfallEdit->value());
    simEngine->setManningCoefficient(manningCoeffEdit->value());
    simEngine->setInfiltrationRate(infiltrationEdit->value());
    simEngine->setMinWaterDepth(minDepthEdit->value());
    simEngine->setTotalTime(totalTimeEdit->value());
    
    // Debug output to check configurations
    qDebug() << "--- Simulation Parameters ---";
    qDebug() << "Cell Resolution:" << resolutionEdit->value() << "m";
    qDebug() << "Rainfall Rate:" << rainfallEdit->value() << "m/s";
    qDebug() << "Manning's Coefficient:" << manningCoeffEdit->value();
    qDebug() << "Infiltration Rate:" << infiltrationEdit->value() << "m/s";
    qDebug() << "Min Water Depth:" << minDepthEdit->value() << "m";
    qDebug() << "Total Time:" << totalTimeEdit->value() << "s";
    qDebug() << "Time-varying Rainfall:" << (simEngine->isTimeVaryingRainfall() ? "Enabled" : "Disabled");
    
    // Configure rainfall schedule if time-varying rainfall is enabled
    if (simEngine->isTimeVaryingRainfall()) {
        updateRainfallSchedule();
    }
    
    // Check outlet configuration
    int outletMethod = outletMethodCombo->currentIndex();
    qDebug() << "Outlet Method:" << (outletMethod == 0 ? "Automatic" : "Manual");
    
    if (outletMethod == 0) {
        // Automatic - by percentile
        double percentile = outletPercentileEdit->value() / 100.0;
        qDebug() << "Outlet Percentile:" << percentile;
        simEngine->configureOutletsByPercentile(percentile);
    } else {
        // Manual - check if we have any outlets defined
        qDebug() << "Manual Outlet Cells:" << manualOutletCells.size();
        if (manualOutletCells.isEmpty()) {
            qDebug() << "WARNING: No manual outlet cells selected!";
            outputLabel->setText("Warning: No manual outlet cells selected. Using automatic method.");
            double percentile = outletPercentileEdit->value() / 100.0;
            simEngine->configureOutletsByPercentile(percentile);
        } else {
            simEngine->setManualOutletCells(manualOutletCells);
        }
    }
    
    // Debug - verify outlet cells were set correctly
    if (outletMethod == 0) {
        QVector<QPoint> autoCells = simEngine->getAutomaticOutletCells();
        qDebug() << "Automatic Outlet Cells Count:" << autoCells.size();
        for (int i = 0; i < qMin(5, autoCells.size()); i++) {
            qDebug() << "  Outlet at:" << autoCells[i].x() << autoCells[i].y();
        }
        if (autoCells.size() > 5) qDebug() << "  ... and" << (autoCells.size() - 5) << "more";
    } else {
        qDebug() << "Manual Outlet Cells Count:" << manualOutletCells.size();
        for (int i = 0; i < qMin(5, manualOutletCells.size()); i++) {
            qDebug() << "  Outlet at:" << manualOutletCells[i].x() << manualOutletCells[i].y();
        }
        if (manualOutletCells.size() > 5) qDebug() << "  ... and" << (manualOutletCells.size() - 5) << "more";
    }
    
    // Initialize the simulation
    simEngine->initSimulation();
    
    // Start the simulation timer
    simTimer->start(50); // 50 ms interval
    uiUpdateTimer->start(200); // 200 ms for UI updates
    
    // Initialize progress bar
    simulationProgress->setMaximum(simEngine->getTotalTime());
    simulationProgress->setValue(0);
    
    // Update outputs
    timeElapsedLabel->setText("Time Elapsed: 0.0 s");
    drainageVolumeLabel->setText("Drainage Volume: 0.0 m³");
    outputLabel->setText("Simulation started.");
}

void MainWindow::onPauseSimulation()
{
    if (simulationRunning) {
        if (simulationPaused) {
            // Resume simulation
            simulationPaused = false;
            simTimer->start(50);
            pauseButton->setText("Pause");
        } else {
            // Pause simulation
            simulationPaused = true;
            simTimer->stop();
            pauseButton->setText("Resume");
        }
    }
}

void MainWindow::onStopSimulation()
{
    simTimer->stop();
    simulationRunning = false;
    simulationPaused = false;
    
    // Show final results
    double totalDrainage = simEngine->getTotalDrainage();
    resultsOutputLabel->setText(QString("Simulation stopped at time: %1 seconds\nTotal Drainage Volume: %2 m^3")
                        .arg(simEngine->getCurrentTime(), 0, 'f', 1)
                        .arg(totalDrainage, 0, 'f', 6));
    
    // Update UI state
    updateUIState();
}

void MainWindow::onSimulationStep()
{
    if (!simEngine)
        return;
    
    // Execute a single simulation step
    simEngine->stepSimulation();
    
    // Get current time and update progress
    double currentTime = simEngine->getCurrentTime();
    double totalTime = simEngine->getTotalTime();
    double totalDrainage = simEngine->getTotalDrainage();
    int progress = int((currentTime / totalTime) * 100.0);
    
    // Update the UI with more detailed drainage info
    simulationProgress->setValue(progress);
    timeElapsedLabel->setText(QString("Time: %1 s").arg(currentTime, 0, 'f', 1));
    drainageVolumeLabel->setText(QString("Drainage Volume: %1 m³").arg(totalDrainage, 0, 'f', 6));
    
    // Log drainage to help with debugging
    if (int(currentTime) % 10 == 0) { // Log every 10 seconds
        qDebug() << "Time:" << currentTime << "s, Total Drainage:" << totalDrainage << "m³";
    }
    
    // If using time-varying rainfall, update the UI with current rate
    if (timeVaryingRainfallCheckbox->isChecked()) {
        double currentRate = simEngine->getCurrentRainfallRate();
        // Highlight the active row in the rainfall table
        for (int row = 0; row < rainfallTableWidget->rowCount(); row++) {
            QTableWidgetItem* timeItem = rainfallTableWidget->item(row, 0);
            if (timeItem) {
                double rowTime = timeItem->text().toDouble();
                
                // If this row is active or the next row is in the future
                if (row == rainfallTableWidget->rowCount() - 1 || 
                    (rowTime <= currentTime && rainfallTableWidget->item(row + 1, 0)->text().toDouble() > currentTime)) {
                    // Highlight this row
                    for (int col = 0; col < rainfallTableWidget->columnCount(); col++) {
                        if (rainfallTableWidget->item(row, col)) {
                            rainfallTableWidget->item(row, col)->setBackground(QColor(200, 255, 200)); // Light green
                        }
                    }
                } else {
                    // Remove highlight
                    for (int col = 0; col < rainfallTableWidget->columnCount(); col++) {
                        if (rainfallTableWidget->item(row, col)) {
                            rainfallTableWidget->item(row, col)->setBackground(QColor(255, 255, 255)); // White
                        }
                    }
                }
            }
        }
    }
    
    // Update the visualization
    updateVisualization();
    
    // Update the outlet table with latest drainage data
    updateOutletTable();
    
    // Check if simulation is done
    if (currentTime >= totalTime)
    {
        simTimer->stop();
        simulationRunning = false;
        simulationPaused = false;
        
        // Get the total drainage volume and display
        resultsOutputLabel->setText(QString("Simulation Complete!\nTotal Drainage Volume: %1 m³\nTotal Simulation Time: %2 seconds")
                            .arg(totalDrainage, 0, 'f', 6)
                            .arg(totalTime));
        
        // Enable save results button
        saveResultsButton->setEnabled(true);
        
        QMessageBox::information(this, "Simulation Complete",
                               QString("Simulation complete!\nTotal drainage volume: %1 m³")
                               .arg(totalDrainage, 0, 'f', 6));
    }
}

void MainWindow::updateUIState()
{
    // Update button states based on simulation state
    startButton->setEnabled(!simulationRunning || simulationPaused);
    pauseButton->setEnabled(simulationRunning);
    stopButton->setEnabled(simulationRunning);
    
    // Disable parameter editing during simulation
    bool canEditParams = !simulationRunning;
    rainfallEdit->setEnabled(canEditParams);
    manningCoeffEdit->setEnabled(canEditParams);
    infiltrationEdit->setEnabled(canEditParams);
    totalTimeEdit->setEnabled(canEditParams);
    minDepthEdit->setEnabled(canEditParams);
    resolutionEdit->setEnabled(canEditParams);
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
    if (demFileLabel->text() == "No file selected") {
        QMessageBox::warning(this, "Error", "Please load a DEM file first.");
        return;
    }
    
    // No need to show a message box here - we already show one in onManualOutletCellsToggled
    // This will prevent duplicate message boxes
    
    // Enable manual outlet selection mode
    manualOutletSelectionMode = true;
    
    // Switch to Outlet Selection tab (index 2 based on our insertTab order)
    mainTabWidget->setCurrentIndex(2);
    
    // Show a special DEM preview for outlet selection
    showDEMPreview();
}

void MainWindow::showDEMPreview()
{
    if (!simEngine) return;
    
    // Store current zoom and pan values to restore after updating
    float currentZoom = zoomLevel;
    QPoint currentPan = panOffset;
    
    // Make sure the SimulationEngine has the latest outlet cells
    qDebug() << "Updating SimulationEngine with" << manualOutletCells.size() << "outlet cells";
    for (const QPoint &p : manualOutletCells) {
        qDebug() << "  Cell at:" << p.x() << p.y();
    }
    
    // Update the simulation engine with current outlet cells
    simEngine->setManualOutletCells(manualOutletCells);
    
    // Always get a fresh DEM preview image to ensure outlet markings are up-to-date
    // This is important since we want to see current outlet cells highlighted
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
        if (manualOutletCells.isEmpty()) {
            outputLabel->setText("No outlet cells selected. Click anywhere on the DEM to select outlets. Drag to pan, scroll to zoom.");
        } else {
            outputLabel->setText(QString("%1 outlet cell(s) selected. Click to add/remove outlets. Drag to pan, scroll to zoom.")
                              .arg(manualOutletCells.size()));
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

void MainWindow::updateDisplay(QLabel* displayLabel, const QImage& image, QLabel* statusLabel, const QString& statusPrefix, const QVector<QPoint>* selectedPoints)
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
        // when they appear/disappear
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    
    // Get viewport size (visible area)
    QSize viewportSize = scrollArea ? scrollArea->viewport()->size() : displayLabel->size();
    QSize imageSize = image.size();
    
    // Apply zoom to the image
    QSize scaledSize = imageSize * zoomLevel;
    
    // Set a reasonable maximum size limit
    scaledSize = scaledSize.boundedTo(QSize(10000, 10000));
    
    // Create a display pixmap that's large enough to contain the scaled image
    // Add a large fixed buffer (100px) to avoid frequent resizing
    QSize displaySize = QSize(
        qMax(viewportSize.width(), scaledSize.width() + 100),
        qMax(viewportSize.height(), scaledSize.height() + 100)
    );
    
    // Create the display pixmap
    QPixmap displayPixmap(displaySize);
    displayPixmap.fill(Qt::lightGray);
    
    // Calculate the centered position for the image
    int x = (displaySize.width() - scaledSize.width()) / 2 + panOffset.x();
    int y = (displaySize.height() - scaledSize.height()) / 2 + panOffset.y();
    
    // Scale the image with high quality
    QPixmap scaledPixmap = QPixmap::fromImage(image).scaled(
        scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Draw the zoomed/panned image on the display pixmap
    QPainter painter(&displayPixmap);
    painter.drawPixmap(x, y, scaledPixmap);
    
    // Add zoom level indicator and navigation help
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(Qt::black);
    
    QString zoomText = QString("Zoom: %1%").arg(int(zoomLevel * 100));
    painter.drawText(10, 20, zoomText);
    
    QString helpText = "Drag to pan | Scroll to zoom | Double-click to reset view";
    painter.drawText(10, displaySize.height() - 10, helpText);
    
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
    if (displayLabel == simDisplayLabel && selectedPoints && !selectedPoints->isEmpty()) {
        statusText += QString(" | %1 outlet(s) selected").arg(selectedPoints->size());
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
    updateDisplay(simDisplayLabel, currentDEMImage, outputLabel, "DEM visualization", &manualOutletCells);
}

void MainWindow::updateVisualization()
{
    if (!simEngine)
        return;
    
    currentSimulationImage = simEngine->getWaterDepthImage();
    if (currentSimulationImage.isNull())
        return;
    
    double drainageVol = simEngine->getTotalDrainage();
    double elapsedTime = simEngine->getCurrentTime();
    double totalTime = simEngine->getTotalTime();
    
    QString statusPrefix = QString("Simulation Progress: %1 / %2 s, Total Drainage: %3 m^3")
                         .arg(elapsedTime, 0, 'f', 1)
                         .arg(totalTime, 0, 'f', 1)
                         .arg(drainageVol, 0, 'f', 3);
    
    updateDisplay(resultDisplayLabel, currentSimulationImage, resultsOutputLabel, "Water visualization", nullptr);
    
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
    if (simEngine) {
        simEngine->setShowGrid(checked);
        
        // Update both the DEM preview and the simulation display
        updateVisualization();
        
        // If in manual outlet selection mode, also update the DEM preview
        if (manualOutletSelectionMode) {
            showDEMPreview();
        }
    }
}

void MainWindow::onToggleRulers(bool checked)
{
    if (simEngine) {
        simEngine->setShowRulers(checked);
        
        // Update both the DEM preview and the simulation display
        updateVisualization();
        
        // If in manual outlet selection mode, also update the DEM preview
        if (manualOutletSelectionMode) {
            showDEMPreview();
        }
    }
}

void MainWindow::onGridIntervalChanged(int value)
{
    if (simEngine) {
        simEngine->setGridInterval(value);
        
        // Update both the DEM preview and the simulation display
        updateVisualization();
        
        // If in manual outlet selection mode, also update the DEM preview
        if (manualOutletSelectionMode) {
            showDEMPreview();
        }
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
    // Update the simulation engine with the new resolution
    if (simEngine) {
        simEngine->setCellResolution(newResolution);
        
        // If we have a loaded DEM, refresh the preview
        if (demFileLabel->text() != "No file selected" && !currentDEMImage.isNull()) {
            // Refresh the DEM preview with new resolution
            currentDEMImage = simEngine->getDEMPreviewImage();
            updateDEMDisplay();
            
            // Add resolution info to status
            QString message = QString("Resolution changed to %1 m. Display adjusted accordingly.").arg(newResolution, 0, 'f', 2);
            outputLabel->setText(message);
        }
    }
}
