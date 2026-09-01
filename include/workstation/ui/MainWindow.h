#pragma once
#include <QMainWindow>
#include "workstation/core/Document.h"

class QLabel;

namespace workstation {
namespace ui {

class ViewportWindow;
class ModelTreeWidget;
class PropertiesWidget;
class ToolSettingsWidget;

// Main application window for GUI Shell 1.
//
// This class is PURE IMPLEMENTATION MECHANICS (Qt window ownership, docks,
// menus, toolbar, status bar). It contains NO CAD, geometry, view, camera,
// rendering, or D3D11 algorithm. It links against the existing `workstation`
// core and holds an empty Document only to demonstrate the architectural
// connection point; no document is loaded in GUI Shell 1.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onExit();
    void onAbout();
    void onOpenFile();
    void updateStatusBar();
    void toggleModelTree(bool visible);
    void toggleProperties(bool visible);
    void toggleToolSettings(bool visible);

private:
    void buildMenu();
    void buildToolBar();
    void buildStatusBar();
    void buildDockWidgets();

    Document m_document;  // existing core, empty in GUI Shell 1 (no file I/O)
    ViewportWindow*  m_viewport    = nullptr;
    ModelTreeWidget* m_modelTree   = nullptr;
    PropertiesWidget* m_properties  = nullptr;
    ToolSettingsWidget* m_toolSettings = nullptr;

    QLabel* m_rendererStatusLabel = nullptr;
};

} // namespace ui
} // namespace workstation
