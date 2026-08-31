#pragma once
#include "workstation/spatial/BoundingBox.h"
#include <cstdint>

namespace workstation { namespace spatial {

// Confirmed node model recovered from ptPtsToLoadInViewport: an ordered
// hierarchical node with left/right children, a key, and an accumulated point
// count. A bounding volume is added as a design decision so a future frustum
// visibility test has a volume to test against (it is NOT a recovered offset).
struct SpatialNode {
    SpatialNode* left = nullptr;
    SpatialNode* right = nullptr;
    uint64_t key = 0;
    uint64_t pointCount = 0;
    BoundingBox bounds;
};

} // namespace spatial
} // namespace workstation
