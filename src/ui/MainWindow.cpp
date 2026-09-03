#include "workstation/ui/MainWindow.h"
#include "workstation/ui/ViewportWidget.h"
#include "workstation/ui/ModelTreeWidget.h"
#include "workstation/ui/PropertiesWidget.h"
#include "workstation/ui/ToolSettingsWidget.h"
#include "workstation/cad/AttachmentManager.h"
#include "workstation/cad/AttachmentDialogs.h"
#include "workstation/scene/SceneManager.h"

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
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDoubleSpinBox>

namespace workstation {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_document(DocumentId{})
    , m_attachmentManager(std::make_unique<cad::AttachmentManager>()) {
    setWindowTitle("WorkstationCAD");
    setMinimumSize(1024, 768);
    buildDockWidgets();
    buildMenu();
    buildToolBar();
    buildCommandConsole();
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

    connect(m_modelTree, &ModelTreeWidget::objectSelected, m_properties, &PropertiesWidget::onSelectionChanged);
    connect(m_modelTree, &ModelTreeWidget::objectDeselected, m_properties, &PropertiesWidget::onSelectionCleared);
    connect(m_viewport, &ViewportWindow::selectionChanged, m_properties, &PropertiesWidget::onSelectionChanged);
    connect(m_viewport, &ViewportWindow::selectionCleared, m_properties, &PropertiesWidget::onSelectionCleared);
}

void MainWindow::buildMenu() {
    QMenu* fileMenu = menuBar()->addMenu("&File");

    QAction* openAct = fileMenu->addAction("&Open...");
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenFile);

    fileMenu->addSeparator();
    QAction* exitAct = fileMenu->addAction("E&xit");
    connect(exitAct, &QAction::triggered, this, &MainWindow::onExit);

    QMenu* editMenu = menuBar()->addMenu("&Edit");

    QAction* undoAct = editMenu->addAction("&Undo");
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setEnabled(false);
    connect(undoAct, &QAction::triggered, m_viewport, &ViewportWindow::Undo);

    QAction* redoAct = editMenu->addAction("&Redo");
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setEnabled(false);
    connect(redoAct, &QAction::triggered, m_viewport, &ViewportWindow::Redo);

    connect(m_viewport, &ViewportWindow::undoStateChanged, this,
        [undoAct, redoAct](bool canUndo, bool canRedo, const QString& undoName, const QString& redoName) {
            undoAct->setEnabled(canUndo);
            redoAct->setEnabled(canRedo);
            if (canUndo) undoAct->setText(QString("Undo %1").arg(undoName));
            else undoAct->setText("Undo");
            if (canRedo) redoAct->setText(QString("Redo %1").arg(redoName));
            else redoAct->setText("Redo");
        });

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

    QAction* tgConsole = viewMenu->addAction("Command Console");
    tgConsole->setCheckable(true);
    tgConsole->setChecked(true);
    connect(tgConsole, &QAction::toggled, this, &MainWindow::toggleCommandConsole);

    QMenu* toolsMenu = menuBar()->addMenu("&Tools");

    QMenu* attachMenu = toolsMenu->addMenu("&Attachments");
    QAction* dxfAct = attachMenu->addAction("Attach &DXF...");
    connect(dxfAct, &QAction::triggered, this, &MainWindow::onAttachDxf);
    QAction* dwgAct = attachMenu->addAction("Attach &DWG...");
    connect(dwgAct, &QAction::triggered, this, &MainWindow::onAttachDwg);
    QAction* sntAct = attachMenu->addAction("Attach SN&T...");
    connect(sntAct, &QAction::triggered, this, &MainWindow::onAttachSnt);
    attachMenu->addSeparator();
    QAction* manageAct = attachMenu->addAction("&Manage Attachments...");
    connect(manageAct, &QAction::triggered, this, &MainWindow::onManageAttachments);

