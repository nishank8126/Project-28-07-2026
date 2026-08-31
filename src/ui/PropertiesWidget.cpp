#include "workstation/ui/PropertiesWidget.h"

#include <QLabel>
#include <QVBoxLayout>

namespace workstation {
namespace ui {

PropertiesWidget::PropertiesWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(new QLabel("No selection"));
}

} // namespace ui
} // namespace workstation
