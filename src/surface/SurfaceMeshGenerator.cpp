#include "workstation/surface/SurfaceMeshGenerator.h"
#include "workstation/pointcloud/PointAttributeChannel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace workstation {
namespace surface {

static constexpr double EPSILON = 1e-10;
static constexpr size_t kColorLookupPointBudget = 4'000'000;

std::vector<math::Point3d> SurfaceMeshGenerator::ExtractPoints(pointcloud::PointCloud& cloud) {
    std::vector<math::Point3d> points;
    if (!cloud.Root()) return points;

    auto* xyzChannel = cloud.Root()->channels().GetChannel(pointcloud::ChannelId::XYZ);
    if (!xyzChannel || xyzChannel->Count() == 0) return points;

    points.reserve(xyzChannel->Count());
    for (size_t i = 0; i < xyzChannel->Count(); ++i) {
        double xyz[3];
        if (cloud.Root()->channels().ReadXYZ(i, xyz)) {
            points.push_back({xyz[0], xyz[1], xyz[2]});
        }
    }

    return points;
}

std::vector<math::Point3d> SurfaceMeshGenerator::FilterPoints(
    const std::vector<math::Point3d>& points,
    const SurfaceGenerationParams& params) {

    std::vector<math::Point3d> filtered = points;

    if (params.removeDuplicates) {
        std::sort(filtered.begin(), filtered.end(), [](const math::Point3d& a, const math::Point3d& b) {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        });
        auto last = std::unique(filtered.begin(), filtered.end(),
            [threshold = params.duplicateThreshold](const math::Point3d& a, const math::Point3d& b) {
                return std::abs(a.x - b.x) < threshold &&
                       std::abs(a.y - b.y) < threshold &&
                       std::abs(a.z - b.z) < threshold;
            });
        filtered.erase(last, filtered.end());
    }

    if (params.maxPoints > 0 && filtered.size() > params.maxPoints) {
        double step = static_cast<double>(filtered.size()) / params.maxPoints;
        std::vector<math::Point3d> downsampled;
        downsampled.reserve(params.maxPoints);
        for (size_t i = 0; i < filtered.size(); i += static_cast<size_t>(step)) {
            downsampled.push_back(filtered[i]);
        }
        filtered = std::move(downsampled);
    }

    return filtered;
}

namespace {

// Uniform 3-D hash grid used by AssignVertexColors. Each cell stores the
// source indices of the points that fall inside it; queries inspect the
// 3x3x3 neighbourhood around a mesh vertex, so the lookup cost is O(N + V)
// rather than the previous O(V * N) brute force.
struct ColorLookupGrid {
    double cellSize = 1.0;
    std::unordered_map<uint64_t, std::vector<uint32_t>> cells;