    QAction* shadingAct = toolsMenu->addAction("&Shading Display...");
    connect(shadingAct, &QAction::triggered, this, &MainWindow::onShadingDisplay);

    QMenu* surfaceMenu = toolsMenu->addMenu("&Surface Reconstruction");
    QAction* genSurfaceAct = surfaceMenu->addAction("&Generate Surface");
    connect(genSurfaceAct, &QAction::triggered, this, [this]() {
        if (m_viewport) m_viewport->GenerateSurfaceForCloud();
        m_toolSettings->appendLog("Surface generated from point cloud.");
    });

    QAction* toggleSurfaceAct = surfaceMenu->addAction("&Toggle Surface");
    connect(toggleSurfaceAct, &QAction::triggered, this, [this]() {
        if (m_viewport) m_viewport->ToggleSurface();
        m_toolSettings->appendLog("Surface visibility toggled.");
    });

    surfaceMenu->addSeparator();
    QAction* surfacePhongAct = surfaceMenu->addAction("&Phong Shading");
    connect(surfacePhongAct, &QAction::triggered, this, [this]() {
        if (m_viewport) m_viewport->SetSurfaceShading(0);
    });

    QAction* surfaceDepthAct = surfaceMenu->addAction("&Depth Shading");
    connect(surfaceDepthAct, &QAction::triggered, this, [this]() {
        if (m_viewport) m_viewport->SetSurfaceShading(1);
    });

    QAction* surfaceCompositeAct = surfaceMenu->addAction("Surface &Composite");
    connect(surfaceCompositeAct, &QAction::triggered, this, [this]() {
        if (m_viewport) m_viewport->SetSurfaceShading(2);
    });

    QAction* surfaceEdlAct = surfaceMenu->addAction("Eye &Dome Lighting");
    connect(surfaceEdlAct, &QAction::triggered, this, [this]() {
        if (m_viewport) m_viewport->SetSurfaceShading(3);
    });

    surfaceMenu->addSeparator();

    // Display mode (radio group). Values match surface::SurfaceMode.
    QMenu* dispModeMenu = surfaceMenu->addMenu("&Display Mode");
    auto* dispGroup = new QActionGroup(this);
    dispGroup->setExclusive(true);
    struct DispModeEntry { const char* label; int mode; };
    static const DispModeEntry kDispModes[] = {
        {"&Points",        0}, // uses the existing point pipeline
        {"&Surface",       1},
        {"&Hybrid",        2},
        {"&Wireframe",     3},
        {"&Shaded Surface",4},
    };
    for (const auto& entry : kDispModes) {
        QAction* act = dispModeMenu->addAction(entry.label);
        act->setCheckable(true);
        // Points is the real startup state (no mesh exists yet - triangulation
        // only happens on demand when the user actually asks for a shaded/
        // surface mode). Checking "Surface" here previously lied about that:
        // a programmatic setChecked() doesn't fire triggered(), so no mesh
        // was ever built, but the menu claimed Surface was already active.
        act->setChecked(entry.mode == 0);
        act->setActionGroup(dispGroup);
        int mode = entry.mode;
        connect(act, &QAction::triggered, this, [this, mode]() {
            if (m_viewport) m_viewport->SetSurfaceMode(mode);
            m_toolSettings->appendLog(QString("Surface display mode: %1").arg(
                QString::fromUtf8(kDispModes[mode].label)));
        });
    }

    // Eye-dome lighting toggle. Drops back to Phong when unchecked.
    QAction* surfaceEdlToggleAct = surfaceMenu->addAction("☑ E&ye-Dome Lighting (EDL)");
    surfaceEdlToggleAct->setCheckable(true);
    surfaceEdlToggleAct->setChecked(false);
    connect(surfaceEdlToggleAct, &QAction::toggled, this, [this](bool checked) {
        if (m_viewport) m_viewport->SetSurfaceShading(checked ? 3 : 0);
    });

