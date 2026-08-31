#include "workstation/ui/ViewportWidget.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPalette>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace workstation {
namespace ui {

ViewportWidget::ViewportWidget(QWidget* parent) : QWidget(parent) {
    // Reserve a native window surface so a future D3D11 renderer can attach
    // via winId()/NativeHandle(). No D3D11 device/swap chain is created.
    setAttribute(Qt::WA_NativeWindow);
    setMinimumSize(320, 240);
}

#ifdef _WIN32
HWND ViewportWidget::NativeHandle() const {
    return reinterpret_cast<HWND>(winId());
}
#endif

void ViewportWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setPen(Qt::lightGray);
    p.drawText(rect(), Qt::AlignCenter,
               "WorkstationCAD Viewport\nRenderer not connected\nGUI Shell 1");
}

} // namespace ui
} // namespace workstation
