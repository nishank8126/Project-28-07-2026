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
#include <cmath>
#include <cstdio>
#include <set>

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
    config.enableDebugReadback = true;  // Enable debug readback for culling validation

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
        cam.SetTopView(bounds);

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
        cam.SetTopView(bounds);

        // -------------------------------------------------------------------
        // TOP VIEW DEBUG
        // -------------------------------------------------------------------
        auto pos = cam.GetPosition();
        auto tgt = cam.GetTarget();
        auto fwd = cam.GetForward();
        auto right = cam.GetRight();
        auto up = cam.GetUp();
        fprintf(stderr,
            "\n[TOP VIEW DEBUG]\n"
            "  Bounds:\n"
            "    minX: %.3f maxX: %.3f\n"
            "    minY: %.3f maxY: %.3f\n"
            "    minZ: %.3f maxZ: %.3f\n"
            "  Camera:\n"
            "    position: (%.3f, %.3f, %.3f)\n"
            "    target:   (%.3f, %.3f, %.3f)\n"
            "    forward:  (%.4f, %.4f, %.4f)\n"
            "    right:    (%.4f, %.4f, %.4f)\n"
            "    up:       (%.4f, %.4f, %.4f)\n"
            "    worldUp:  (%.4f, %.4f, %.4f)\n"
            "    yaw=%.2f pitch=%.2f\n"
            "  Projection:\n"
            "    type: Orthographic\n"
            "    left: %.3f right: %.3f bottom: %.3f top: %.3f\n"
            "    near: %.3f far: %.3f\n",
            bounds.minX, bounds.maxX,
            bounds.minY, bounds.maxY,
            bounds.minZ, bounds.maxZ,
            pos.x, pos.y, pos.z,
            tgt.x, tgt.y, tgt.z,
            fwd.x, fwd.y, fwd.z,
            right.x, right.y, right.z,
            up.x, up.y, up.z,
            cam.GetWorldUp().x, cam.GetWorldUp().y, cam.GetWorldUp().z,
            cam.GetYaw(), cam.GetPitch(),
            cam.GetOrthoLeft(), cam.GetOrthoRight(),
            cam.GetOrthoBottom(), cam.GetOrthoTop(),
            cam.GetNearClip(), cam.GetFarClip());
        fflush(stderr);
        // -------------------------------------------------------------------

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
    // The MicroStation-style PTC shading modes (17-20) light the cloud the
    // same way, so they need real normals too.
    const uint32_t m = static_cast<uint32_t>(mode);
    bool needsNormals = (vmode == renderer::VisualizationMode::NormalShading ||
                          vmode == renderer::VisualizationMode::SurfaceShading ||
                          (m >= 17u && m <= 24u));
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

    // TEMPORARY DEBUG (PTC color-loss investigation): print the classification
    // IDs that will reach the shader for this cloud, plus normals state, once
    // per PTC mode selection. Satisfies "print first 20 unique classification
    // IDs reaching the shader" from the debug protocol (CPU-side view of the
    // exact attribute buffer the GPU reads).
    const uint32_t dbg = static_cast<uint32_t>(mode);
    if (dbg >= 17u && dbg <= 24u && m_cloud && m_cloud->Root()) {
        auto& channels = m_cloud->Root()->channels();
        auto* clsCh = channels.GetChannel(pointcloud::ChannelId::Classification);
        auto* nrmCh = channels.GetChannel(pointcloud::ChannelId::Normals);
        fprintf(stderr, "[PTC-DEBUG] mode=%u classificationChannel=%s normalsChannel=%s",
                dbg,
                (clsCh && clsCh->Data()) ? "yes" : "MISSING",
                (nrmCh && nrmCh->Data()) ? "yes" : "no");
        if (nrmCh && nrmCh->Data() && nrmCh->Count() >= 3) {
            const float* fn = reinterpret_cast<const float*>(nrmCh->Data());
            fprintf(stderr, " normal[0]=(%.3f,%.3f,%.3f)", fn[0], fn[1], fn[2]);
        }
        if (clsCh && clsCh->Data()) {
            std::set<int> uniq;  // first 20 unique IDs in buffer order
            const uint8_t* d = clsCh->Data();
            const size_t n = clsCh->Count();
            for (size_t i = 0; i < n && uniq.size() < 20; ++i) uniq.insert(d[i]);
            fprintf(stderr, " uniqueClassIDs(first20)=[");
            for (int v : uniq) fprintf(stderr, " %d", v);
            fprintf(stderr, " ] count=%zu", n);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
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

    // Progressive elevation: swap in higher-res mesh when ready
    if (elevationGenComplete_.exchange(false, std::memory_order_acquire)) {
        auto* sr = GetSurfaceRenderer();
        if (sr && surfaceVisible_ && m_cloud) {
            auto* entry = elevationCache_.Find(
                m_cloud->Id(), elevationReadyResolution_,
                surface::ElevationSourceMode::AllPoints);
            if (entry && entry->isValid) {
                sr->ClearAllMeshes();
                entry->mesh.SetName("Elevation_LOD_" +
                           std::to_string(elevationReadyResolution_.load()));
                sr->AddSurfaceMesh(entry->mesh);

                auto& cfg = m_renderer->GetContext().GetConfig();
                cfg.elevationMin = entry->grid.GetMinElevation();
                cfg.elevationMax = entry->grid.GetMaxElevation();
            }
        }
    }

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
    if (event->key() == Qt::Key_T && !event->isAutoRepeat()) {
        SetTopView();
        return;
    }
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

void ViewportWindow::SetTopView() {
    if (!m_rendererInitialized) return;
    auto& cam = m_renderer->GetContext().GetCamera();
    if (m_cloud && m_cloud->Root()) {
        cam.SetTopView(m_cloud->Root()->bounds());
    }
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
    auto* sr = GetSurfaceRenderer();
    if (!sr) return;

    // Auto-switch to ShadedSurface mode when user picks a shading type.
    // Without this, clicking "Elevation Heatmap" only sets ShadingType but
    // SurfaceMode remains Points (0), so the surface pass is skipped.
    if (sr->GetMode() == surface::SurfaceMode::Points) {
        sr->SetMode(surface::SurfaceMode::ShadedSurface);
    }

    // Auto-generate elevation grid if none exists.
    if (sr->GetMeshCount() == 0 && m_cloud && m_cloud->Root()) {
        GenerateSurfaceForCloud();
    }

    sr->SetShading(static_cast<surface::ShadingType>(shading));
}

void ViewportWindow::GenerateSurfaceForCloud() {
    if (!m_rendererInitialized || !m_cloud || !m_cloud->Root()) return;

    auto* sr = GetSurfaceRenderer();
    if (!sr) return;

    uint32_t cloudID = m_cloud->Id();

    // Initialize cache (idempotent)
    elevationCache_.Initialize();

    surface::ElevationGridParams params;
    params.resolution = m_terrainGridRes;
    // GroundOnly for DTM/hillshade - prevents vegetation/building spikes
    params.sourceMode = surface::ElevationSourceMode::GroundOnly;

    // Stage 1: Instant low-res display (256x256, ~1ms)
    auto* lod0 = elevationCache_.GetOrCreate(cloudID, *m_cloud, params);
    if (lod0 && lod0->isValid) {
        sr->ClearAllMeshes();
        lod0->mesh.SetName("Elevation_LOD0_256");
        sr->AddSurfaceMesh(lod0->mesh);
        surfaceVisible_ = true;
        sr->SetVisible(true);

        // Push elevation range to renderer config
        auto& cfg = m_renderer->GetContext().GetConfig();
        cfg.elevationMin = lod0->grid.GetMinElevation();
        cfg.elevationMax = lod0->grid.GetMaxElevation();
    }

    // Stage 2: Progressive background generation of higher resolutions
    if (pendingElevationGen_.valid()) {
        pendingElevationGen_.wait();
    }
    elevationGenComplete_ = false;
    pendingElevationGen_ = std::async(
        std::launch::async, [this, cloudID]() {
            auto* sr = GetSurfaceRenderer();
            if (!sr || !m_cloud) return;

            fprintf(stderr, "[RESOLUTION COMPARISON]\n");
            for (uint32_t res : {256u, 512u, 1024u}) {
                surface::ElevationGridParams p;
                p.resolution = res;
                p.sourceMode = surface::ElevationSourceMode::GroundOnly;

                auto t0 = std::chrono::steady_clock::now();
                auto* entry = elevationCache_.GetOrCreate(
                    cloudID, *m_cloud, p);
                auto t1 = std::chrono::steady_clock::now();
                double genMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

                if (entry && entry->isValid) {
                    const auto& stats = entry->grid.GetStats();
                    fprintf(stderr,
                        "  %ux%u: verts=%zu tris=%zu elev=[%.1f,%.1f] "
                        "Zrange=%.1f bin=%.1fms interp=%.1fms mesh=%.1fms total=%.1fms\n",
                        res, res,
                        entry->mesh.VertexCount(), entry->mesh.TriangleCount(),
                        stats.minElevation, stats.maxElevation,
                        stats.maxElevation - stats.minElevation,
                        stats.binningTimeMs, stats.interpolateTimeMs,
                        stats.meshGenTimeMs, genMs);
                    fflush(stderr);

                    elevationReadyResolution_.store(res,
                        std::memory_order_relaxed);
                    elevationGenComplete_.store(true,
                        std::memory_order_release);
                }
            }
            fprintf(stderr, "[RESOLUTION COMPARISON] done\n");
            fflush(stderr);
        });
}

void ViewportWindow::SetSurfaceQuality(int quality) {
    // Low / Medium / High: controls terrain grid resolution and normal quality.
    // Higher resolution = more terrain detail preserved.
    switch (quality) {
        case 0: // Low
            m_surfaceGenParams.normalNeighborCount = 6;
            m_surfaceGenParams.maxEdgeLength = 0.0;   // automatic
            m_surfaceGenParams.adaptiveTriangulation = true;
            m_terrainGridRes = 256;
            break;
        case 2: // High
            m_surfaceGenParams.normalNeighborCount = 16;
            m_surfaceGenParams.maxEdgeLength = 0.0;   // automatic
            m_surfaceGenParams.adaptiveTriangulation = true;
            m_terrainGridRes = 1024;
            break;
        default: // Medium
            m_surfaceGenParams.normalNeighborCount = 10;
            m_surfaceGenParams.maxEdgeLength = 0.0;
            m_surfaceGenParams.adaptiveTriangulation = true;
            m_terrainGridRes = 512;
            break;
    }

    // Rebuild in place if a surface is already displayed.
    if (surfaceVisible_ && m_cloud && m_cloud->Root()) {
        auto* sr = GetSurfaceRenderer();
        if (sr) {
            sr->ClearAllMeshes();
            elevationCache_.Clear();
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
    // Derive the canonical azimuth/elevation form from the raw direction so
    // the point pipeline (which now reads the angles) and the surface
    // pipeline stay in agreement no matter which dialog was used.
    float len = std::sqrt(x * x + y * y + z * z);
    if (len < 1e-5f) return;
    x /= len; y /= len; z /= len;
    constexpr float kPi = 3.14159265f;
    float elev = std::asin(std::clamp(z, -1.0f, 1.0f));
    float azim = std::atan2(y, x);
    if (azim < 0.0f) azim += 2.0f * kPi;
    cfg.lightAzimuthDeg = azim * 180.0f / kPi;
    cfg.lightElevationDeg = std::clamp(elev * 180.0f / kPi, 0.0f, 90.0f);
    // Keep the legacy XYZ mirror in sync for any existing consumers.
    cfg.surfaceLightDirX = x;
    cfg.surfaceLightDirY = y;
    cfg.surfaceLightDirZ = z;
}

void ViewportWindow::SetShadingParams(float ambient, float diffuse,
                                      float specular, float shininess) {
    if (!m_rendererInitialized || !m_renderer) return;
    auto& cfg = m_renderer->GetContext().GetConfig();
    cfg.surfaceAmbient = ambient;
    cfg.surfaceDiffuse = diffuse;
    cfg.surfaceSpecular = specular;
    cfg.surfaceShininess = shininess;
}

void ViewportWindow::SetEDLStrength(float strength) {
    if (!m_rendererInitialized || !m_renderer) return;
    m_renderer->GetContext().GetConfig().edlStrength = strength;
}

void ViewportWindow::SetSunAngles(float azimuthDeg, float elevationDeg) {
    if (!m_rendererInitialized || !m_renderer) return;
    auto& cfg = m_renderer->GetContext().GetConfig();
    cfg.lightAzimuthDeg = std::clamp(azimuthDeg, 0.0f, 360.0f);
    cfg.lightElevationDeg = std::clamp(elevationDeg, 0.0f, 90.0f);
}

void ViewportWindow::SetSurfaceShadingIfPresent(int shading) {
    auto* sr = GetSurfaceRenderer();
    if (!sr || sr->GetMeshCount() == 0) return;
    sr->SetShading(static_cast<surface::ShadingType>(shading));
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
