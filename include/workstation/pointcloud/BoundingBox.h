#pragma once
#include "workstation/spatial/BoundingBox.h"

namespace workstation { namespace pointcloud {

// Re-use the spatial axis-aligned bounding volume (no duplicate). The point
// cloud layer names it directly in its own namespace for readability.
using BoundingBox = workstation::spatial::BoundingBox;

} // namespace pointcloud
} // namespace workstation
