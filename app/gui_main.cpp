#include <QApplication>
#include "workstation/ui/MainWindow.h"
#include "workstation/surface/SurfaceLog.h"

int main(int argc, char** argv) {
    // Surface pipeline log: recreated next to wk_gui.exe on every run.
    // It records surface initialisation, mesh generation, triangulation
    // results, GPU uploads, render decisions and every reason the surface
    // is skipped -- making shading/triangulation problems diagnosable from
    // disk alone. Per-frame states are logged only on change, so the file
    // stays small.
    SLOG_INFO("wk_gui session start - surface log: %s",
              workstation::surface::SurfaceLog::GetPath());

    QApplication app(argc, argv);
    QApplication::setApplicationName("WorkstationCAD");

    workstation::ui::MainWindow window;
    window.resize(1600, 900);
    window.show();

    return app.exec();
}
