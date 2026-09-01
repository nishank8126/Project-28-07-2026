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

#include <memory>
#include <vector>
#include <cstdint>

namespace workstation {
namespace renderer {

struct RenderConfig {
    VisualizationMode visualizationMode = VisualizationMode::RGB;
    float pointSize = 2.0f;
    float intensityMin = 0.0f;
    float intensityMax = 1.0f;
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
    uint32_t maxPointsPerFrame = 50'000'000;
    bool lodEnabled = true;
    float pointBudgetMillions = 25.0f;
    int32_t forceLODLevel = -1;
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