    // Shading quality preset (affects surface generation).
    QMenu* qualityMenu = surfaceMenu->addMenu("Shading &Quality");
    auto* qualityGroup = new QActionGroup(this);
    qualityGroup->setExclusive(true);
    struct QualityEntry { const char* label; int quality; };
    static const QualityEntry kQualities[] = {
        {"&Low", 0}, {"&Medium", 1}, {"&High", 2},
    };
    for (const auto& entry : kQualities) {
        QAction* act = qualityMenu->addAction(entry.label);
        act->setCheckable(true);
        act->setChecked(entry.quality == 1);
        act->setActionGroup(qualityGroup);
        int quality = entry.quality;
        connect(act, &QAction::triggered, this, [this, quality]() {
            if (m_viewport) m_viewport->SetSurfaceQuality(quality);
            m_toolSettings->appendLog(QString("Surface shading quality: %1").arg(
                QString::fromUtf8(kQualities[quality].label)));
        });
    }

    // Surface parameter dialog: generation inputs (max edge length,
    // neighbour radius) plus Phong material weights and light direction.
    QAction* surfaceParamsAct = surfaceMenu->addAction("Surface &Parameters...");
    connect(surfaceParamsAct, &QAction::triggered, this, [this]() {
        if (!m_viewport) return;

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Surface Parameters"));
        auto* form = new QFormLayout(&dialog);

        const auto& gen = m_viewport->GetSurfaceGenParams();

        auto makeSpin = [&dialog](double minV, double maxV, double step, double value,
                                  int decimals, const QString& special = QString()) {
            auto* spin = new QDoubleSpinBox(&dialog);
            spin->setRange(minV, maxV);
            spin->setDecimals(decimals);
            spin->setSingleStep(step);
            spin->setValue(value);
            if (!special.isEmpty()) spin->setSpecialValueText(special);
            return spin;
        };

        // 0 = automatic spacing-derived edge length (see AdaptiveTriangulator).
        auto* maxEdgeSpin = makeSpin(0.0, 100000.0, 0.5,
                                     gen.maxEdgeLength, 2, tr("Automatic"));
        form->addRow(tr("Maximum Edge Length"), maxEdgeSpin);

        auto* radiusSpin = makeSpin(0.01, 100000.0, 0.1,
                                    gen.normalNeighborRadius > 0.0
                                        ? gen.normalNeighborRadius : 1.5, 2);
        form->addRow(tr("Neighbour Radius"), radiusSpin);

        auto* lightXSpin = makeSpin(-1.0, 1.0, 0.05, 0.35, 2);
        auto* lightYSpin = makeSpin(-1.0, 1.0, 0.05, 0.35, 2);
        auto* lightZSpin = makeSpin(-1.0, 1.0, 0.05, 0.87, 2);
        form->addRow(tr("Light Direction X"), lightXSpin);
        form->addRow(tr("Light Direction Y"), lightYSpin);
        form->addRow(tr("Light Direction Z"), lightZSpin);

        auto* ambientSpin = makeSpin(0.0, 1.0, 0.05, 0.2, 2);
        auto* diffuseSpin = makeSpin(0.0, 1.0, 0.05, 0.7, 2);
        auto* specularSpin = makeSpin(0.0, 1.0, 0.05, 0.3, 2);
        auto* shininessSpin = makeSpin(1.0, 256.0, 1.0, 32.0, 0);
        form->addRow(tr("Ambient"), ambientSpin);
        form->addRow(tr("Diffuse"), diffuseSpin);
        form->addRow(tr("Specular"), specularSpin);
        form->addRow(tr("Shininess"), shininessSpin);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);

        if (dialog.exec() == QDialog::Accepted) {
            m_viewport->SetSurfaceMaxEdgeLength(maxEdgeSpin->value());
            m_viewport->SetSurfaceNeighborRadius(radiusSpin->value());
            m_viewport->SetSurfaceLightDirection(
                static_cast<float>(lightXSpin->value()),
                static_cast<float>(lightYSpin->value()),
                static_cast<float>(lightZSpin->value()));
            m_viewport->SetSurfaceMaterial(
                static_cast<float>(ambientSpin->value()),
                static_cast<float>(diffuseSpin->value()),
                static_cast<float>(specularSpin->value()),
                static_cast<float>(shininessSpin->value()));
            m_toolSettings->appendLog(tr("Surface parameters updated"));
        }
    });

    QMenu* vizMenu = toolsMenu->addMenu("&Visualization");
    auto* vizGroup = new QActionGroup(this);
    vizGroup->setExclusive(true);

    struct VizModeEntry { const char* label; int mode; };
    // Values match workstation::renderer::VisualizationMode.
    static const VizModeEntry kVizModes[] = {
        {"RGB", 0},
        {"Intensity", 1},
        {"Classification", 2},
        {"Elevation", 3},
        {"Height Ramp", 4},
        {"Normal Shading", 5},
        {"Density", 6},
        {"Depth Shading", 12},
        {"Surface Shading", 13},
        {"Eye-Dome Lighting", 14},
        {"Classification (Full)", 15},
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
    tb->setMovable(false);
    QAction* exitAct = tb->addAction("Exit");
    connect(exitAct, &QAction::triggered, this, &MainWindow::onExit);

    tb->addSeparator();
    addCadToolActions(tb);
}

