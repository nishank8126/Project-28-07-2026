#pragma once
#include "workstation/display/ElementRef.h"
#include "workstation/display/CacheKeys.h"
#include "workstation/display/CachedGraphics.h"
#include <memory>

namespace workstation {

// RE B/C/D/G/K: locate a previously generated retained representation.
// Does NOT generate graphics (that arrives in Piece 2).
//   - simple key  -> consult the cheap direct variant slot
//   - special key -> consult the specialized variant set, validating
//                    transform key, variant, style key, filter key and the
//                    view-metric validity range.
// A mismatch in the element's geometry revision also yields a miss so stale
// graphics are never returned.
CachedGraphicsHandle FindCachedGraphics(const ElementRef& ref,
                                        const GraphicsUnsizedKey& key,
                                        double currentViewMetric);

// RE F/H: store generated graphics.
//   - simple key AND validityFactor == 0 -> direct common variant slot
//   - otherwise -> specialized entry with validity range computed as
//       validityFactor <= 0 : (-inf, +inf)
//       validityFactor  > 0 : [metric / F, metric * F]
// Duplicate / overlapping entries are NOT merged (RE H).
void SaveCachedGraphics(ElementRef& ref,
                        const GraphicsUnsizedKey& key,
                        double currentViewMetric,
                        double validityFactor,
                        CachedGraphicsHandle graphics);

} // namespace workstation
