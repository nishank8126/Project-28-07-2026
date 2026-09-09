#pragma once
#include "workstation/renderer/Camera.h"
#include "workstation/renderer/LODManager.h"
#include "workstation/renderer/VisibilitySystem.h"
#include "workstation/renderer/VisibilityCache.h"
#include "workstation/renderer/ViewportPointBudget.h"
#include "workstation/renderer/RenderQueue.h"
#include "workstation/renderer/RenderCommand.h"
#include "workstation/renderer/PipelineCacheManager.h"
#include "workstation/renderer/VulkanStateCache.h"
#include "workstation/renderer/ConstantBufferManager.h"
#include "workstation/renderer/VisualizationManager.h"
#include "workstation/gpu/PointStreamingManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"

#include <cmath>
#include <memory>
#include <vector>
#include <cstdint>

namespace workstation {
namespace renderer {

// Convert sun azimuth/elevation angles (degrees) into a unit direction vector
// pointing TOWARD the light source, in world space with Z up (the LiDAR data
// convention used throughout this engine). Both the point pipeline and the
// surface pipeline read this same direction, so moving the sun moves it for
// every shading mode at once. Pure push-constant math -- no mesh/geometry
// regeneration is involved.
//   azimuth:   0-360, compass angle in the XY plane (0 = +X, 90 = +Y)
//   elevation: 0-90,  angle above the horizon (90 = straight overhead)
inline void ComputeLightDirection(float azimuthDeg, float elevationDeg,
                                  float out[3]) {
    constexpr float kPi = 3.14159265358979f;
    const float az = azimuthDeg * kPi / 180.0f;
    const float el = std::clamp(elevationDeg, 0.0f, 90.0f) * kPi / 180.0f;
    const float cosEl = std::cos(el);
    out[0] = cosEl * std::cos(az);
    out[1] = cosEl * std::sin(az);
    out[2] = std::sin(el);
}

struct RenderConfig {
    VisualizationMode visualizationMode = VisualizationMode::RGB;
    float pointSize = 2.0f;
    float intensityMin = 0.0f;
    float intensityMax = 1.0f;
    float elevationMin = 0.0f;
    float elevationMax = 1000.0f;
    bool showBoundingBoxes = false;
    bool showLODColors = false;
    bool showVisibleNodes = false;
    bool showFrustum = false;
    bool frustumCulling = true;
    bool backfaceCulling = false;
    bool sortByPipeline = true;
    bool useIndirectDrawing = false;
    bool useComputeCulling = false;
    bool forceDrawAll = false;
    uint32_t maxPointsPerFrame = 5'000'000;
    bool lodEnabled = true;
    float pointBudgetMillions = 5.0f;
    int32_t forceLODLevel = -1;

    // Depth / Surface shading parameters
    float depthMin = 0.0f;
    float depthMax = 1000.0f;
    float surfaceAmbient = 0.1f;
    float surfaceDiffuse = 1.0f;
    float surfaceSpecular = 0.05f;
    float surfaceShininess = 64.0f;
    float edlStrength = 1.5f;
    // Surface pipeline light direction (world space). Matches the
    // SurfaceRenderParams defaults so both pipelines agree out of the box.
    float surfaceLightDirX = 0.35f;
    float surfaceLightDirY = 0.35f;
    float surfaceLightDirZ = 0.87f;
    // Canonical sun control shared by ALL shading pipelines (point + surface).
    // The default 45/45 reproduces strong directional terrain relief.
    float lightAzimuthDeg = 45.0f;    // 0-360, compass angle of the light
    float lightElevationDeg = 45.0f;  // 0-90, angle above the horizon

    // Vertical relief exaggeration for terrain visualization
    // 1.0 = real scale, >1.0 amplifies Z for subtle terrain visibility
    float verticalExaggeration = 1.0f;

    // Performance settings (PHASE 13)
    enum class PointSizeMode { Auto = 0, Fixed1px = 1, Fixed2px = 2, Fixed3px = 3 };
    PointSizeMode pointSizeMode = PointSizeMode::Auto;
    float maxPointSizePx = 4.0f;   // Maximum point size in pixels
    float minPointSizePx = 1.0f;   // Minimum point size in pixels

    enum class LODQuality { High = 0, Balanced = 1, Performance = 2 };
    LODQuality lodQuality = LODQuality::Balanced;

    enum class StreamingQuality { Quality = 0, Balanced = 1, Fast = 2 };
    StreamingQuality streamingQuality = StreamingQuality::Balanced;

