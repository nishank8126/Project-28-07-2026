#include "workstation/display/GraphicsVariant.h"
#include <limits>
#include <algorithm>

namespace workstation {

void ElementGraphicsVariantSet::Insert(GraphicsVariant v) {
    auto it = std::lower_bound(
        m_entries.begin(), m_entries.end(), v.minViewMetric,
        [](const GraphicsVariant& e, double m) { return e.minViewMetric < m; });
    m_entries.insert(it, std::move(v));
}

CachedGraphicsHandle ElementGraphicsVariantSet::Find(const GraphicsUnsizedKey& key,
                                                     double metric,
                                                     uint64_t currentRevision) const {
    auto keysEqual = [](const GraphicsUnsizedKey& a, const GraphicsUnsizedKey& b) {
        if (a.styleKey.has_value() != b.styleKey.has_value()) return false;
        if (a.styleKey && *a.styleKey != *b.styleKey) return false;
        if (a.filterKey.has_value() != b.filterKey.has_value()) return false;
        if (a.filterKey && *a.filterKey != *b.filterKey) return false;
        return true;
    };

    CachedGraphicsHandle best;
    double bestMin = -std::numeric_limits<double>::infinity();

    for (const auto& e : m_entries) {
        if (e.key.variant != key.variant) continue;
        if (e.key.transformKey != key.transformKey) continue;
        if (!keysEqual(e.key, key)) continue;
        if (metric < e.minViewMetric || metric > e.maxViewMetric) continue;
        if (e.graphics->SourceGeometryRevision() != currentRevision) continue;
        // Prefer the tightest matching range.
        if (e.minViewMetric > bestMin) {
            best = e.graphics;
            bestMin = e.minViewMetric;
        }
    }
    return best;
}

void ElementGraphicsVariantSet::Clear() {
    m_entries.clear();
}

} // namespace workstation
