#include "workstation/surface/AdaptiveTriangulator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace workstation {
namespace surface {

std::vector<math::Point3d> AdaptiveTriangulator::SpatialFilter(
    const std::vector<math::Point3d>& points, double gridSize, uint32_t maxPointsPerCell) {
    if (points.empty() || gridSize <= 0.0) return points;
    if (maxPointsPerCell == 0) maxPointsPerCell = 1;

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    // Per-cell spatial filter: the coordinate → cell key is fully integer,
    // so neighbouring cells are never merged and the process is stable.
    struct CellAccumulator {
        uint32_t count = 0;
        double bestDist2 = std::numeric_limits<double>::max();
        math::Point3d bestPoint{};
    };
    std::unordered_map<uint64_t, CellAccumulator> cells;

    auto cellKey = [gridSize](const math::Point3d& p) -> uint64_t {
        int64_t ix = static_cast<int64_t>(std::floor(p.x / gridSize));
        int64_t iy = static_cast<int64_t>(std::floor(p.y / gridSize));
        uint64_t ux = static_cast<uint64_t>(ix);
        uint64_t uy = static_cast<uint64_t>(iy);
        return (ux << 32) ^ uy;
    };

    for (const auto& p : points) {
        uint64_t key = cellKey(p);
        auto& cell = cells[key];

        if (cell.count >= maxPointsPerCell) continue; // cell saturated

        // Distance to cell centre (in world units).
        int64_t ix = static_cast<int64_t>(std::floor(p.x / gridSize));
        int64_t iy = static_cast<int64_t>(std::floor(p.y / gridSize));
        double cxWorld = (static_cast<double>(ix) + 0.5) * gridSize;
        double cyWorld = (static_cast<double>(iy) + 0.5) * gridSize;
        double dx = p.x - cxWorld;
        double dy = p.y - cyWorld;
        double dist2 = dx * dx + dy * dy;

        if (cell.count == 0 || dist2 < cell.bestDist2) {
            cell.bestDist2 = dist2;
            cell.bestPoint = p;
        }
        ++cell.count;
    }

    std::vector<math::Point3d> filtered;
    filtered.reserve(cells.size() * maxPointsPerCell);
    for (const auto& [key, cell] : cells) {
        if (cell.count > 0) filtered.push_back(cell.bestPoint);
    }
    return filtered;
}

SurfaceMesh AdaptiveTriangulator::Triangulate(const std::vector<math::Point3d>& points,
                                               const TriangulationSettings& settings) {
    if (points.size() < 3) return {};

    TriangulationSettings effective = settings;

    // Derive a sensible grid size when the caller only supplied a max edge
    // length: a few samples along the longest allowed edge prevents the
    // triangulation from seeing edge-length "holes" that the filter would
    // otherwise keep sampling into.
    double gridSize = effective.pointSpacing;
    if (gridSize <= 0.0 && effective.maxEdgeLength > 0.0) {
        gridSize = effective.maxEdgeLength * 0.25;
    }

    std::vector<math::Point3d> working = points;
    if (effective.adaptive && gridSize > 0.0) {
        working = SpatialFilter(working, gridSize, 1);
        if (working.size() < 3) return {};
    }

    DelaunayTriangulator delaunay;
    delaunay.SetSettings(effective);
    auto mesh = delaunay.Triangulate(working, effective);
    mesh.ComputeEdges();
    mesh.ComputeBounds();
    return mesh;
}

} // namespace surface
} // namespace workstation