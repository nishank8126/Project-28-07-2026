#include "workstation/ui/ViewportWidget.h"
#include "workstation/renderer/Renderer.h"
#include "workstation/renderer/VisualizationManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/LasFileReader.h"
#include "workstation/pointcloud/NormalEstimator.h"
#include "workstation/pointcloud/SntFileReader.h"

#include <chrono>
#include <cstdio>

#include <QExposeEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QElapsedTimer>

namespace workstation {
namespace ui {

ViewportWindow::ViewportWindow() {
    setSurfaceType(QWindow::VulkanSurface);
    resize(320, 240);

    m_renderer = std::make_unique<renderer::Renderer>();
    m_cloud = std::make_unique<pointcloud::PointCloud>();

    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ViewportWindow::onFrame);
    timer->start(16);
}

ViewportWindow::~ViewportWindow() {
    if (m_rendererInitialized) {
        m_renderer->Shutdown();
    }
}

void ViewportWindow::exposeEvent(QExposeEvent* event) {
    QWindow::exposeEvent(event);
    if (isExposed() && !m_rendererInitialized) {
        InitializeRenderer();
    }
}

bool ViewportWindow::InitializeRenderer() {
    if (width() <= 0 || height() <= 0) return false;

    renderer::RendererConfig config{};
    config.appName = "WorkstationCAD";
    config.initialWidth = static_cast<uint32_t>(width());
    config.initialHeight = static_cast<uint32_t>(height());
    config.enableValidation = true;
    config.enableImGui = false; // Qt panels are the UI here.

    if (!m_renderer->InitializeEmbedded(config, reinterpret_cast<void*>(winId()))) {
        emit statusChanged("Renderer: failed to initialize");
        return false;
    }

    m_rendererInitialized = true;
    auto& cam = m_renderer->GetContext().GetCamera();
    cam.SetPerspective(45.0, static_cast<double>(width()) / static_cast<double>(height()), 0.1, 1'000'000.0);

    // If a point cloud was loaded before the renderer initialized,
    // send it to the renderer and re-frame the camera on the cloud bounds.
    if (m_cloud && m_cloud->Root()) {
        m_renderer->SetPointCloud(m_cloud.get());
        auto bounds = m_cloud->Root()->bounds();
        cam.FocusOnBounds(bounds);

        auto& cfg = m_renderer->GetContext().GetConfig();
        // LAS/LiDAR source data is Z-up (Z = true elevation); see the
        // matching comment in shaders/point.vert's Elevation/HeightRamp cases.
        cfg.elevationMin = static_cast<float>(bounds.minZ);
        cfg.elevationMax = static_cast<float>(bounds.maxZ);
    }

    emit statusChanged("Renderer: ready");
    return true;
}

bool ViewportWindow::LoadPointCloudFile(const QString& path, QString* errorMessage) {
    auto newCloud = std::make_unique<pointcloud::PointCloud>();
    std::string err;
    if (!pointcloud::LoadLasFile(path.toStdString(), *newCloud, &err)) {
        if (errorMessage) *errorMessage = QString::fromStdString(err);
        return false;
    }

    m_cloud = std::move(newCloud);
    if (m_rendererInitialized) {
        m_renderer->SetPointCloud(m_cloud.get());
        auto bounds = m_cloud->Root() ? m_cloud->Root()->bounds() : pointcloud::BoundingBox{};
        auto& cam = m_renderer->GetContext().GetCamera();
        cam.FocusOnBounds(bounds);

        auto& cfg = m_renderer->GetContext().GetConfig();
        // LAS/LiDAR source data is Z-up (Z = true elevation); see the
        // matching comment in shaders/point.vert's Elevation/HeightRamp cases.
        cfg.elevationMin = static_cast<float>(bounds.minZ);
        cfg.elevationMax = static_cast<float>(bounds.maxZ);
    }
    return true;
}

bool ViewportWindow::LoadVectorOverlayFile(const QString& path, QString* errorMessage,
                                            quint32* outEntityCount, quint32* outPolylineCount) {
    pointcloud::SntEntities entities;
    std::string err;
    if (!pointcloud::LoadSntFile(path.toStdString(), entities, &err)) {
        if (errorMessage) *errorMessage = QString::fromStdString(err);
        return false;
    }

    if (outEntityCount) *outEntityCount = entities.headerEntityCount;
    if (outPolylineCount) *outPolylineCount = static_cast<quint32>(entities.polylines.size());

    if (m_rendererInitialized) {
        m_renderer->SetVectorOverlay(entities);
    }
    return true;
}

void ViewportWindow::SetVisualizationMode(int mode) {
    if (!m_rendererInitialized) return;

    auto vmode = static_cast<renderer::VisualizationMode>(mode);
    if (vmode == renderer::VisualizationMode::NormalShading && m_cloud && m_cloud->Root()) {
        auto* root = m_cloud->Root();
        if (!root->channels().GetChannel(pointcloud::ChannelId::Normals)) {
            auto* xyz = root->channels().GetChannel(pointcloud::ChannelId::XYZ);
            if (xyz && xyz->Data() && xyz->Count() > 0) {
                emit statusChanged("Computing normals (one-time, may take a while)...");

                auto t0 = std::chrono::steady_clock::now();
                std::vector<float> normals;
                pointcloud::EstimateNormalsFromPositions(
                    reinterpret_cast<const float*>(xyz->Data()), xyz->Count(), normals);
                double ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0).count();
                fprintf(stderr, "[ViewportWindow] Computed normals for %zu points in %.1f ms\n",
                        xyz->Count(), ms);

                root->channels().AddChannel(pointcloud::CreateChannel(
                    pointcloud::ChannelId::Normals, pointcloud::PointFormat::Float32,
                    xyz->Count(), normals.data()));
                m_renderer->RefreshNormals();

                emit statusChanged("Normals ready");
            }
        }
    }

