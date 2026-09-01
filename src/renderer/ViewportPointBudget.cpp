#include "workstation/renderer/ViewportPointBudget.h"
#include "workstation/renderer/LODManager.h"

namespace workstation {
namespace renderer {

void ViewportPointBudget::BeginFrame() {
    usedThisFrame_ = 0;
    nodesThisFrame_ = 0;
}

bool ViewportPointBudget::CanAllocatePoints(uint64_t count) const {
    if (!config_.enforceBudget) return true;
    return usedThisFrame_ + count <= config_.gpuBudget;
}

void ViewportPointBudget::AllocatePoints(uint64_t count) {
    usedThisFrame_ += count;
    nodesThisFrame_++;
}

uint64_t ViewportPointBudget::GetRemainingBudget() const {
    if (usedThisFrame_ >= config_.gpuBudget) return 0;
    return config_.gpuBudget - usedThisFrame_;
}

bool ViewportPointBudget::ShouldRejectNode(const LODNodeCandidate& node,
                                             const VisibilityCache& visCache) const {
    if (!config_.enforceBudget) return false;

    if (usedThisFrame_ + node.pointCount > config_.gpuBudget) {
        return true;
    }

    if (nodesThisFrame_ >= config_.maxNodesPerFrame) {
        return true;
    }

    auto* entry = visCache.Get(node.nodeKey);
    if (entry && entry->wasVisibleLastFrame && node.screenSpaceError < 2.0) {
        return false;
    }

    return false;
}

ViewportPointBudget::Stats ViewportPointBudget::GetStats() const {
    Stats stats{};
    stats.totalBudget = config_.gpuBudget;
    stats.usedBudget = usedThisFrame_;
    stats.remainingBudget = GetRemainingBudget();
    return stats;
}

} // namespace renderer
} // namespace workstation
