#pragma once
#include "workstation/spatial/SpatialTree.h"
#include "workstation/spatial/SpatialNode.h"
#include "workstation/renderer/Camera.h"
#include "workstation/renderer/VisibilityCache.h"

#include <vector>
#include <cstdint>

namespace workstation {
namespace renderer {

struct VisibilityResult {
    std::vector<uint64_t> visibleNodeKeys;
    uint64_t totalVisiblePoints = 0;
    uint32_t nodesTested = 0;
    uint32_t nodesPassed = 0;
    uint32_t nodesCached = 0;
};

class VisibilitySystem {
public:
    void Initialize(spatial::SpatialTree* tree);

    VisibilityResult ComputeVisibility(const Camera& camera,
                                        uint32_t viewportWidth,
                                        uint32_t viewportHeight,
                                        VisibilityCache& visCache,
                                        uint32_t frameNumber);

    std::vector<uint64_t> CollectVisibleNodes(const Camera& camera,
                                               uint32_t viewportWidth,
                                               uint32_t viewportHeight,
                                               VisibilityCache& visCache,
                                               uint32_t frameNumber);

    void SetDebugEnabled(bool enabled) { debugEnabled_ = enabled; }
    bool IsDebugEnabled() const { return debugEnabled_; }

    const std::vector<spatial::SpatialNode*>& GetCulledNodes() const {
        return culledNodes_;
    }

private:
    spatial::SpatialTree* tree_ = nullptr;
    bool debugEnabled_ = false;
    std::vector<spatial::SpatialNode*> culledNodes_;

    bool TestNodeVisibility(const spatial::SpatialNode& node,
                            const FrustumPlanes& frustum) const;
};

} // namespace renderer
} // namespace workstation
