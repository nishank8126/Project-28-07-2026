#include "workstation/ui/PropertiesWidget.h"

#include <QLabel>
#include <QVBoxLayout>

namespace workstation {
namespace ui {

PropertiesWidget::PropertiesWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    m_content = new QLabel("No selection");
    m_content->setWordWrap(true);
    layout->addWidget(m_content);
    layout->addStretch();
}

void PropertiesWidget::showPointCloudProperties(const QString& name, quint64 pointCount,
                                                 double sizeX, double sizeY, double sizeZ) {
    m_content->setText(QString(
        "<b>%1</b><br><br>"
        "Points: %2<br>"
        "Extent X: %3<br>"
        "Extent Y: %4<br>"
        "Extent Z: %5")
        .arg(name)
        .arg(pointCount)
        .arg(sizeX, 0, 'f', 2)
        .arg(sizeY, 0, 'f', 2)
        .arg(sizeZ, 0, 'f', 2));
}

} // namespace ui
} // namespace workstation
