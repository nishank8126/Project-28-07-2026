#include "workstation/ui/MainWindow.h"
#include "workstation/ui/ViewportWidget.h"
#include "workstation/ui/ModelTreeWidget.h"
#include "workstation/ui/PropertiesWidget.h"
#include "workstation/ui/ToolSettingsWidget.h"

#include <QMenuBar>
#include <QActionGroup>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QMessageBox>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>

namespace workstation {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_document(DocumentId{}) {
    setWindowTitle("WorkstationCAD");
    buildDockWidgets();
    buildMenu();
    buildToolBar();
    buildStatusBar();

    auto* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    statusTimer->start(500);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildDockWidgets() {
    m_viewport = new ViewportWindow();
    QWidget* viewportContainer = QWidget::createWindowContainer(m_viewport, this);
    viewportContainer->setMinimumSize(320, 240);
    viewportContainer->setFocusPolicy(Qt::StrongFocus);
    setCentralWidget(viewportContainer);

    auto* leftDock = new QDockWidget("Model Tree", this);
    leftDock->setObjectName("ModelTreeDock");
    m_modelTree = new ModelTreeWidget(leftDock);
    leftDock->setWidget(m_modelTree);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    auto* rightDock = new QDockWidget("Properties", this);
    rightDock->setObjectName("PropertiesDock");
    m_properties = new PropertiesWidget(rightDock);
    rightDock->setWidget(m_properties);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    auto* bottomDock = new QDockWidget("Tool Settings / Output", this);
    bottomDock->setObjectName("ToolSettingsDock");
    m_toolSettings = new ToolSettingsWidget(bottomDock);
    bottomDock->setWidget(m_toolSettings);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);

    m_modelTree->setDocument(&m_document);
    m_toolSettings->appendLog("WorkstationCAD started.");
}

void MainWindow::buildMenu() {
    QMenu* fileMenu = menuBar()->addMenu("&File");

    QAction* openAct = fileMenu->addAction("&Open...");
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenFile);

    fileMenu->addSeparator();
    QAction* exitAct = fileMenu->addAction("E&xit");
    connect(exitAct, &QAction::triggered, this, &MainWindow::onExit);

    menuBar()->addMenu("&Edit");  // no supported commands yet

    QMenu* viewMenu = menuBar()->addMenu("&View");
    QAction* tgModel = viewMenu->addAction("Model Tree");
    tgModel->setCheckable(true);
    tgModel->setChecked(true);
    connect(tgModel, &QAction::toggled, this, &MainWindow::toggleModelTree);

    QAction* tgProps = viewMenu->addAction("Properties");
    tgProps->setCheckable(true);
    tgProps->setChecked(true);
    connect(tgProps, &QAction::toggled, this, &MainWindow::toggleProperties);

    QAction* tgTools = viewMenu->addAction("Tool Settings");
    tgTools->setCheckable(true);
    tgTools->setChecked(true);
    connect(tgTools, &QAction::toggled, this, &MainWindow::toggleToolSettings);

    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    QMenu* vizMenu = toolsMenu->addMenu("&Visualization");
    auto* vizGroup = new QActionGroup(this);
    vizGroup->setExclusive(true);

