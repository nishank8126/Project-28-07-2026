#pragma once
#include "workstation/spatial/BoundingBox.h"
#include <array>

namespace workstation { namespace spatial {

// View frustum represented as 6 planes in world-space.
// Each plane is (nx, ny, nz, d) where nx*x + ny*y + nz*z + d >= 0 is the
// inside half-space.
struct Frustum {
    std::array<double, 4> planes[6];

    // Build the frustum from a view-projection matrix (column-major).
    // Reference: Gil Gribb & Klaus Hartmann, "Fast Frustum Culling", 2001.
    void BuildFromViewProjection(const double vp[16]);

    // Test a bounding box against the frustum.
    // Returns true if the box is at least partially inside.
    bool IntersectsBoundingBox(const BoundingBox& box) const;

    // Test a point against the frustum.
    bool ContainsPoint(double x, double y, double z) const;
};

} // namespace spatial
} // namespace workstation
