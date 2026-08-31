#include <QApplication>
#include "workstation/ui/MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("WorkstationCAD");

    workstation::ui::MainWindow window;
    window.resize(1600, 900);
    window.show();

    return app.exec();
}
