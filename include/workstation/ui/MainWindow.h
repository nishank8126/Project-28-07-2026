#pragma once
#include <QMainWindow>
#include "workstation/core/Document.h"
#include <memory>

class QLabel;
class QToolBar;
class QLineEdit;
class QTextEdit;

namespace workstation {
namespace cad {
class AttachmentManager;
}

namespace ui {

class ViewportWindow;
class ModelTreeWidget;
class PropertiesWidget;
class ToolSettingsWidget;

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
    void toggleCommandConsole(bool visible);
    void onVisualizationModeSelected(int mode);
    void onCommandEntered();
    void onToolAction(int toolId);

    void onAttachDxf();
    void onAttachDwg();
    void onAttachSnt();
    void onManageAttachments();
    void onShadingDisplay();

private:
    void buildMenu();
    void buildToolBar();
    void buildStatusBar();
    void buildDockWidgets();
    void buildCommandConsole();
    void addCadToolActions(QToolBar* toolbar);

    Document m_document;
    ViewportWindow*  m_viewport    = nullptr;
    ModelTreeWidget* m_modelTree   = nullptr;
    PropertiesWidget* m_properties  = nullptr;
    ToolSettingsWidget* m_toolSettings = nullptr;

    QLabel* m_rendererStatusLabel = nullptr;
    QLineEdit* m_commandInput = nullptr;
    QTextEdit* m_commandOutput = nullptr;
    QToolBar* m_cadToolBar = nullptr;

    std::unique_ptr<cad::AttachmentManager> m_attachmentManager;
};

} // namespace ui
} // namespace workstation