    // LOD hysteresis thresholds (PHASE 4)
    float lodEnterThreshold = 20.0f;   // SSE below this -> increase detail
    float lodExitThreshold = 30.0f;    // SSE above this -> decrease detail
};

struct RendererStats {
    uint64_t totalPoints = 0;
    uint64_t visiblePoints = 0;
    uint32_t visibleNodes = 0;
    uint32_t gpuBuffers = 0;
    uint32_t drawCalls = 0;
    uint32_t pipelineChanges = 0;
    uint32_t descriptorChanges = 0;
    uint32_t bufferChanges = 0;
    uint32_t renderPassChanges = 0;
    uint32_t totalVulkanCommands = 0;
    double frameTimeMs = 0.0;
    double fps = 0.0;
    uint32_t cachedPipelines = 0;
    uint64_t gpuMemoryBytes = 0;
    uint32_t indirectDrawCount = 0;
    double gpuComputeTimeMs = 0.0;
    double gpuRenderTimeMs = 0.0;
    double gpuSurfaceTimeMs = 0.0;
    uint32_t culledNodes = 0;
    uint32_t indirectDraws = 0;
    uint32_t gpuCullingDispatches = 0;
    uint32_t lodLevelDistribution[5] = {0,0,0,0,0};
    // Enhanced profiling (PHASE 1)
    uint64_t gpuMemoryTotalBytes = 0;
    uint32_t uploadsThisFrame = 0;
    double mbUploadedThisFrame = 0.0;
    uint32_t tileReuseHits = 0;
    uint32_t computeCommandsGenerated = 0;
    uint32_t cpuDrawCalls = 0;
    double cacheHitRate = 0.0;
};

struct LODDebugStats {
    uint32_t visibleNodes = 0;
    uint32_t selectedNodes = 0;
    uint32_t rejectedNodes = 0;
    uint64_t visiblePoints = 0;
    uint64_t renderedPoints = 0;
    uint64_t pointBudget = 0;
    double averageSSE = 0.0;
    uint32_t currentLOD = 0;
    double lodTimeMs = 0.0;
};

using StreamingDebugStats = gpu::StreamingDebugStats;

class RenderContext {
public:
    Camera& GetCamera() { return camera_; }
    const Camera& GetCamera() const { return camera_; }
    LODManager& GetLODManager() { return lodManager_; }
    const LODManager& GetLODManager() const { return lodManager_; }
    VisibilitySystem& GetVisibilitySystem() { return visibilitySystem_; }
    VisibilityCache& GetVisibilityCache() { return visibilityCache_; }
    ViewportPointBudget& GetPointBudget() { return pointBudget_; }
    RenderQueue& GetRenderQueue() { return renderQueue_; }
    RenderConfig& GetConfig() { return config_; }
    const RenderConfig& GetConfig() const { return config_; }
    RendererStats& GetStats() { return stats_; }
    const RendererStats& GetStats() const { return stats_; }
    LODDebugStats& GetLODDebugStats() { return lodDebugStats_; }
    const LODDebugStats& GetLODDebugStats() const { return lodDebugStats_; }
    StreamingDebugStats& GetStreamingDebugStats() { return streamingDebugStats_; }
    const StreamingDebugStats& GetStreamingDebugStats() const { return streamingDebugStats_; }
    VulkanStateCache& GetStateCache() { return stateCache_; }
    ConstantBufferManager& GetConstantBuffers() { return constantBuffers_; }

    void SetViewportSize(uint32_t width, uint32_t height) {
        viewportWidth_ = width;
        viewportHeight_ = height;
        camera_.SetAspectRatio(static_cast<double>(width) / height);
    }
    uint32_t GetViewportWidth() const { return viewportWidth_; }
    uint32_t GetViewportHeight() const { return viewportHeight_; }

    void UpdateFrameStats(double frameTimeMs);

private:
    Camera camera_;
    LODManager lodManager_;
    VisibilitySystem visibilitySystem_;
    VisibilityCache visibilityCache_;
    ViewportPointBudget pointBudget_;
    RenderQueue renderQueue_;
    RenderConfig config_;
    RendererStats stats_ = {};
    LODDebugStats lodDebugStats_ = {};
    StreamingDebugStats streamingDebugStats_ = {};
    VulkanStateCache stateCache_;
    ConstantBufferManager constantBuffers_;
    uint32_t viewportWidth_ = 1920;
    uint32_t viewportHeight_ = 1080;
};

} // namespace renderer
} // namespace workstation
