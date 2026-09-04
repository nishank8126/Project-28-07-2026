#include "workstation/ui/ViewportWidget.h"
#include "workstation/renderer/Renderer.h"
#include "workstation/renderer/VisualizationManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/LasFileReader.h"
#include "workstation/pointcloud/NormalEstimator.h"
#include "workstation/pointcloud/SntFileReader.h"
#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/DwgAttachment.h"
#include "workstation/cad/SntAttachment.h"
#include "workstation/scene/SceneManager.h"
#include "workstation/scene/CadSceneObject.h"
#include "workstation/core/Commands.h"

#include <algorithm>
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

    selectionRenderer_.Initialize();
    overlayRenderer_.Initialize();

    commandManager_.SetHistoryChangedCallback([this]() {
        emit undoStateChanged(
            commandManager_.CanUndo(), commandManager_.CanRedo(),
            QString::fromStdString(commandManager_.GetUndoActionName()),
            QString::fromStdString(commandManager_.GetRedoActionName()));
    });

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
    if (!pointcloud::LoadLasFile(path.toStdString(), *newCloud, &err, &m_renderer->GetCoordNormalizer())) {
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
    // SurfaceShading (point.frag case 13, "normals + depth composite") reads
    // inNormal exactly like NormalShading does - it needs this same one-time
    // computation. Without it, PointCloudRenderAdapter::ExtractPointCloudData
    // fills every point's normal with a fixed straight-up (0,1,0) fallback
    // (there's no per-point Normals channel for a freshly loaded LAS/LAZ
    // cloud), so every point gets the *same* lighting dot product regardless
    // of the surface it's actually on - a flat, uniformly dim result that
    // reads as a featureless dark/black cloud instead of real shading.
    bool needsNormals = (vmode == renderer::VisualizationMode::NormalShading ||
                          vmode == renderer::VisualizationMode::SurfaceShading);
    if (needsNormals && m_cloud && m_cloud->Root()) {
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

bool ViewportWindow::LoadDxfAttachment(cad::DxfAttachment* attachment, QString* errorMessage) {
    if (!attachment) { if (errorMessage) *errorMessage = "Null attachment"; return false; }
    if (m_rendererInitialized) {
        m_renderer->LoadDxfAttachment(attachment);
        auto& sceneMgr = m_renderer->GetSceneManager();
        auto cadObj = std::make_unique<scene::CadSceneObject>();
        cadObj->SetCadType(scene::CadSceneObject::CadType::DXF);
        cadObj->SetFilePath(attachment->filepath());
        cadObj->SetDisplayName(QString::fromStdString(attachment->filepath()).section('/', -1).toStdString());
        cadObj->SetDxfAttachment(attachment);
        cadObj->SetCadRenderer(&m_renderer->GetCadRenderer());
        sceneMgr.AddObject(std::move(cadObj), "DXF Attachment");
        FocusCameraOnLastCadAttachment();
    }
    return true;
}

bool ViewportWindow::LoadDwgAttachment(cad::DwgAttachment* attachment, QString* errorMessage) {
    if (!attachment) { if (errorMessage) *errorMessage = "Null attachment"; return false; }
    if (m_rendererInitialized) {
        m_renderer->LoadDwgAttachment(attachment);
        auto& sceneMgr = m_renderer->GetSceneManager();
        auto cadObj = std::make_unique<scene::CadSceneObject>();
        cadObj->SetCadType(scene::CadSceneObject::CadType::DWG);
        cadObj->SetFilePath(attachment->filepath());
        cadObj->SetDisplayName(QString::fromStdString(attachment->filepath()).section('/', -1).toStdString());
        cadObj->SetDwgAttachment(attachment);
        cadObj->SetCadRenderer(&m_renderer->GetCadRenderer());
        sceneMgr.AddObject(std::move(cadObj), "DWG Attachment");
        FocusCameraOnLastCadAttachment();
    }
    return true;
}

bool ViewportWindow::LoadSntAttachment(cad::SntAttachment* attachment, QString* errorMessage) {
    if (!attachment) { if (errorMessage) *errorMessage = "Null attachment"; return false; }
    if (m_rendererInitialized) {
        m_renderer->LoadSntAttachment(attachment);
        auto& sceneMgr = m_renderer->GetSceneManager();
        auto cadObj = std::make_unique<scene::CadSceneObject>();
        cadObj->SetCadType(scene::CadSceneObject::CadType::SNT);
        cadObj->SetFilePath(attachment->filepath());
        cadObj->SetDisplayName(QString::fromStdString(attachment->filepath()).section('/', -1).toStdString());
        cadObj->SetSntAttachment(attachment);
        cadObj->SetCadRenderer(&m_renderer->GetCadRenderer());
        sceneMgr.AddObject(std::move(cadObj), "SNT Attachment");
        FocusCameraOnLastCadAttachment();
    }
    return true;
}

void ViewportWindow::FocusCameraOnLastCadAttachment() {
    // Nothing previously moved the camera to a freshly attached CAD/SNT
    // object's own bounds. If a point cloud was already loaded (the common
    // order: load LAZ, then attach SNT as a reference), the camera stayed
    // framed on just the cloud - and since the SNT's real-world coordinates
    // don't necessarily fall inside that frame, the newly attached geometry
    // could be well outside the visible frustum and never show up. Frame the
    // union of the cloud (if any) and the new attachment instead of either
    // skipping the refocus or jumping away from the cloud entirely.
    const auto& attachments = m_renderer->GetCadRenderer().GetAttachments();
    if (attachments.empty()) return;

    const auto& newBounds = attachments.back().bounds;
    spatial::BoundingBox combined = newBounds;
    bool haveCloud = m_cloud && m_cloud->Root();
    double cloudMaxDim = 0.0;
    if (haveCloud) {
        const auto& cb = m_cloud->Root()->bounds();
        cloudMaxDim = std::max({cb.maxX - cb.minX, cb.maxY - cb.minY, cb.maxZ - cb.minZ});
        combined.minX = std::min(combined.minX, cb.minX);
        combined.minY = std::min(combined.minY, cb.minY);
        combined.minZ = std::min(combined.minZ, cb.minZ);
        combined.maxX = std::max(combined.maxX, cb.maxX);
        combined.maxY = std::max(combined.maxY, cb.maxY);
        combined.maxZ = std::max(combined.maxZ, cb.maxZ);
    }

    double combinedMaxDim = std::max({combined.maxX - combined.minX,
                                       combined.maxY - combined.minY,
                                       combined.maxZ - combined.minZ});

    // If the new attachment sits nowhere near the already-loaded cloud (e.g.
    // an SNT from an unrelated survey site attached over an unrelated point
    // cloud - both perfectly valid individually, in real-world coordinates
    // that just don't overlap), a "union" refocus would retreat the camera
    // so far that BOTH pieces of geometry shrink to sub-pixel and vanish -
    // not "nothing rendered", just imperceptibly tiny. Only union-refocus
    // when the combined extent is still a reasonable multiple of what the
    // cloud alone needed; otherwise leave the working view alone rather than
    // silently breaking it, and say why.
    constexpr double kMaxReasonableGrowth = 50.0;
    if (haveCloud && cloudMaxDim > 0.0 && combinedMaxDim > cloudMaxDim * kMaxReasonableGrowth) {
        emit statusChanged("Attached geometry is far outside the current view "
                            "(coordinates don't overlap the loaded point cloud) - "
                            "camera view was not changed");
        return;
    }

    auto& cam = m_renderer->GetContext().GetCamera();
    cam.FocusOnBounds(combined);
}

void ViewportWindow::RemoveCadAttachment(cad::DxfAttachment* attachment) {
    if (m_rendererInitialized) m_renderer->RemoveDxfAttachment(attachment);
}

void ViewportWindow::RemoveCadAttachment(cad::DwgAttachment* attachment) {
    if (m_rendererInitialized) m_renderer->RemoveDwgAttachment(attachment);
}

void ViewportWindow::RemoveCadAttachment(cad::SntAttachment* attachment) {
    if (m_rendererInitialized) m_renderer->RemoveSntAttachment(attachment);
}

void ViewportWindow::RemoveAllCadAttachments() {
    if (m_rendererInitialized) m_renderer->RemoveAllCadAttachments();
}

void ViewportWindow::SetCadLayerVisibility(const std::string& layerName, bool visible) {
    if (m_rendererInitialized) m_renderer->SetCadLayerVisibility(layerName, visible);
}

scene::SceneManager* ViewportWindow::GetSceneManager() const {
    return m_rendererInitialized ? &m_renderer->GetSceneManager() : nullptr;
}

void ViewportWindow::HandlePicking(int mouseX, int mouseY) {
    if (!m_rendererInitialized) return;

    auto* sceneMgr = GetSceneManager();
    if (!sceneMgr) return;

    selectionManager_.SetSceneManager(sceneMgr);

    auto& cam = m_renderer->GetContext().GetCamera();
    int vpW = m_renderer->GetContext().GetViewportWidth();
    int vpH = m_renderer->GetContext().GetViewportHeight();

    auto result = selectionManager_.Pick(mouseX, mouseY, vpW, vpH, cam,
                                          scene::SelectionMode::Single);

    if (result.valid) {
        std::vector<scene::SelectionResult> selections;
        selections.push_back(result);
        selectionRenderer_.UpdateSelection(selections);
        sceneMgr->ClearSelection();
        sceneMgr->SetSelected(result.objectID, true);
        emit selectionChanged(result.objectID);
    } else {
        selectionRenderer_.Clear();
        sceneMgr->ClearSelection();
        emit selectionCleared();
    }
}

void ViewportWindow::Undo() {
    commandManager_.Undo();
}

void ViewportWindow::Redo() {
    commandManager_.Redo();
}

void ViewportWindow::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton && selectMode_) {
        HandlePicking(event->pos().x(), event->pos().y());
    } else if (event->button() == Qt::RightButton) {
        m_rotating = true;
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = true;
    }
}

