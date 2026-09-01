#include "workstation/renderer/VisibilitySystem.h"

namespace workstation {
namespace renderer {

void VisibilitySystem::Initialize(spatial::SpatialTree* tree) {
    tree_ = tree;
}

VisibilityResult VisibilitySystem::ComputeVisibility(const Camera& camera,
                                                       uint32_t viewportWidth,
                                                       uint32_t viewportHeight,
                                                       VisibilityCache& visCache,
                                                       uint32_t frameNumber) {
    VisibilityResult result;

    if (!tree_) return result;

    const auto& frustum = const_cast<Camera&>(camera).GetFrustumPlanes();

    culledNodes_.clear();

    tree_->Traverse([&](spatial::SpatialNode& node) {
        result.nodesTested++;

        const auto* cached = visCache.Get(node.key);
        if (cached && cached->lastTestedFrame == frameNumber) {
            if (cached->isVisible) {
                result.visibleNodeKeys.push_back(node.key);
                result.totalVisiblePoints += node.pointCount;
                result.nodesPassed++;
                result.nodesCached++;
            }
            return;
        }

        bool visible = TestNodeVisibility(node, frustum);

        float sse = 0.0f;
        float distance = 0.0f;
        visCache.UpdateVisibility(node.key, visible, sse, distance, frameNumber);

        if (visible) {
            result.visibleNodeKeys.push_back(node.key);
            result.totalVisiblePoints += node.pointCount;
            result.nodesPassed++;
        } else {
            culledNodes_.push_back(&node);
        }
    });

    return result;
}

std::vector<uint64_t> VisibilitySystem::CollectVisibleNodes(
    const Camera& camera,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    VisibilityCache& visCache,
    uint32_t frameNumber) {
    auto result = ComputeVisibility(camera, viewportWidth, viewportHeight, visCache, frameNumber);
    return std::move(result.visibleNodeKeys);
}

bool VisibilitySystem::TestNodeVisibility(const spatial::SpatialNode& node,
                                           const FrustumPlanes& frustum) const {
    return frustum.TestAABB(node.bounds);
}

} // namespace renderer
} // namespace workstation
