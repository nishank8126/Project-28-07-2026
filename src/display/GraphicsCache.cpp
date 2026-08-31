#include "workstation/display/GraphicsCache.h"
#include <limits>

namespace workstation {

CachedGraphicsHandle FindCachedGraphics(const ElementRef& ref,
                                        const GraphicsUnsizedKey& key,
                                        double currentViewMetric) {
    if (key.IsSimple()) {
        auto d = ref.GetDirectGraphics(key.variant);
        if (d) return d;
    }
    return ref.FindSpecialized(key, currentViewMetric);
}

void SaveCachedGraphics(ElementRef& ref,
                        const GraphicsUnsizedKey& key,
                        double currentViewMetric,
                        double validityFactor,
                        CachedGraphicsHandle graphics) {
    // Cheap direct slot only when there is no special state and no metric
    // validity requirement (RE C/H).
    if (key.IsSimple() && validityFactor == 0.0) {
        ref.SetDirectGraphics(key.variant, graphics);
        return;
    }

    double minM, maxM;
    if (validityFactor <= 0.0) {
        minM = -std::numeric_limits<double>::infinity();
        maxM =  std::numeric_limits<double>::infinity();
    } else {
        minM = currentViewMetric / validityFactor;
        maxM = currentViewMetric * validityFactor;
    }

    GraphicsVariant v;
    v.key = key;
    v.minViewMetric = minM;
    v.maxViewMetric = maxM;
    v.graphics = graphics;

    ref.InsertSpecialized(std::move(v));
}

} // namespace workstation
