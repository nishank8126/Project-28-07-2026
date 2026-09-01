#pragma once
#include "workstation/renderer/VisibilityCache.h"

#include <cstdint>

namespace workstation {
namespace renderer {

struct LODNodeCandidate;

struct PointBudgetConfig {
    uint64_t gpuBudget = 50'000'000;
    uint64_t cpuCacheBudget = 100'000'000;
    uint32_t maxNodesPerFrame = 1000;
    bool enforceBudget = true;
};

class ViewportPointBudget {
public:
    void SetConfig(const PointBudgetConfig& config) { config_ = config; }
    const PointBudgetConfig& GetConfig() const { return config_; }

    void BeginFrame();
    bool CanAllocatePoints(uint64_t count) const;
    void AllocatePoints(uint64_t count);
    uint64_t GetRemainingBudget() const;
    uint64_t GetUsedBudget() const { return usedThisFrame_; }
    uint64_t GetTotalBudget() const { return config_.gpuBudget; }

    bool ShouldRejectNode(const LODNodeCandidate& node,
                           const VisibilityCache& visCache) const;

    struct Stats {
        uint64_t totalBudget = 0;
        uint64_t usedBudget = 0;
        uint64_t remainingBudget = 0;
        uint32_t nodesAccepted = 0;
        uint32_t nodesRejected = 0;
    };
    Stats GetStats() const;

private:
    PointBudgetConfig config_;
    uint64_t usedThisFrame_ = 0;
    uint32_t nodesThisFrame_ = 0;
};

} // namespace renderer
} // namespace workstation
