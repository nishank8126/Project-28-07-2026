#pragma once
#include <QWindow>
#include <QSet>
#include <QPoint>

#include <memory>
#include <string>

namespace workstation {

namespace pointcloud { class PointCloud; }
namespace renderer { class Renderer; }

namespace ui {

// Central CAD viewport: a real QWindow (not a plain QWidget) hosting the
// Vulkan point-cloud renderer directly against its native Win32 surface.
//
// This must be a QWindow embedded via QWidget::createWindowContainer()
// rather than a QWidget painted on screen (WA_PaintOnScreen): Qt6's own
// compositing of sibling dock widgets is unreliable with the latter and can
// leave the natively-rendered surface invisible even while it renders
// correctly underneath.
class ViewportWindow : public QWindow {
    Q_OBJECT
public:
    explicit ViewportWindow();
    ~ViewportWindow() override;

    // Loads a .las/.laz file and displays it. Returns false (and fills
    // errorMessage) on failure.
    bool LoadPointCloudFile(const QString& path, QString* errorMessage = nullptr);

    bool IsRendererReady() const { return m_rendererInitialized; }
    double GetLastFPS() const;
    quint64 GetLoadedPointCount() const;
    void GetLoadedExtent(double& sizeX, double& sizeY, double& sizeZ) const;

signals:
    void statusChanged(const QString& text);

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

    std::unique_ptr<renderer::Renderer> m_renderer;
    std::unique_ptr<pointcloud::PointCloud> m_cloud;

    bool m_rendererInitialized = false;
    QSet<int> m_heldKeys;
    QPoint m_lastMousePos;
    bool m_rotating = false;
    bool m_panning = false;

    qint64 m_lastFrameTimeNs = 0;
};

} // namespace ui
} // namespace workstation
