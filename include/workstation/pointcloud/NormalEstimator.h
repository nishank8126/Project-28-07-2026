#pragma once
#include <cstddef>
#include <vector>

namespace workstation { namespace pointcloud {

// Estimates a per-point surface normal from each point's local neighborhood
// (grid-bucketed nearest-neighbor search + PCA plane fit -- the standard
// technique for raw LiDAR/point-cloud data, which has no true per-point
// normal the way a mesh does). This is real per-point CPU work (neighbor
// search + a small eigen-decomposition per point); callers on a large cloud
// should run it lazily (e.g. only when a normal-dependent visualization mode
// is actually selected) rather than unconditionally on every load.
//
// `positions` holds `count` XYZ float triples (as stored in a point cloud's
// XYZ channel). `outNormals` is resized to `count * 3` and filled with unit
// normal vectors, oriented to face +Z by convention (source data here is
// always Z-up) where a stable orientation can't otherwise be determined.
void EstimateNormalsFromPositions(const float* positions, size_t count,
                                   std::vector<float>& outNormals);

} // namespace pointcloud
} // namespace workstation