    struct VizModeEntry { const char* label; int mode; };
    // Values match workstation::renderer::VisualizationMode. Modes without
    // backing data (ReturnNumber/ScanAngle/gpsTime/User) are omitted.
    static const VizModeEntry kVizModes[] = {
        {"RGB", 0},
        {"Intensity", 1},
        {"Classification", 2},
        {"Elevation", 3},
        {"Height Ramp", 4},
        {"Normal Shading", 5},
        {"Density", 6},
    };
    for (const auto& entry : kVizModes) {
        QAction* act = vizMenu->addAction(entry.label);
        act->setCheckable(true);
        act->setChecked(entry.mode == 0);
        act->setActionGroup(vizGroup);
        int mode = entry.mode;
        connect(act, &QAction::triggered, this, [this, mode]() { onVisualizationModeSelected(mode); });
    }

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    QAction* aboutAct = helpMenu->addAction("&About WorkstationCAD");
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::buildToolBar() {
    QToolBar* tb = addToolBar("Main");
    QAction* exitAct = tb->addAction("Exit");
    connect(exitAct, &QAction::triggered, this, &MainWindow::onExit);
}

void MainWindow::buildStatusBar() {
    statusBar()->showMessage("Ready");
    m_rendererStatusLabel = new QLabel("Renderer: initializing...");
    statusBar()->addPermanentWidget(m_rendererStatusLabel);
}

void MainWindow::onExit() {
    close();
}

void MainWindow::onVisualizationModeSelected(int mode) {
    m_viewport->SetVisualizationMode(mode);
}

void MainWindow::onOpenFile() {
    QString path = QFileDialog::getOpenFileName(
        this, "Open File", QString(),
        "Point Cloud / Vector Files (*.las *.laz *.snt);;"
        "Point Cloud Files (*.las *.laz);;"
        "SNT Vector Files (*.snt);;All Files (*)");
    if (path.isEmpty()) return;

    QString baseName = QFileInfo(path).fileName();

    if (path.endsWith(".snt", Qt::CaseInsensitive)) {
        QString error;
        quint32 entityCount = 0, polylineCount = 0;
        if (!m_viewport->LoadVectorOverlayFile(path, &error, &entityCount, &polylineCount)) {
            QMessageBox::warning(this, "Open SNT File",
                                  QString("Failed to load file:\n%1").arg(error));
            m_toolSettings->appendLog(QString("Failed to load %1: %2").arg(path, error));
            return;
        }
        m_toolSettings->appendLog(
            QString("Loaded %1 -- header reports %2 entities, recovered %3 polyline/shape "
                    "geometries (circles and text are not yet decoded by this best-effort "
                    "reader; see plan notes).")
                .arg(path).arg(entityCount).arg(polylineCount));
        statusBar()->showMessage(QString("Loaded %1 (%2/%3 entities recovered)")
                                      .arg(baseName).arg(polylineCount).arg(entityCount), 5000);
        return;
    }

    QString error;
    if (!m_viewport->LoadPointCloudFile(path, &error)) {
        QMessageBox::warning(this, "Open Point Cloud",
                              QString("Failed to load file:\n%1").arg(error));
        m_toolSettings->appendLog(QString("Failed to load %1: %2").arg(path, error));
        return;
    }

    quint64 pointCount = m_viewport->GetLoadedPointCount();
    m_modelTree->setLoadedPointCloud(baseName, pointCount);

    double sizeX = 0, sizeY = 0, sizeZ = 0;
    m_viewport->GetLoadedExtent(sizeX, sizeY, sizeZ);
    m_properties->showPointCloudProperties(baseName, pointCount, sizeX, sizeY, sizeZ);
    m_toolSettings->appendLog(QString("Loaded %1 (%2 points)").arg(path).arg(pointCount));
    statusBar()->showMessage(QString("Loaded %1").arg(baseName), 5000);
}

void MainWindow::updateStatusBar() {
    if (!m_rendererStatusLabel) return;
    if (!m_viewport->IsRendererReady()) {
        m_rendererStatusLabel->setText("Renderer: initializing...");
        return;
    }
    m_rendererStatusLabel->setText(
        QString("Renderer: %1 FPS | %2 points")
            .arg(m_viewport->GetLastFPS(), 0, 'f', 0)
            .arg(m_viewport->GetLoadedPointCount()));
}

void MainWindow::onAbout() {
    const QString info =
        QString("WorkstationCAD\nGUI Shell 1 (Qt %1)\nViewport renderer: not connected\n"
                "D3D11: not connected\nNo camera/view algorithm present")
            .arg(QT_VERSION_STR);
    QMessageBox::about(this, "About WorkstationCAD", info);
}

void MainWindow::toggleModelTree(bool visible) {
    if (QDockWidget* d = findChild<QDockWidget*>("ModelTreeDock")) d->setVisible(visible);
}

void MainWindow::toggleProperties(bool visible) {
    if (QDockWidget* d = findChild<QDockWidget*>("PropertiesDock")) d->setVisible(visible);
}

void MainWindow::toggleToolSettings(bool visible) {
    if (QDockWidget* d = findChild<QDockWidget*>("ToolSettingsDock")) d->setVisible(visible);
}

} // namespace ui
} // namespace workstation
