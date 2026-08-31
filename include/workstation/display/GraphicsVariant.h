#pragma once
#include "workstation/display/CacheKeys.h"
#include "workstation/display/CachedGraphics.h"
#include <vector>
#include <algorithm>

namespace workstation {

// One retained graphics representation bound to an unsized key plus a
// view-metric validity range (RE D/F). Multiple entries for the same
// element/state with overlapping ranges are permitted (RE H). The original
// node was ~0x38 bytes with an intrusive next pointer; we use a plain struct
// kept in a std::vector sorted ascending by minViewMetric instead.
struct GraphicsVariant {
    GraphicsUnsizedKey   key;
    double               minViewMetric = 0.0;
    double               maxViewMetric = 0.0;
    CachedGraphicsHandle graphics;
};

// Holds the specialized (non-simple) retained graphics variants for one
// element. Sorted ascending by minViewMetric via std::lower_bound (RE H).
// A custom slab/free-list allocator (RE I) may be introduced later without
// changing this public interface.
class ElementGraphicsVariantSet {
    std::vector<GraphicsVariant> m_entries;

public:
    // Insert preserving ascending minViewMetric order (insert before the
    // first existing entry whose minViewMetric is >= the new one).
    void Insert(GraphicsVariant v);

    // Return graphics if an entry matches all key fields and the metric is
    // inside [minViewMetric, maxViewMetric], else nullptr. If several
    // overlapping entries match, the tightest (largest minViewMetric) wins.
    CachedGraphicsHandle Find(const GraphicsUnsizedKey& key,
                              double metric,
                              uint64_t currentRevision) const;

    void Clear();
    size_t Size() const { return m_entries.size(); }
    const std::vector<GraphicsVariant>& Entries() const { return m_entries; }
};

} // namespace workstation
