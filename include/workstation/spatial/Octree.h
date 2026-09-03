#pragma once
#include "workstation/spatial/BoundingBox.h"
#include <cstdint>
#include <memory>
#include <array>

namespace workstation { namespace spatial {

// Octree node for spatial hierarchical subdivision of point clouds.
//
// Each node subdivides its bounding box into 8 octants.  The octant index is
// determined by comparing the point's centre against the node's midpoint:
//   bit 0 = x >= midX   (right)
//   bit 1 = y >= midY   (back)
//   bit 2 = z >= midZ   (top)
//
// Leaf nodes store a contiguous range of points [pointOffset, pointOffset+pointCount).
// Internal nodes store 8 children (some may be nullptr) and aggregate stats.
//
// The tree is built bottom-up from a flat array of sorted point indices.
//
struct OctreeNode {
    static constexpr int kMaxDepth = 16;
    static constexpr int kMaxLeafPoints = 4096;

    BoundingBox bounds;

    // Internal node: 8 children, indexed by octant.
    // child[i] == nullptr if that octant is empty.
    std::array<std::unique_ptr<OctreeNode>, 8> children;

    // Leaf data: index range into the external point array.
    uint32_t pointOffset = 0;
    uint32_t pointCount = 0;

    // Aggregate stats (valid for both leaves and internals).
    uint64_t totalPoints = 0;
    uint32_t depth = 0;

    bool IsLeaf() const { return !children[0] && !children[1]; }
    bool IsEmpty() const { return totalPoints == 0; }

    // Determine which octant a point falls into relative to this node's bounds.
    // Returns 0-7.
    int GetOctant(double px, double py, double pz) const {
        double mx = (bounds.minX + bounds.maxX) * 0.5;
        double my = (bounds.minY + bounds.maxY) * 0.5;
        double mz = (bounds.minZ + bounds.maxZ) * 0.5;
        int idx = 0;
        if (px >= mx) idx |= 1;
        if (py >= my) idx |= 2;
        if (pz >= mz) idx |= 4;
        return idx;
    }

    // Get the bounding box for a specific octant child.
    BoundingBox GetOctantBounds(int octant) const {
        double mx = (bounds.minX + bounds.maxX) * 0.5;
        double my = (bounds.minY + bounds.maxY) * 0.5;
        double mz = (bounds.minZ + bounds.maxZ) * 0.5;
        BoundingBox ob;
        ob.minX = (octant & 1) ? mx : bounds.minX;
        ob.maxX = (octant & 1) ? bounds.maxX : mx;
        ob.minY = (octant & 2) ? my : bounds.minY;
        ob.maxY = (octant & 2) ? bounds.maxY : my;
        ob.minZ = (octant & 4) ? mz : bounds.minZ;
        ob.maxZ = (octant & 4) ? bounds.maxZ : mz;
        return ob;
    }
};

} // namespace spatial
} // namespace workstation
