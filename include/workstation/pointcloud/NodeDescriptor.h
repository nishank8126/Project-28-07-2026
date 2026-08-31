#pragma once
#include "workstation/pointcloud/BoundingBox.h"
#include <cstdint>

namespace workstation { namespace pointcloud {

// Abstraction of a block's node metadata (FUN_180078840 node descriptor).
// The exact binary layout is NOT reversed; this is an independent representation.
struct NodeDescriptor {
    enum class Type : uint32_t {
        Normal = 0,   // -> PointCloudNode
        Voxel  = 1    // -> VoxelNode
    };

    BoundingBox bounds;
    uint32_t flags = 0;
    uint32_t type = 0;   // NodeDescriptor::Type value
};

} // namespace pointcloud
} // namespace workstation