void ViewportWindow::SetSurfaceMode(int mode) {
    auto* sr = GetSurfaceRenderer();
    if (!sr) return;

    sr->SetMode(static_cast<surface::SurfaceMode>(mode));

    // The Display Mode radio buttons must work on their own: the first switch
    // to a surface mode builds the mesh. Previously only the separate
    // "Generate Surface" menu action did, so Surface/Hybrid/Wireframe showed
    // nothing when the user just changed the display mode.
    // Points mode (0) is served entirely by the point pipeline - no mesh.
    if (mode == static_cast<int>(surface::SurfaceMode::Points)) return;
    if (!m_rendererInitialized || !m_cloud || !m_cloud->Root()) return;

    if (sr->GetMeshCount() == 0) {
        GenerateSurfaceForCloud();  // also marks the surface visible
    } else if (surfaceVisible_) {
        sr->SetVisible(true);
    }
}

void ViewportWindow::SetSurfaceShading(int shading) {
    if (auto* sr = GetSurfaceRenderer())
        sr->SetShading(static_cast<surface::ShadingType>(shading));
}

void ViewportWindow::GenerateSurfaceForCloud() {
    if (!m_rendererInitialized || !m_cloud || !m_cloud->Root()) return;

    auto* sr = GetSurfaceRenderer();
    if (!sr) return;

    surface::SurfaceGenerationParams params = m_surfaceGenParams;
    params.computeNormals = true;

    sr->GenerateLODs(*m_cloud, params);
    surfaceVisible_ = true;
    sr->SetVisible(true);
}