void MainWindow::addCadToolActions(QToolBar* toolbar) {
    struct ToolEntry { const char* label; int id; };
    static const ToolEntry tools[] = {
        {"Pt", 0}, {"Line", 1}, {"PLine", 2}, {"Poly", 3},
        {"Rect", 4}, {"Circ", 5}, {"Arc", 6}, {"Text", 7}, {"Dim", 8},
    };
    for (const auto& t : tools) {
        QAction* act = toolbar->addAction(t.label);
        act->setToolTip(t.label);
        int id = t.id;
        connect(act, &QAction::triggered, this, [this, id]() { onToolAction(id); });
    }

    toolbar->addSeparator();
    // Quick measurement toggle
    QAction* measureAct = toolbar->addAction("Measure");
    measureAct->setToolTip("Measurement Tool");
    connect(measureAct, &QAction::triggered, this, [this]() { onToolAction(10); });

    QAction* selectAct = toolbar->addAction("Select");
    selectAct->setToolTip("Selection Tool");
    connect(selectAct, &QAction::triggered, this, [this]() { onToolAction(11); });

    QAction* clipAct = toolbar->addAction("Clip");
    clipAct->setToolTip("Clip Tool");
    connect(clipAct, &QAction::triggered, this, [this]() { onToolAction(12); });
}

void MainWindow::buildCommandConsole() {
    auto* consoleDock = new QDockWidget("Command Console", this);
    consoleDock->setObjectName("CommandConsoleDock");

    auto* consoleWidget = new QWidget(consoleDock);
    auto* layout = new QVBoxLayout(consoleWidget);
    layout->setContentsMargins(2, 2, 2, 2);

    m_commandOutput = new QTextEdit(consoleWidget);
    m_commandOutput->setReadOnly(true);
    m_commandOutput->setMaximumHeight(120);
    m_commandOutput->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: Consolas, monospace; font-size: 11px;");
    layout->addWidget(m_commandOutput);

    auto* inputRow = new QWidget(consoleWidget);
    auto* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);

    m_commandInput = new QLineEdit(inputRow);
    m_commandInput->setPlaceholderText("Type a command (e.g. OPEN, MEASURE, CLIP)...");
    m_commandInput->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    connect(m_commandInput, &QLineEdit::returnPressed, this, &MainWindow::onCommandEntered);
    inputLayout->addWidget(m_commandInput);

    auto* sendBtn = new QPushButton("Send", inputRow);
    sendBtn->setFixedWidth(50);
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onCommandEntered);
    inputLayout->addWidget(sendBtn);

    layout->addWidget(inputRow);

    consoleDock->setWidget(consoleWidget);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

    m_commandOutput->append("WorkstationCAD Command Console v1.0");
    m_commandOutput->append("Type 'help' for available commands.");
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
    if (auto* sceneMgr = m_viewport->GetSceneManager()) {
        m_modelTree->setSceneManager(sceneMgr);
        m_properties->setSceneManager(sceneMgr);
    }

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

