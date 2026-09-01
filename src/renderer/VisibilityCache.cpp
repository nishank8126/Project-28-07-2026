#include "workstation/renderer/VisibilityCache.h"

namespace workstation {
namespace renderer {

void VisibilityCache::Initialize(uint32_t maxEntries) {
    maxEntries_ = maxEntries;
}

VisibilityEntry* VisibilityCache::GetOrCreate(uint64_t nodeKey) {
    auto it = cache_.find(nodeKey);
    if (it != cache_.end()) return &it->second;

    if (cache_.size() >= maxEntries_) return nullptr;

    VisibilityEntry entry{};
    entry.nodeKey = nodeKey;
    cache_[nodeKey] = entry;
    return &cache_[nodeKey];
}

const VisibilityEntry* VisibilityCache::Get(uint64_t nodeKey) const {
    auto it = cache_.find(nodeKey);
    return it != cache_.end() ? &it->second : nullptr;
}

void VisibilityCache::UpdateVisibility(uint64_t nodeKey, bool visible,
                                         float sse, float distance, uint32_t frame) {
    auto* entry = GetOrCreate(nodeKey);
    if (!entry) return;

    entry->wasVisibleLastFrame = entry->isVisible;
    entry->isVisible = visible;
    entry->screenSpaceError = sse;
    entry->distanceToCamera = distance;
    entry->lastTestedFrame = frame;

    if (visible) {
        entry->consecutiveVisible++;
        entry->consecutiveInvisible = 0;
    } else {
        entry->consecutiveInvisible++;
        entry->consecutiveVisible = 0;
    }
}

bool VisibilityCache::WasVisibleLastFrame(uint64_t nodeKey) const {
    auto* entry = Get(nodeKey);
    return entry ? entry->wasVisibleLastFrame : false;
}

bool VisibilityCache::IsStable(uint64_t nodeKey, uint32_t stabilityThreshold) const {
    auto* entry = Get(nodeKey);
    if (!entry) return false;
    return entry->consecutiveVisible >= stabilityThreshold ||
           entry->consecutiveInvisible >= stabilityThreshold;
}

void VisibilityCache::InvalidateAll() {
    for (auto& [key, entry] : cache_) {
        entry.isVisible = false;
    }
}

void VisibilityCache::Clear() {
    cache_.clear();
}

} // namespace renderer
} // namespace workstation