void ViewportWindow::SetSurfaceQuality(int quality) {
    // Low / Medium / High: normal neighbourhood count affects smoothness,
    // max edge length scales how far triangulation may bridge gaps.
    switch (quality) {
        case 0: // Low
            m_surfaceGenParams.normalNeighborCount = 6;
            m_surfaceGenParams.maxEdgeLength = 0.0;   // automatic
            m_surfaceGenParams.adaptiveTriangulation = true;
            break;
        case 2: // High
            m_surfaceGenParams.normalNeighborCount = 16;
            m_surfaceGenParams.maxEdgeLength = 0.0;   // automatic
            m_surfaceGenParams.adaptiveTriangulation = true;
            break;
        default: // Medium
            m_surfaceGenParams.normalNeighborCount = 10;
            m_surfaceGenParams.maxEdgeLength = 0.0;
            m_surfaceGenParams.adaptiveTriangulation = true;
            break;
    }

    // Rebuild in place if a surface is already displayed.
    if (surfaceVisible_ && m_cloud && m_cloud->Root()) {
        auto* sr = GetSurfaceRenderer();
        if (sr) {
            sr->ClearAllMeshes();
            GenerateSurfaceForCloud();
        }
    }
}

void ViewportWindow::SetSurfaceMaxEdgeLength(double maxEdgeLength) {
    m_surfaceGenParams.maxEdgeLength = maxEdgeLength;
    if (surfaceVisible_ && m_cloud && m_cloud->Root()) {
        auto* sr = GetSurfaceRenderer();
        if (sr) {
            sr->ClearAllMeshes();
            GenerateSurfaceForCloud();
        }
    }
}

