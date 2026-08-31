#include "workstation/ui/MainWindow.h"
#include "workstation/ui/ViewportWidget.h"
#include "workstation/ui/ModelTreeWidget.h"
#include "workstation/ui/PropertiesWidget.h"
#include "workstation/ui/ToolSettingsWidget.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QMessageBox>
#include <QLabel>

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
}

MainWindow::~MainWindow() = default;

void MainWindow::buildDockWidgets() {
    m_viewport = new ViewportWidget(this);
    setCentralWidget(m_viewport);

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
    m_toolSettings->appendLog("GUI Shell 1 started; renderer not connected.");
}

void MainWindow::buildMenu() {
    QMenu* fileMenu = menuBar()->addMenu("&File");

    QAction* openAct = fileMenu->addAction("&Open...");
    openAct->setEnabled(false);  // file I/O not implemented in GUI Shell 1

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

    menuBar()->addMenu("&Tools");  // no CAD tools implemented in GUI Shell 1

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
    statusBar()->addPermanentWidget(new QLabel("Renderer: not connected"));
}

void MainWindow::onExit() {
    close();
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
