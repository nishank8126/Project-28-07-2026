#include "workstation/ui/PropertiesWidget.h"
#include "workstation/scene/SceneManager.h"
#include "workstation/scene/SceneObject.h"
#include "workstation/scene/CadSceneObject.h"
#include "workstation/scene/PointCloudSceneObject.h"

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

void PropertiesWidget::setSceneManager(scene::SceneManager* mgr) {
    m_sceneManager = mgr;
}

void PropertiesWidget::showPointCloudProperties(const QString& name, quint64 pointCount,
                                                  double sizeX, double sizeY, double sizeZ) {
    m_content->setText(QString(
        "<b>%1</b><br><br>"
        "Type: Point Cloud<br>"
        "Points: %2<br>"
        "Extent X: %3 m<br>"
        "Extent Y: %4 m<br>"
        "Extent Z: %5 m")
        .arg(name)
        .arg(pointCount)
        .arg(sizeX, 0, 'f', 3)
        .arg(sizeY, 0, 'f', 3)
        .arg(sizeZ, 0, 'f', 3));
}

void PropertiesWidget::showCadAttachmentProperties(scene::SceneObject* obj) {
    if (!obj) { showEmptyState(); return; }
    showObjectProperties(obj);
}

void PropertiesWidget::onSelectionChanged(uint64_t nodeID) {
    if (!m_sceneManager) return;
    auto* obj = m_sceneManager->FindObject(nodeID);
    if (obj) {
        showObjectProperties(obj);
    }
}

void PropertiesWidget::onSelectionCleared() {
    showEmptyState();
}

void PropertiesWidget::onSelectionInfoChanged(const QString& info) {
    m_content->setText(info);
}

void PropertiesWidget::showObjectProperties(scene::SceneObject* obj) {
    if (!obj || !obj->GetNode()) { showEmptyState(); return; }

    auto* node = obj->GetNode();
    QString typeName = QString::fromUtf8(obj->GetTypeName());
    QString name = obj->GetDisplayName().empty()
        ? QString::fromStdString(node->GetName())
        : QString::fromStdString(obj->GetDisplayName());

    auto bounds = node->GetWorldBounds();
    double extentX = bounds.maxX - bounds.minX;
    double extentY = bounds.maxY - bounds.minY;
    double extentZ = bounds.maxZ - bounds.minZ;

    QString info = QString(
        "<b>%1</b><br><br>"
        "Type: %2<br>"
        "Node ID: %3<br>"
        "Visible: %4<br>"
        "Selected: %5<br>"
        "Layer: %6<br><br>"
        "<b>Bounds:</b><br>"
        "X: %7 to %8 (%9 m)<br>"
        "Y: %10 to %11 (%12 m)<br>"
        "Z: %13 to %14 (%15 m)")
        .arg(name)
        .arg(typeName)
        .arg(node->GetID())
        .arg(node->IsVisible() ? "Yes" : "No")
        .arg(node->IsSelected() ? "Yes" : "No")
        .arg(node->GetLayerID())
        .arg(bounds.minX, 0, 'f', 3).arg(bounds.maxX, 0, 'f', 3).arg(extentX, 0, 'f', 3)
        .arg(bounds.minY, 0, 'f', 3).arg(bounds.maxY, 0, 'f', 3).arg(extentY, 0, 'f', 3)
        .arg(bounds.minZ, 0, 'f', 3).arg(bounds.maxZ, 0, 'f', 3).arg(extentZ, 0, 'f', 3);

    if (obj->GetType() == scene::ObjectType::CadAttachment) {
        auto* cad = static_cast<scene::CadSceneObject*>(obj);
        QString cadType;
        switch (cad->GetCadType()) {
            case scene::CadSceneObject::CadType::DXF: cadType = "DXF"; break;
            case scene::CadSceneObject::CadType::DWG: cadType = "DWG"; break;
            case scene::CadSceneObject::CadType::SNT: cadType = "SN&T"; break;
        }
        info += QString("<br><b>CAD:</b> %1").arg(cadType);
    } else if (obj->GetType() == scene::ObjectType::PointCloud) {
        auto* pc = static_cast<scene::PointCloudSceneObject*>(obj);
        info += QString("<br><b>Points:</b> %1").arg(pc->GetPointCount());
    }

    m_content->setText(info);
}

void PropertiesWidget::showEmptyState() {
    m_content->setText("No selection");
}

} // namespace ui
} // namespace workstation
