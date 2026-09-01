#pragma once
#include "workstation/renderer/Camera.h"
#include "workstation/renderer/VisibilityCache.h"
#include "workstation/renderer/RenderCommand.h"
#include "workstation/renderer/ViewportPointBudget.h"
#include "workstation/spatial/SpatialTree.h"
#include "workstation/gpu/GeometryCache.h"

#include <vector>
#include <cstdint>
#include <unordered_map>

namespace workstation {
namespace renderer {

struct LODConfig {
    uint64_t totalPointBudget = 50'000'000;
    uint64_t visiblePointBudget = 30'000'000;
    double minScreenSpaceError = 1.0;
    double maxScreenSpaceError = 100.0;
    double errorReductionFactor = 2.0;
    uint32_t maxLODLevels = 16;
    float minNodeScreenSize = 2.0;
    bool lodEnabled = true;
    int32_t forceLODLevel = -1;
};

struct LODNodeCandidate {
    uint64_t nodeKey = 0;
    uint64_t pointCount = 0;
    spatial::BoundingBox bounds;
    double screenSpaceError = 0.0;
    double distanceToCamera = 0.0;
    float screenSizePixels = 0.0f;
    uint32_t level = 0;
    bool isVisible = false;
    bool wasVisibleLastFrame = false;

    bool operator<(const LODNodeCandidate& o) const {
        return screenSpaceError > o.screenSpaceError;
    }
};

struct LODSelectionResult {
    std::vector<uint64_t> selectedNodes;
    uint64_t selectedPointCount = 0;
    uint64_t rejectedPointCount = 0;
    uint32_t selectedNodeCount = 0;
    uint32_t rejectedNodeCount = 0;
    uint32_t maxLOD = 0;
    double averageSSE = 0.0;
};

struct LODCacheEntry {
    uint64_t nodeKey = 0;
    uint32_t currentLOD = 0;
    uint32_t lastSelectedFrame = 0;
    double screenSpaceError = 0.0;
    uint64_t pointCount = 0;
    bool isSelected = false;
};

class LODCache {
public:
    void Initialize(uint32_t maxEntries = 16384);

    LODCacheEntry* GetOrCreate(uint64_t nodeKey);
    const LODCacheEntry* Get(uint64_t nodeKey) const;

    void UpdateEntry(uint64_t nodeKey, uint32_t lod, double sse,
                      uint64_t pointCount, bool selected, uint32_t frame);

    bool WasSelectedLastFrame(uint64_t nodeKey) const;
    bool NeedsUpdate(uint64_t nodeKey, double newSSE, uint32_t frame,
                      double sseThreshold = 0.1) const;

    void Clear();

    uint32_t GetCachedCount() const { return static_cast<uint32_t>(cache_.size()); }

private:
    std::unordered_map<uint64_t, LODCacheEntry> cache_;
    uint32_t maxEntries_ = 16384;
};

class LODManager {
public:
    void SetConfig(const LODConfig& config) { config_ = config; }
    LODConfig& GetConfig() { return config_; }
    const LODConfig& GetConfig() const { return config_; }

    void EvaluateNodes(const Camera& camera, uint32_t viewportWidth,
                        uint32_t viewportHeight,
                        gpu::GeometryCache& geometryCache,
                        VisibilityCache& visCache);

    LODSelectionResult SelectNodes(
        const std::vector<uint64_t>& visibleNodes,
        const Camera& camera,
        uint32_t viewportWidth,
        uint32_t viewportHeight,
        ViewportPointBudget& budget,
        const pointcloud::PointCloud* cloud,
        VisibilityCache& visCache,
        uint32_t frameNumber);

    const std::vector<LODNodeCandidate>& GetSelectedNodes() const { return selectedNodes_; }
    const std::vector<LODNodeCandidate>& GetRejectedNodes() const { return rejectedNodes_; }
    uint64_t GetSelectedPointCount() const { return selectedPointCount_; }
    uint32_t GetActiveLODLevel() const { return activeLODLevel_; }
    uint32_t GetBudgetRemaining() const;
    LODCache& GetLODCache() { return lodCache_; }
    const LODCache& GetLODCache() const { return lodCache_; }

    void Clear();

private:
    LODConfig config_;
    std::vector<LODNodeCandidate> selectedNodes_;
    std::vector<LODNodeCandidate> rejectedNodes_;
    uint64_t selectedPointCount_ = 0;
    uint32_t activeLODLevel_ = 0;
    LODCache lodCache_;

    double CalculateScreenSpaceError(const LODNodeCandidate& node,
                                      const Camera& camera,
                                      uint32_t viewportWidth,
                                      uint32_t viewportHeight) const;
    float CalculateScreenSize(const LODNodeCandidate& node,
                               const Camera& camera,
                               uint32_t viewportWidth,
                               uint32_t viewportHeight) const;
    double CalculateDistance(const LODNodeCandidate& node,
                             const Camera& camera) const;
    double CalculateLODScore(const LODNodeCandidate& node,
                              uint32_t viewportWidth,
                              uint32_t viewportHeight) const;
};

} // namespace renderer
} // namespace workstation
