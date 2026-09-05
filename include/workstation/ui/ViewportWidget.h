#pragma once
#include <QWindow>
#include <QSet>
#include <QPoint>

#include <memory>
#include <string>
#include <future>
#include <atomic>

#include "workstation/scene/SceneSelectionManager.h"
#include "workstation/renderer/SelectionRenderer.h"
#include "workstation/renderer/OverlayRenderer.h"
#include "workstation/core/CommandManager.h"
#include "workstation/surface/SurfaceRenderer.h"
#include "workstation/surface/SurfaceMeshCache.h"
#include "workstation/surface/ElevationCache.h"

namespace workstation {

namespace pointcloud { class PointCloud; }
namespace renderer { class Renderer; }
namespace scene { class SceneManager; }
namespace cad { class DxfAttachment; class DwgAttachment; class SntAttachment; }

namespace ui {

class ViewportWindow : public QWindow {
    Q_OBJECT
public:
    explicit ViewportWindow();
    ~ViewportWindow() override;

    bool LoadPointCloudFile(const QString& path, QString* errorMessage = nullptr);

    bool LoadVectorOverlayFile(const QString& path, QString* errorMessage = nullptr,
                                quint32* outEntityCount = nullptr,
                                quint32* outPolylineCount = nullptr);

    bool LoadDxfAttachment(cad::DxfAttachment* attachment, QString* errorMessage = nullptr);
    bool LoadDwgAttachment(cad::DwgAttachment* attachment, QString* errorMessage = nullptr);
    bool LoadSntAttachment(cad::SntAttachment* attachment, QString* errorMessage = nullptr);
    void FocusCameraOnLastCadAttachment();
    void RemoveCadAttachment(cad::DxfAttachment* attachment);
    void RemoveCadAttachment(cad::DwgAttachment* attachment);
    void RemoveCadAttachment(cad::SntAttachment* attachment);
    void RemoveAllCadAttachments();
    void SetCadLayerVisibility(const std::string& layerName, bool visible);

    bool IsRendererReady() const { return m_rendererInitialized; }
    double GetLastFPS() const;
    quint64 GetLoadedPointCount() const;
    void GetLoadedExtent(double& sizeX, double& sizeY, double& sizeZ) const;

    scene::SceneManager* GetSceneManager() const;
    scene::SceneSelectionManager* GetSelectionManager() { return &selectionManager_; }
    core::CommandManager* GetCommandManager() { return &commandManager_; }
    renderer::SelectionRenderer* GetSelectionRenderer() { return &selectionRenderer_; }
    renderer::OverlayRenderer* GetOverlayRenderer() { return &overlayRenderer_; }

    void SetVisualizationMode(int mode);

    void Undo();
    void Redo();

    bool IsSelectMode() const { return selectMode_; }
    void SetSelectMode(bool on) { selectMode_ = on; }

    surface::SurfaceRenderer* GetSurfaceRenderer();
    surface::SurfaceMeshCache* GetSurfaceCache() { return &surfaceCache_; }

    void SetSurfaceMode(int mode);
    void SetSurfaceShading(int shading);
    void GenerateSurfaceForCloud();

    // Shading quality preset: 0 Low, 1 Medium, 2 High. Tunes generation
    // params (normal neighbourhood, max edge length) and regenerates the
    // surface when one already exists.
    void SetSurfaceQuality(int quality);
    void SetSurfaceMaxEdgeLength(double maxEdgeLength);
    void SetSurfaceNeighborRadius(double radius);
    void SetSurfaceLightDirection(float x, float y, float z);
    void SetSurfaceMaterial(float ambient, float diffuse, float specular, float shininess);
    void ToggleSurface();

    // Surface parameters used by the next GenerateSurfaceForCloud() call.
    void SetSurfaceGenParams(const surface::SurfaceGenerationParams& params) { m_surfaceGenParams = params; }
    const surface::SurfaceGenerationParams& GetSurfaceGenParams() const { return m_surfaceGenParams; }

    void LoadClassificationPTC(const QString& path, QString* error = nullptr);
    void ClearCustomClassificationPalette();
    void UpdateClassificationVisibility(int classCode, bool visible);

signals:
    void statusChanged(const QString& text);
    void selectionChanged(uint64_t objectID);
    void selectionCleared();
    void undoStateChanged(bool canUndo, bool canRedo, const QString& undoName, const QString& redoName);

protected:
    void exposeEvent(QExposeEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onFrame();

private:
    bool InitializeRenderer();
    void ApplyHeldKeyMovement(float dt);

    void HandlePicking(int mouseX, int mouseY);

    std::unique_ptr<renderer::Renderer> m_renderer;
    std::unique_ptr<pointcloud::PointCloud> m_cloud;

    bool m_rendererInitialized = false;
    QSet<int> m_heldKeys;
    QPoint m_lastMousePos;
    bool m_rotating = false;
    bool m_panning = false;
    bool selectMode_ = true;

    qint64 m_lastFrameTimeNs = 0;

    scene::SceneSelectionManager selectionManager_;
    core::CommandManager commandManager_;
    renderer::SelectionRenderer selectionRenderer_;
    renderer::OverlayRenderer overlayRenderer_;
    surface::SurfaceMeshCache surfaceCache_;
    surface::ElevationCache elevationCache_;
    surface::SurfaceGenerationParams m_surfaceGenParams;
    bool surfaceVisible_ = false;
    // Progressive elevation generation
    std::future<void> pendingElevationGen_;
    std::atomic<bool> elevationGenComplete_{false};
    std::atomic<uint32_t> elevationReadyResolution_{0};
};

} // namespace ui
} // namespace workstation
