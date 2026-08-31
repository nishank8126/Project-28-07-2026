#include "workstation/ui/ToolSettingsWidget.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace workstation {
namespace ui {

ToolSettingsWidget::ToolSettingsWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("No active tool"));

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    layout->addWidget(m_log);
}

void ToolSettingsWidget::appendLog(const QString& line) {
    m_log->appendPlainText(line);
}

} // namespace ui
} // namespace workstation
