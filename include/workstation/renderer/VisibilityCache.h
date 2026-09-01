#pragma once
#include "workstation/spatial/SpatialNode.h"
#include "workstation/renderer/Camera.h"

#include <unordered_map>
#include <cstdint>

namespace workstation {
namespace renderer {

struct VisibilityEntry {
    uint64_t nodeKey = 0;
    bool isVisible = false;
    bool wasVisibleLastFrame = false;
    float screenSpaceError = 0.0f;
    float distanceToCamera = 0.0f;
    uint32_t lastTestedFrame = 0;
    uint32_t consecutiveVisible = 0;
    uint32_t consecutiveInvisible = 0;
};

class VisibilityCache {
public:
    void Initialize(uint32_t maxEntries = 16384);

    VisibilityEntry* GetOrCreate(uint64_t nodeKey);
    const VisibilityEntry* Get(uint64_t nodeKey) const;

    void UpdateVisibility(uint64_t nodeKey, bool visible,
                           float sse, float distance, uint32_t frame);

    bool WasVisibleLastFrame(uint64_t nodeKey) const;
    bool IsStable(uint64_t nodeKey, uint32_t stabilityThreshold = 3) const;

    void InvalidateAll();
    void Clear();

    uint32_t GetVisibleCount() const { return visibleCount_; }
    uint32_t GetCachedCount() const { return static_cast<uint32_t>(cache_.size()); }

private:
    std::unordered_map<uint64_t, VisibilityEntry> cache_;
    uint32_t maxEntries_ = 16384;
    uint32_t visibleCount_ = 0;
};

} // namespace renderer
} // namespace workstation
