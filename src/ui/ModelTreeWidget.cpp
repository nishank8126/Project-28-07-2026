#include "workstation/ui/ModelTreeWidget.h"

#include <QTreeWidget>
#include <QVBoxLayout>

namespace workstation {
namespace ui {

ModelTreeWidget::ModelTreeWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabel("Model Tree");
    layout->addWidget(m_tree);
}

void ModelTreeWidget::setDocument(const Document* document) {
    m_tree->clear();
    // Truthful state only. No CAD elements are fabricated. The existing core
    // exposes ModelCount() but no enumeration accessor, and no document is
    // loaded in GUI Shell 1, so the empty state is shown.
    Q_UNUSED(document);
    m_tree->addTopLevelItem(new QTreeWidgetItem(QStringList{"No document loaded"}));
}

void ModelTreeWidget::setLoadedPointCloud(const QString& name, quint64 pointCount) {
    m_tree->clear();
    auto* item = new QTreeWidgetItem(QStringList{name});
    item->addChild(new QTreeWidgetItem(QStringList{
        QString("%1 points").arg(pointCount)}));
    m_tree->addTopLevelItem(item);
    m_tree->expandAll();
}

} // namespace ui
} // namespace workstation