    static uint64_t Key(int64_t gx, int64_t gy, int64_t gz) {
        uint64_t ux = static_cast<uint64_t>(gx);
        uint64_t uy = static_cast<uint64_t>(gy);
        uint64_t uz = static_cast<uint64_t>(gz);
        // Mix coordinates so nearby cells don't trivially collide.
        return ux * 73856093ull ^ uy * 19349663ull ^ uz * 83492791ull;
    }
};

} // namespace

void SurfaceMeshGenerator::AssignVertexColors(SurfaceMesh& mesh, pointcloud::PointCloud& cloud) {
    if (!cloud.Root()) return;

    auto* xyzChannel = cloud.Root()->channels().GetChannel(pointcloud::ChannelId::XYZ);
    auto* rgbChannel = cloud.Root()->channels().GetChannel(pointcloud::ChannelId::RGB);
    if (!xyzChannel || !rgbChannel) return;

    const size_t pointCount = xyzChannel->Count();
    auto& vertices = mesh.Vertices();
    if (pointCount == 0 || vertices.empty()) return;

    // Pass 1: source bounds.
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double minZ = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    double maxZ = std::numeric_limits<double>::lowest();
    for (size_t i = 0; i < pointCount; ++i) {
        double xyz[3];
        if (!cloud.Root()->channels().ReadXYZ(i, xyz)) continue;
        minX = std::min(minX, xyz[0]); maxX = std::max(maxX, xyz[0]);
        minY = std::min(minY, xyz[1]); maxY = std::max(maxY, xyz[1]);
        minZ = std::min(minZ, xyz[2]); maxZ = std::max(maxZ, xyz[2]);
    }
    double dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
    double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (diag <= 0.0 || pointCount == 0) return;

    // Cell size ~ twice the average sample spacing so each cell holds tens of
    // points; queries then cover far more than the true nearest neighbourhood.
    ColorLookupGrid grid;
    grid.cellSize = diag / std::cbrt(static_cast<double>(pointCount)) * 2.0;
    grid.cells.reserve(pointCount / 8 + 64);

    size_t stride = 1;
    if (pointCount > kColorLookupPointBudget) {
        stride = pointCount / kColorLookupPointBudget + 1;
    }

    // Pass 2: populate the grid.
    for (size_t i = 0; i < pointCount; i += stride) {
        double xyz[3];
        if (!cloud.Root()->channels().ReadXYZ(i, xyz)) continue;
        int64_t gx = static_cast<int64_t>(std::floor((xyz[0] - minX) / grid.cellSize));
        int64_t gy = static_cast<int64_t>(std::floor((xyz[1] - minY) / grid.cellSize));
        int64_t gz = static_cast<int64_t>(std::floor((xyz[2] - minZ) / grid.cellSize));
        grid.cells[ColorLookupGrid::Key(gx, gy, gz)].push_back(static_cast<uint32_t>(i));
    }

    // Query: nearest source point in the 27-cell neighbourhood.
    auto fileChannels = &cloud.Root()->channels();
    for (auto& v : vertices) {
        double px = v.position[0], py = v.position[1], pz = v.position[2];
        int64_t gx = static_cast<int64_t>(std::floor((px - minX) / grid.cellSize));
        int64_t gy = static_cast<int64_t>(std::floor((py - minY) / grid.cellSize));
        int64_t gz = static_cast<int64_t>(std::floor((pz - minZ) / grid.cellSize));

        double bestDist = std::numeric_limits<double>::max();
        uint32_t bestIdx = 0;
        bool found = false;

        for (int64_t ox = -1; ox <= 1; ++ox) {
            for (int64_t oy = -1; oy <= 1; ++oy) {
                for (int64_t oz = -1; oz <= 1; ++oz) {
                    auto it = grid.cells.find(ColorLookupGrid::Key(gx + ox, gy + oy, gz + oz));
                    if (it == grid.cells.end()) continue;
                    for (uint32_t idx : it->second) {
                        double xyz[3];
                        if (!fileChannels->ReadXYZ(idx, xyz)) continue;
                        double ddx = xyz[0] - px, ddy = xyz[1] - py, ddz = xyz[2] - pz;
                        double dist = ddx * ddx + ddy * ddy + ddz * ddz;
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = idx;
                            found = true;
                        }
                    }
                }
            }
        }

        uint8_t rgb[3];
        if (found && fileChannels->ReadRGB(bestIdx, rgb)) {
            v.color[0] = rgb[0] / 255.0f;
            v.color[1] = rgb[1] / 255.0f;
            v.color[2] = rgb[2] / 255.0f;
        }
        // Not found: point is outside the indexed extent, keep the mesh's
        // seeded mid-grey colour (same visual as the no-RGB path).
    }
}

SurfaceMesh SurfaceMeshGenerator::Generate(pointcloud::PointCloud& cloud,
                                             const SurfaceGenerationParams& params) {
    params_ = params;
    const auto tTotal = std::chrono::steady_clock::now();
    lastStats_ = {};

    auto points = ExtractPoints(cloud);
    lastStats_.inputPointCount = points.size();
    if (points.empty()) return {};

    points = FilterPoints(points, params);
    lastStats_.filteredPointCount = points.size();
    if (points.size() < 3) return {};

    // Adaptive triangulation: density-based spatial filtering + Delaunay with
    // a maximum-edge constraint so scan gaps don't produce long bridge edges.
    TriangulationSettings ts;
    ts.adaptive = params.adaptiveTriangulation;
    ts.maxEdgeLength = params.maxEdgeLength;
    ts.pointSpacing = params.pointSpacing;

    const auto tTri = std::chrono::steady_clock::now();
    auto mesh = triangulator_.Triangulate(points, ts);
    mesh.ComputeEdges();
    mesh.ComputeBounds();
    lastStats_.triangulationTimeMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tTri).count();

    if (params.computeNormals) {
        NormalEstimationParams np;
        np.neighborCount = params.normalNeighborCount;
        np.neighborRadius = params.normalNeighborRadius;
        np.useOpenMP = true;
        const auto tNormal = std::chrono::steady_clock::now();
        normalEstimator_.ComputeNormals(mesh, np);
        lastStats_.normalTimeMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tNormal).count();
    }

    const auto tColor = std::chrono::steady_clock::now();
    AssignVertexColors(mesh, cloud);
    lastStats_.colorTimeMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tColor).count();

    mesh.ComputeBounds();
    mesh.SetSourceCloudID(cloud.Id());

    lastStats_.vertexCount = mesh.VertexCount();
    lastStats_.triangleCount = mesh.TriangleCount();
    lastStats_.generationTimeMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tTotal).count();

    return mesh;
}

} // namespace surface
} // namespace workstation
