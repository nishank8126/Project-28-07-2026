#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/surface/DelaunayTriangulator.h"
#include "workstation/surface/SurfaceMesh.h"

#include <vector>
#include <cstdint>

namespace workstation {
namespace surface {

// Density-adaptive triangulation front-end.
//
// Layer 1 (spatial filtering): points are thinned on a uniform grid whose
// cell size follows the configured point spacing / maximum edge length. Each
// grid cell keeps at most a small number of points (nearest to the cell
// center), so dense urban scans are decimated before triangulation while
// sparse terrain keeps its resolution.
//
// Layer 2 (Delaunay): the filtered set is triangulated with the Bowyer-Watson
// implementation in DelaunayTriangulator. Triangles whose edges exceed
// maxEdgeLength are rejected, which suppresses the long bridges / erroneous
// terrain connections that appear across scan gaps.
class AdaptiveTriangulator {
public:
    SurfaceMesh Triangulate(const std::vector<math::Point3d>& points,
                            const TriangulationSettings& settings = {});

    // Thins `points` on a uniform grid of `gridSize` world units, keeping at
    // most `maxPointsPerCell` representative points per cell (the one closest
    // to the cell centre). O(N) via a hash of integer cell coordinates.
    std::vector<math::Point3d> SpatialFilter(const std::vector<math::Point3d>& points,
                                             double gridSize,
                                             uint32_t maxPointsPerCell = 1);

    void SetSettings(const TriangulationSettings& s) { settings_ = s; }
    const TriangulationSettings& GetSettings() const { return settings_; }

    // Debug: triangle counts from underlying DelaunayTriangulator
    size_t GetTrianglesBeforeValidation() const { return triangulator_.GetTrianglesBeforeValidation(); }
    size_t GetTrianglesAfterEdgeFilter() const { return triangulator_.GetTrianglesAfterEdgeFilter(); }
    size_t GetTrianglesAfterZFilter() const { return triangulator_.GetTrianglesAfterZFilter(); }

private:
    TriangulationSettings settings_;
    DelaunayTriangulator triangulator_;
};

} // namespace surface
} // namespace workstation