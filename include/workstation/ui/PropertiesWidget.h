#pragma once
#include <QWidget>

class QLabel;

namespace workstation {
namespace ui {

// Right dock content: properties of the current selection.
//
// GUI Shell 1 has no selection algorithm, so it shows the truthful
// "No selection" state. No properties are inferred.
class PropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesWidget(QWidget* parent = nullptr);

    void showPointCloudProperties(const QString& name, quint64 pointCount,
                                   double sizeX, double sizeY, double sizeZ);

private:
    QLabel* m_content = nullptr;
};

} // namespace ui
} // namespace workstation
