#include "workstation/ui/ModelTreeWidget.h"
#include "workstation/scene/SceneManager.h"
#include "workstation/scene/SceneObject.h"

#include <QTreeWidget>
#include <QVBoxLayout>

namespace workstation {
namespace ui {

enum ItemRole { NodeIDRole = Qt::UserRole + 1, ObjectTypeRole, LayerIDRole };

ModelTreeWidget::ModelTreeWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabel("Model Tree");
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked, this, &ModelTreeWidget::onItemClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ModelTreeWidget::onItemDoubleClicked);
}

void ModelTreeWidget::setSceneManager(scene::SceneManager* mgr) {
    m_sceneManager = mgr;
    refreshTree();
}

void ModelTreeWidget::setDocument(const Document* document) {
    m_tree->clear();
    Q_UNUSED(document);
    if (!m_sceneManager || m_sceneManager->GetObjectCount() == 0) {
        m_tree->addTopLevelItem(new QTreeWidgetItem(QStringList{"No document loaded"}));
    }
}

void ModelTreeWidget::setLoadedPointCloud(const QString& name, quint64 pointCount) {
    m_tree->clear();
    auto* item = new QTreeWidgetItem(QStringList{name});
    item->addChild(new QTreeWidgetItem(QStringList{
        QString("%1 points").arg(pointCount)}));
    m_tree->addTopLevelItem(item);
    m_tree->expandAll();
}

void ModelTreeWidget::refreshTree() {
    m_tree->clear();
    if (!m_sceneManager) {
        m_tree->addTopLevelItem(new QTreeWidgetItem(QStringList{"No scene loaded"}));
        return;
    }
    if (m_sceneManager->GetObjectCount() == 0) {
        m_tree->addTopLevelItem(new QTreeWidgetItem(QStringList{"No objects"}));
        return;
    }
    addObjectItems();
    addLayerItems();
    m_tree->expandAll();
}

void ModelTreeWidget::addObjectItems() {
    auto* objectsRoot = new QTreeWidgetItem(QStringList{"Objects"});
    objectsRoot->setExpanded(true);

    m_sceneManager->ForEachObject([this, objectsRoot](scene::SceneObject* obj) {
        if (!obj || !obj->GetNode()) return;
        auto* node = obj->GetNode();
        QString typeName = QString::fromUtf8(obj->GetTypeName());
        QString displayName = obj->GetDisplayName().empty()
            ? QString::fromStdString(node->GetName())
            : QString::fromStdString(obj->GetDisplayName());

        auto* item = new QTreeWidgetItem(QStringList{displayName, typeName});
        item->setData(0, NodeIDRole, node->GetID());
        item->setData(0, ObjectTypeRole, static_cast<int>(obj->GetType()));
        item->setCheckState(0, node->IsVisible() ? Qt::Checked : Qt::Unchecked);

        if (node->IsSelected()) {
            item->setSelected(true);
        }

        objectsRoot->addChild(item);
    });

    m_tree->addTopLevelItem(objectsRoot);
}

void ModelTreeWidget::addLayerItems() {
    const auto& layers = m_sceneManager->GetLayers();
    if (layers.empty()) return;

    auto* layersRoot = new QTreeWidgetItem(QStringList{"Layers"});
    layersRoot->setExpanded(true);

    for (const auto& [id, layer] : layers) {
        auto* item = new QTreeWidgetItem(QStringList{QString::fromStdString(layer.name)});
        item->setData(0, LayerIDRole, id);
        item->setCheckState(0, layer.visible ? Qt::Checked : Qt::Unchecked);
        layersRoot->addChild(item);
    }

    m_tree->addTopLevelItem(layersRoot);
}

void ModelTreeWidget::onItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item || !m_sceneManager) return;

    uint64_t nodeID = item->data(0, NodeIDRole).toULongLong();
    int layerID = item->data(0, LayerIDRole).toInt();

        if (nodeID > 0) {
        bool checked = item->checkState(0) == Qt::Checked;
        auto* obj = m_sceneManager->FindObject(nodeID);
        if (obj && obj->GetNode()) {
            obj->GetNode()->SetVisible(checked);
        }
    } else if (layerID > 0) {
        bool checked = item->checkState(0) == Qt::Checked;
        m_sceneManager->SetLayerVisible(static_cast<uint32_t>(layerID), checked);
    }
}

void ModelTreeWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item || !m_sceneManager) return;

    uint64_t nodeID = item->data(0, NodeIDRole).toULongLong();
    if (nodeID > 0) {
        auto* obj = m_sceneManager->FindObject(nodeID);
        if (obj) {
            m_sceneManager->ClearSelection();
            m_sceneManager->SetSelected(nodeID, true);
            emit objectSelected(nodeID);
        }
    }
}

} // namespace ui
} // namespace workstation
