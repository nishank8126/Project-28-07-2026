#pragma once
#include <QWidget>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace workstation {
namespace ui {

// Central CAD viewport area placeholder for GUI Shell 1.
//
// Role: reserve the central window surface and expose a native HWND so a
// future D3D11 renderer can attach. No D3D11 device, swap chain, shaders,
// camera, projection, or geometry is created here. paintEvent only draws
// UI-only placeholder text (not CAD rendering).
class ViewportWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewportWidget(QWidget* parent = nullptr);

    // Native window handle for future D3D11 viewport wiring.
    // GUI Shell 1 creates NO D3D11 objects; it only exposes the HWND.
#ifdef _WIN32
    HWND NativeHandle() const;
#endif

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace ui
} // namespace workstation
