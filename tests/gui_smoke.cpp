#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QPlainTextEdit>
#include <QTimer>
#include <cstdio>

#include "workstation/ui/MainWindow.h"
#include "workstation/ui/ViewportWidget.h"
#include "workstation/ui/ModelTreeWidget.h"
#include "workstation/ui/PropertiesWidget.h"
#include "workstation/ui/ToolSettingsWidget.h"

// GUI Shell 1 smoke test: validates that the main window and all placeholder
// widgets are actually constructed, visible, docked, and shut down cleanly.
// This is a validation harness, not a product feature.
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    workstation::ui::MainWindow w;
    w.resize(1600, 900);
    w.show();

    bool ok = true;

    if (!qobject_cast<workstation::ui::ViewportWidget*>(w.centralWidget())) {
        printf("FAIL: central widget is not ViewportWidget\n");
        ok = false;
    }

    auto docks = w.findChildren<QDockWidget*>();
    if (docks.size() != 3) {
        printf("FAIL: dock widget count = %d (expected 3)\n", (int)docks.size());
        ok = false;
    }

    if (w.findChildren<workstation::ui::ModelTreeWidget*>().isEmpty()) {
        printf("FAIL: ModelTreeWidget not present\n");
        ok = false;
    }
    if (w.findChildren<workstation::ui::PropertiesWidget*>().isEmpty()) {
        printf("FAIL: PropertiesWidget not present\n");
        ok = false;
    }
    auto ts = w.findChildren<workstation::ui::ToolSettingsWidget*>();
    if (ts.isEmpty()) {
        printf("FAIL: ToolSettingsWidget not present\n");
        ok = false;
    } else if (ts.first()->findChild<QPlainTextEdit*>() == nullptr) {
        printf("FAIL: ToolSettingsWidget log widget (m_log) not present\n");
        ok = false;
    }

    QMenuBar* mb = w.menuBar();
    if (mb == nullptr || mb->actions().size() < 5) {
        printf("FAIL: menu bar missing or fewer than 5 menus\n");
        ok = false;
    }
    if (w.findChildren<QToolBar*>().isEmpty()) {
        printf("FAIL: toolbar missing\n");
        ok = false;
    }

    if (!w.isVisible()) {
        printf("FAIL: main window not visible after show()\n");
        ok = false;
    }

    // resize behavior
    w.resize(800, 600);
    if (w.width() != 800 || w.height() != 600) {
        printf("FAIL: resize ineffective (got %dx%d)\n", w.width(), w.height());
        ok = false;
    }

    // clean shutdown via event loop
    QTimer::singleShot(250, &w, [&w]() { w.close(); });
    int rc = app.exec();
    if (rc != 0) {
        printf("NOTE: app.exec returned %d\n", rc);
    }

    printf(ok ? "SMOKE_OK\n" : "SMOKE_FAIL\n");
    return ok ? 0 : 1;
}