void MainWindow::toggleCommandConsole(bool visible) {
    if (QDockWidget* d = findChild<QDockWidget*>("CommandConsoleDock")) d->setVisible(visible);
}

void MainWindow::onCommandEntered() {
    if (!m_commandInput || !m_commandOutput) return;
    QString cmd = m_commandInput->text().trimmed();
    if (cmd.isEmpty()) return;

    m_commandOutput->append("> " + cmd);
    m_commandInput->clear();

    QString upper = cmd.toUpper();
    if (upper == "HELP") {
        m_commandOutput->append("Available commands:");
        m_commandOutput->append("  OPEN <file>      - Open a LAS/LAZ/SNT file");
        m_commandOutput->append("  MEASURE          - Activate measurement tool");
        m_commandOutput->append("  SELECT           - Activate selection tool");
        m_commandOutput->append("  CLIP             - Activate clip tool");
        m_commandOutput->append("  VIZ <mode>       - Set visualization mode");
        m_commandOutput->append("  STATUS           - Show renderer status");
        m_commandOutput->append("  CLEAR            - Clear console");
        m_commandOutput->append("  HELP             - Show this help");
    } else if (upper == "STATUS") {
        if (m_viewport && m_viewport->IsRendererReady()) {
            m_commandOutput->append(QString("FPS: %1 | Points: %2")
                .arg(m_viewport->GetLastFPS(), 0, 'f', 1)
                .arg(m_viewport->GetLoadedPointCount()));
        } else {
            m_commandOutput->append("Renderer not ready.");
        }
    } else if (upper == "CLEAR") {
        m_commandOutput->clear();
    } else if (upper.startsWith("OPEN ")) {
        QString path = cmd.mid(5).trimmed();
        if (!path.isEmpty()) {
            QString error;
            if (m_viewport->LoadPointCloudFile(path, &error)) {
                m_commandOutput->append("Loaded: " + path);
            } else {
                m_commandOutput->append("Error: " + error);
            }
        }
    } else if (upper.startsWith("VIZ ")) {
        QString modeStr = cmd.mid(4).trimmed().toLower();
        int mode = -1;
        if (modeStr == "rgb") mode = 0;
        else if (modeStr == "intensity") mode = 1;
        else if (modeStr == "classification") mode = 2;
        else if (modeStr == "elevation") mode = 3;
        else if (modeStr == "height") mode = 4;
        else if (modeStr == "normal") mode = 5;
        else if (modeStr == "depth") mode = 12;
        else if (modeStr == "surface") mode = 13;
        else if (modeStr == "edl") mode = 14;
        if (mode >= 0) {
            onVisualizationModeSelected(mode);
            m_commandOutput->append("Visualization set to: " + modeStr);
        } else {
            m_commandOutput->append("Unknown mode. Use: rgb, intensity, classification, elevation, height, normal, depth, surface, edl");
        }
    } else {
        m_commandOutput->append("Unknown command. Type 'help' for available commands.");
    }
}

void MainWindow::onToolAction(int toolId) {
    m_toolSettings->appendLog(QString("Tool activated: %1").arg(toolId));
}

