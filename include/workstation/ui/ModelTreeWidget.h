#pragma once
#include <QWidget>
#include <QTreeWidgetItem>

class QTreeWidget;

namespace workstation {

namespace scene { class SceneManager; class SceneObject; }
class Document;

namespace ui {

class ModelTreeWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelTreeWidget(QWidget* parent = nullptr);

    void setSceneManager(scene::SceneManager* mgr);
    void refreshTree();
    void setLoadedPointCloud(const QString& name, quint64 pointCount);
    void setDocument(const Document* document);

signals:
    void objectSelected(uint64_t nodeID);
    void objectDeselected();

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void addObjectItems();
    void addLayerItems();

    QTreeWidget* m_tree = nullptr;
    scene::SceneManager* m_sceneManager = nullptr;
};

} // namespace ui
} // namespace workstation