void ViewportWindow::SetSurfaceNeighborRadius(double radius) {
    m_surfaceGenParams.normalNeighborRadius = radius;
    if (surfaceVisible_ && m_cloud && m_cloud->Root()) {
        auto* sr = GetSurfaceRenderer();
        if (sr) {
            sr->ClearAllMeshes();
            GenerateSurfaceForCloud();
        }
    }
}

void ViewportWindow::SetSurfaceLightDirection(float x, float y, float z) {
    if (!m_rendererInitialized || !m_renderer) return;
    auto& cfg = m_renderer->GetContext().GetConfig();
    cfg.surfaceLightDirX = x;
    cfg.surfaceLightDirY = y;
    cfg.surfaceLightDirZ = z;
}

void ViewportWindow::SetSurfaceMaterial(float ambient, float diffuse,
                                        float specular, float shininess) {
    if (!m_rendererInitialized || !m_renderer) return;
    auto& cfg = m_renderer->GetContext().GetConfig();
    cfg.surfaceAmbient = ambient;
    cfg.surfaceDiffuse = diffuse;
    cfg.surfaceSpecular = specular;
    cfg.surfaceShininess = shininess;
}

void ViewportWindow::ToggleSurface() {
    auto* sr = GetSurfaceRenderer();
    if (!sr) return;

    if (sr->GetMeshCount() == 0) {
        GenerateSurfaceForCloud();
    } else {
        surfaceVisible_ = !surfaceVisible_;
        sr->SetVisible(surfaceVisible_);
    }
}

surface::SurfaceRenderer* ViewportWindow::GetSurfaceRenderer() {
    if (!m_renderer) return nullptr;
    return &m_renderer->GetSurfaceRenderer();
}

void ViewportWindow::LoadClassificationPTC(const QString& path, QString* error) {
    if (!m_renderer) {
        if (error) *error = "Renderer not initialized";
        return;
    }
    std::string stdError;
    m_renderer->LoadClassificationPTC(path.toStdString(), &stdError);
    if (!stdError.empty()) {
        if (error) *error = QString::fromStdString(stdError);
        return;
    }
    // Auto-switch to Classification visualization mode so the loaded palette is visible.
    SetVisualizationMode(2);
}

void ViewportWindow::ClearCustomClassificationPalette() {
    if (m_renderer) m_renderer->ClearCustomClassificationPalette();
}

void ViewportWindow::UpdateClassificationVisibility(int classCode, bool visible) {
    if (m_renderer) m_renderer->UpdateClassificationVisibility(classCode, visible);
}

} // namespace ui
} // namespace workstation