void MainWindow::onAttachDxf() {
    QString path = QFileDialog::getOpenFileName(this, "Attach DXF File", QString(), "DXF Files (*.dxf);;All Files (*)");
    if (path.isEmpty()) return;
    std::string error;
    std::string stdPath = path.toStdString();
    int idx = m_attachmentManager->addDxf(stdPath, &error);
    if (idx >= 0) {
        auto* dxf = m_attachmentManager->dxfAttachment(idx);
        if (m_viewport && dxf) {
            m_viewport->LoadDxfAttachment(dxf);
        }
        if (auto* sceneMgr = m_viewport->GetSceneManager()) {
            m_modelTree->setSceneManager(sceneMgr);
            m_properties->setSceneManager(sceneMgr);
        }
        m_toolSettings->appendLog(QString("Attached DXF: %1").arg(QFileInfo(path).fileName()));
        statusBar()->showMessage(QString("DXF attached: %1").arg(QFileInfo(path).fileName()), 3000);
    } else {
        QMessageBox::warning(this, "DXF Load Failed", QString::fromStdString(error));
        m_toolSettings->appendLog(QString("Failed to attach DXF: %1").arg(QString::fromStdString(error)));
    }
}

void MainWindow::onAttachDwg() {
    QString path = QFileDialog::getOpenFileName(this, "Attach DWG File", QString(), "DWG Files (*.dwg);;All Files (*)");
    if (path.isEmpty()) return;
    std::string error;
    std::string stdPath = path.toStdString();
    int idx = m_attachmentManager->addDwg(stdPath, &error);
    if (idx >= 0) {
        auto* dwg = m_attachmentManager->dwgAttachment(idx);
        if (m_viewport && dwg) {
            m_viewport->LoadDwgAttachment(dwg);
        }
        if (auto* sceneMgr = m_viewport->GetSceneManager()) {
            m_modelTree->setSceneManager(sceneMgr);
            m_properties->setSceneManager(sceneMgr);
        }
        m_toolSettings->appendLog(QString("Attached DWG: %1").arg(QFileInfo(path).fileName()));
        statusBar()->showMessage(QString("DWG attached: %1").arg(QFileInfo(path).fileName()), 3000);
    } else {
        QMessageBox::warning(this, "DWG Load Failed", QString::fromStdString(error));
        m_toolSettings->appendLog(QString("Failed to attach DWG: %1").arg(QString::fromStdString(error)));
    }
}

void MainWindow::onAttachSnt() {
    QString path = QFileDialog::getOpenFileName(this, "Attach SNT File", QString(), "SNT Files (*.snt);;All Files (*)");
    if (path.isEmpty()) return;
    std::string error;
    std::string stdPath = path.toStdString();
    int idx = m_attachmentManager->addSnt(stdPath, &error);
    if (idx >= 0) {
        auto* snt = m_attachmentManager->sntAttachment(idx);
        if (m_viewport && snt) {
            m_viewport->LoadSntAttachment(snt);
        }
        if (auto* sceneMgr = m_viewport->GetSceneManager()) {
            m_modelTree->setSceneManager(sceneMgr);
            m_properties->setSceneManager(sceneMgr);
        }
        m_toolSettings->appendLog(QString("Attached SNT: %1").arg(QFileInfo(path).fileName()));
        statusBar()->showMessage(QString("SNT attached: %1").arg(QFileInfo(path).fileName()), 3000);
    } else {
        QMessageBox::warning(this, "SNT Load Failed", QString::fromStdString(error));
        m_toolSettings->appendLog(QString("Failed to attach SNT: %1").arg(QString::fromStdString(error)));
    }
}

void MainWindow::onManageAttachments() {
    auto* dlg = new cad::AttachmentDialog(m_attachmentManager.get(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onShadingDisplay() {
    auto* dlg = new cad::ShadingDisplayDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &cad::ShadingDisplayDialog::shadingRequested, this, [this, dlg]() {
        m_toolSettings->appendLog(QString("Shading requested: az=%1 el=%2 amb=%3 quality=%4")
            .arg(dlg->azimuth(), 0, 'f', 1)
            .arg(dlg->elevation(), 0, 'f', 1)
            .arg(dlg->ambient(), 0, 'f', 2)
            .arg(dlg->qualityLevel()));
    });
    dlg->show();
}

} // namespace ui
} // namespace workstation
