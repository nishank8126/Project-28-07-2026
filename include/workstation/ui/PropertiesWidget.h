#pragma once
#include <QWidget>
#include <cstdint>

class QLabel;

namespace workstation {

namespace scene { class SceneManager; class SceneObject; }

namespace ui {

class PropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesWidget(QWidget* parent = nullptr);

    void setSceneManager(scene::SceneManager* mgr);

    void showPointCloudProperties(const QString& name, quint64 pointCount,
                                   double sizeX, double sizeY, double sizeZ);
    void showCadAttachmentProperties(scene::SceneObject* obj);

public slots:
    void onSelectionChanged(uint64_t nodeID);
    void onSelectionCleared();
    void onSelectionInfoChanged(const QString& info);

private:
    void showObjectProperties(scene::SceneObject* obj);
    void showEmptyState();

    QLabel* m_content = nullptr;
    scene::SceneManager* m_sceneManager = nullptr;
};

} // namespace ui
} // namespace workstation