    m_renderer->GetContext().GetConfig().visualizationMode = vmode;
}

quint64 ViewportWindow::GetLoadedPointCount() const {
    return m_cloud ? m_cloud->PointCount() : 0;
}

void ViewportWindow::GetLoadedExtent(double& sizeX, double& sizeY, double& sizeZ) const {
    sizeX = sizeY = sizeZ = 0.0;
    if (!m_cloud || !m_cloud->Root()) return;
    const auto& b = m_cloud->Root()->bounds();
    sizeX = b.maxX - b.minX;
    sizeY = b.maxY - b.minY;
    sizeZ = b.maxZ - b.minZ;
}

double ViewportWindow::GetLastFPS() const {
    if (!m_rendererInitialized) return 0.0;
    return m_renderer->GetContext().GetStats().fps;
}

void ViewportWindow::resizeEvent(QResizeEvent* event) {
    QWindow::resizeEvent(event);
    if (!m_rendererInitialized) {
        if (isExposed()) InitializeRenderer();
        return;
    }
    if (width() > 0 && height() > 0) {
        m_renderer->OnResize(static_cast<uint32_t>(width()), static_cast<uint32_t>(height()));
        auto& cam = m_renderer->GetContext().GetCamera();
        cam.SetAspectRatio(static_cast<double>(width()) / static_cast<double>(height()));
    }
}

void ViewportWindow::onFrame() {
    if (!m_rendererInitialized) return;

    static QElapsedTimer clock;
    static bool clockStarted = false;
    if (!clockStarted) { clock.start(); clockStarted = true; }
    qint64 nowNs = clock.nsecsElapsed();
    float dt = m_lastFrameTimeNs == 0 ? 0.016f
        : static_cast<float>(nowNs - m_lastFrameTimeNs) / 1e9f;
    m_lastFrameTimeNs = nowNs;

    ApplyHeldKeyMovement(dt);

    m_renderer->BeginFrame();
    m_renderer->RenderFrame();
    m_renderer->EndFrame();
}

void ViewportWindow::ApplyHeldKeyMovement(float dt) {
    auto& cam = m_renderer->GetContext().GetCamera();
    const float speed = 50.0f * dt;
    if (m_heldKeys.contains(Qt::Key_W)) cam.MoveForward(speed);
    if (m_heldKeys.contains(Qt::Key_S)) cam.MoveForward(-speed);
    if (m_heldKeys.contains(Qt::Key_A)) cam.MoveRight(-speed);
    if (m_heldKeys.contains(Qt::Key_D)) cam.MoveRight(speed);
    if (m_heldKeys.contains(Qt::Key_Q)) cam.MoveUp(speed);
    if (m_heldKeys.contains(Qt::Key_E)) cam.MoveUp(-speed);
}

void ViewportWindow::keyPressEvent(QKeyEvent* event) {
    m_heldKeys.insert(event->key());
}

void ViewportWindow::keyReleaseEvent(QKeyEvent* event) {
    m_heldKeys.remove(event->key());
}

void ViewportWindow::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();
    if (event->button() == Qt::RightButton) m_rotating = true;
    if (event->button() == Qt::MiddleButton) m_panning = true;
}

void ViewportWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) m_rotating = false;
    if (event->button() == Qt::MiddleButton) m_panning = false;
}

void ViewportWindow::mouseMoveEvent(QMouseEvent* event) {
    if (!m_rendererInitialized) return;
    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    auto& cam = m_renderer->GetContext().GetCamera();
    if (m_rotating) {
        cam.Rotate(delta.x() * 0.3f, delta.y() * 0.3f);
    } else if (m_panning) {
        cam.Pan(delta.x() * 0.05f, delta.y() * 0.05f);
    }
}

void ViewportWindow::wheelEvent(QWheelEvent* event) {
    if (!m_rendererInitialized) return;
    auto& cam = m_renderer->GetContext().GetCamera();
    cam.Zoom(event->angleDelta().y() / 120.0f * 5.0f);
}

} // namespace ui
} // namespace workstation
