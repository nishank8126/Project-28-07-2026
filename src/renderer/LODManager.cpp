#include "workstation/renderer/LODManager.h"
#include "workstation/renderer/ViewportPointBudget.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include <cmath>
#include <algorithm>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace workstation {
namespace renderer {

// ---- LODCache implementation ----

void LODCache::Initialize(uint32_t maxEntries) {
    maxEntries_ = maxEntries;
}

LODCacheEntry* LODCache::GetOrCreate(uint64_t nodeKey) {
    auto it = cache_.find(nodeKey);
    if (it != cache_.end()) return &it->second;

    if (cache_.size() >= maxEntries_) return nullptr;

    LODCacheEntry entry{};
    entry.nodeKey = nodeKey;
    cache_[nodeKey] = entry;
    return &cache_[nodeKey];
}

const LODCacheEntry* LODCache::Get(uint64_t nodeKey) const {
    auto it = cache_.find(nodeKey);
    return it != cache_.end() ? &it->second : nullptr;
}

void LODCache::UpdateEntry(uint64_t nodeKey, uint32_t lod, double sse,
                            uint64_t pointCount, bool selected, uint32_t frame) {
    auto* entry = GetOrCreate(nodeKey);
    if (!entry) return;

    entry->currentLOD = lod;
    entry->lastSelectedFrame = frame;
    entry->screenSpaceError = sse;
    entry->pointCount = pointCount;
    entry->isSelected = selected;
}

bool LODCache::WasSelectedLastFrame(uint64_t nodeKey) const {
    auto* entry = Get(nodeKey);
    return entry ? entry->isSelected : false;
}

bool LODCache::NeedsUpdate(uint64_t nodeKey, double newSSE, uint32_t frame,
                            double sseThreshold) const {
    auto* entry = Get(nodeKey);
    if (!entry) return true;
    if (entry->lastSelectedFrame == 0) return true;
    if (entry->lastSelectedFrame >= frame) return false;

    double sseDelta = std::abs(newSSE - entry->screenSpaceError);
    return sseDelta > sseThreshold;
}

void LODCache::Clear() {
    cache_.clear();
}

// ---- LODManager implementation ----

void LODManager::EvaluateNodes(const Camera& camera, uint32_t viewportWidth,
                                 uint32_t viewportHeight,
                                 gpu::GeometryCache& geometryCache,
                                 VisibilityCache& visCache) {
    selectedNodes_.clear();
    rejectedNodes_.clear();
    selectedPointCount_ = 0;
    activeLODLevel_ = 0;

    (void)geometryCache;
    (void)visCache;
}

LODSelectionResult LODManager::SelectNodes(
    const std::vector<uint64_t>& visibleNodes,
    const Camera& camera,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    const ViewportPointBudget& budget,
    const pointcloud::PointCloud* cloud,
    VisibilityCache& visCache,
    uint32_t frameNumber) {

    LODSelectionResult result{};
    selectedNodes_.clear();
    rejectedNodes_.clear();
    selectedPointCount_ = 0;
    activeLODLevel_ = 0;

    if (!config_.lodEnabled || visibleNodes.empty() || !cloud) {
        result.selectedNodes = visibleNodes;
        for (uint64_t key : visibleNodes) {
            auto* root = cloud ? cloud->Root() : nullptr;
            if (root) {
                result.selectedPointCount += root->PointCount();
                result.selectedNodeCount++;
            }
        }
        return result;
    }

    if (config_.forceLODLevel >= 0) {
        activeLODLevel_ = static_cast<uint32_t>(config_.forceLODLevel);
    }

    std::vector<LODNodeCandidate> candidates;
    candidates.reserve(visibleNodes.size());

    for (uint64_t key : visibleNodes) {
        auto* root = cloud->Root();
        if (!root) continue;

        LODNodeCandidate candidate{};
        candidate.nodeKey = key;
        candidate.pointCount = root->PointCount();
        candidate.bounds = root->bounds();
        candidate.isVisible = true;

        const auto* cached = visCache.Get(key);
        candidate.wasVisibleLastFrame = cached ? cached->wasVisibleLastFrame : false;

        candidate.screenSpaceError = CalculateScreenSpaceError(
            candidate, camera, viewportWidth, viewportHeight);
        candidate.distanceToCamera = CalculateDistance(candidate, camera);
        candidate.screenSizePixels = CalculateScreenSize(
            candidate, camera, viewportWidth, viewportHeight);

        if (config_.forceLODLevel >= 0) {
            candidate.level = static_cast<uint32_t>(config_.forceLODLevel);
        } else {
            double sse = candidate.screenSpaceError;
            if (sse >= config_.maxScreenSpaceError) {
                candidate.level = 0;
            } else if (sse <= config_.minScreenSpaceError) {
                candidate.level = config_.maxLODLevels - 1;
            } else {
                double normalized = (sse - config_.minScreenSpaceError) /
                                    (config_.maxScreenSpaceError - config_.minScreenSpaceError);
                candidate.level = static_cast<uint32_t>(
                    (1.0 - normalized) * (config_.maxLODLevels - 1));
            }
        }

        candidates.push_back(candidate);
    }

    std::sort(candidates.begin(), candidates.end(),
              [this, viewportWidth, viewportHeight](const LODNodeCandidate& a,
                                                     const LODNodeCandidate& b) {
                  return CalculateLODScore(a, viewportWidth, viewportHeight) >
                         CalculateLODScore(b, viewportWidth, viewportHeight);
              });

    uint64_t budgetUsed = 0;
    uint64_t budgetTotal = budget.GetTotalBudget();
    double totalSSE = 0.0;
    uint32_t maxLODSeen = 0;

    for (auto& candidate : candidates) {
        bool selected = false;

        if (budget.CanAllocatePoints(candidate.pointCount)) {
            selected = true;
            budgetUsed += candidate.pointCount;
            selectedPointCount_ += candidate.pointCount;
            selectedNodes_.push_back(candidate);

            if (candidate.level > maxLODSeen) {
                maxLODSeen = candidate.level;
            }
            activeLODLevel_ = maxLODSeen;
            totalSSE += candidate.screenSpaceError;

            lodCache_.UpdateEntry(candidate.nodeKey, candidate.level,
                                   candidate.screenSpaceError,
                                   candidate.pointCount, true, frameNumber);
        } else {
            rejectedNodes_.push_back(candidate);
            result.rejectedPointCount += candidate.pointCount;
            result.rejectedNodeCount++;

            lodCache_.UpdateEntry(candidate.nodeKey, candidate.level,
                                   candidate.screenSpaceError,
                                   candidate.pointCount, false, frameNumber);
        }
    }

    result.selectedNodes.clear();
    result.selectedNodes.reserve(selectedNodes_.size());
    for (const auto& node : selectedNodes_) {
        result.selectedNodes.push_back(node.nodeKey);
    }

    result.selectedPointCount = selectedPointCount_;
    result.selectedNodeCount = static_cast<uint32_t>(selectedNodes_.size());
    result.maxLOD = maxLODSeen;
    result.averageSSE = result.selectedNodeCount > 0 ?
        totalSSE / result.selectedNodeCount : 0.0;

    return result;
}

double LODManager::CalculateScreenSpaceError(const LODNodeCandidate& node,
                                               const Camera& camera,
                                               uint32_t viewportWidth,
                                               uint32_t viewportHeight) const {
    double dx = node.bounds.maxX - node.bounds.minX;
    double dy = node.bounds.maxY - node.bounds.minY;
    double dz = node.bounds.maxZ - node.bounds.minZ;
    double nodeRadius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5;

    double cx = (node.bounds.minX + node.bounds.maxX) * 0.5;
    double cy = (node.bounds.minY + node.bounds.maxY) * 0.5;
    double cz = (node.bounds.minZ + node.bounds.maxZ) * 0.5;

    math::Point3d camPos = camera.GetPosition();
    double dist = std::sqrt((cx - camPos.x) * (cx - camPos.x) +
                            (cy - camPos.y) * (cy - camPos.y) +
                            (cz - camPos.z) * (cz - camPos.z));
    if (dist < 1e-10) dist = 1e-10;

    double fovRad = camera.GetFOV() * M_PI / 180.0;
    return (nodeRadius / dist) * viewportHeight / std::tan(fovRad * 0.5);
}

float LODManager::CalculateScreenSize(const LODNodeCandidate& node,
                                        const Camera& camera,
                                        uint32_t viewportWidth,
                                        uint32_t viewportHeight) const {
    return static_cast<float>(CalculateScreenSpaceError(node, camera,
                                                         viewportWidth, viewportHeight));
}

double LODManager::CalculateDistance(const LODNodeCandidate& node,
                                      const Camera& camera) const {
    double cx = (node.bounds.minX + node.bounds.maxX) * 0.5;
    double cy = (node.bounds.minY + node.bounds.maxY) * 0.5;
    double cz = (node.bounds.minZ + node.bounds.maxZ) * 0.5;

    math::Point3d camPos = camera.GetPosition();
    return std::sqrt((cx - camPos.x) * (cx - camPos.x) +
                    (cy - camPos.y) * (cy - camPos.y) +
                    (cz - camPos.z) * (cz - camPos.z));
}

double LODManager::CalculateLODScore(const LODNodeCandidate& node,
                                       uint32_t viewportWidth,
                                       uint32_t viewportHeight) const {
    double sse = node.screenSpaceError;
    double distance = node.distanceToCamera;
    double screenSize = node.screenSizePixels;

    double distanceFactor = 1.0 / (1.0 + distance * 0.001);
    double sizeFactor = screenSize / static_cast<double>(viewportHeight);

    return sse * 0.5 + distanceFactor * 1000.0 + sizeFactor * 500.0;
}

uint32_t LODManager::GetBudgetRemaining() const {
    if (selectedPointCount_ >= config_.visiblePointBudget) return 0;
    return static_cast<uint32_t>(config_.visiblePointBudget - selectedPointCount_);
}

void LODManager::Clear() {
    selectedNodes_.clear();
    rejectedNodes_.clear();
    selectedPointCount_ = 0;
    activeLODLevel_ = 0;
    lodCache_.Clear();
}

} // namespace renderer
} // namespace workstation
