#pragma once
#include <QWidget>

class QLabel;
class QPlainTextEdit;

namespace workstation {
namespace ui {

// Bottom dock content: active tool state + read-only diagnostic log.
//
// GUI Shell 1 implements no CAD tools, so it shows "No active tool" and a
// log area that is currently used only for startup diagnostics.
class ToolSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolSettingsWidget(QWidget* parent = nullptr);

    // Append a line to the read-only diagnostic log.
    void appendLog(const QString& line);

private:
    QPlainTextEdit* m_log = nullptr;
};

} // namespace ui
} // namespace workstation
