#include "workstation/surface/SurfaceMeshGenerator.h"
#include "workstation/pointcloud/PointAttributeChannel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
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

// Splits every triangle into 3 unique vertices carrying that triangle's own
// flat face normal (cross product of its edges), instead of the shared,
// averaged-across-neighbours vertex normal NormalEstimator produces. This is
// what gives a triangulated point-cloud surface the "faceted crystalline"
// look seen in reference hillshade renderers (MicroStation, etc.): each
// facet catches the fixed light direction independently, so adjacent facets
// at different slopes read as distinctly different brightness rather than
// blending smoothly into one another.
void FlattenFaceNormals(SurfaceMesh& mesh) {
    const auto& oldVertices = mesh.Vertices();
    const auto& oldTriangles = mesh.Triangles();
    if (oldTriangles.empty()) return;

    std::vector<SurfaceVertex> flatVertices;
    std::vector<SurfaceTriangle> flatTriangles;
    flatVertices.reserve(oldTriangles.size() * 3);
    flatTriangles.reserve(oldTriangles.size());

    for (const auto& tri : oldTriangles) {
        const SurfaceVertex& v0 = oldVertices[tri.indices[0]];
        const SurfaceVertex& v1 = oldVertices[tri.indices[1]];
        const SurfaceVertex& v2 = oldVertices[tri.indices[2]];

        double e1[3] = {v1.position[0] - v0.position[0], v1.position[1] - v0.position[1], v1.position[2] - v0.position[2]};
        double e2[3] = {v2.position[0] - v0.position[0], v2.position[1] - v0.position[1], v2.position[2] - v0.position[2]};
        double nx = e1[1] * e2[2] - e1[2] * e2[1];
        double ny = e1[2] * e2[0] - e1[0] * e2[2];
        double nz = e1[0] * e2[1] - e1[1] * e2[0];
        double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        float fn[3];
        if (len > EPSILON) {
            fn[0] = static_cast<float>(nx / len);
            fn[1] = static_cast<float>(ny / len);
            fn[2] = static_cast<float>(nz / len);
        } else {
            fn[0] = 0.0f; fn[1] = 0.0f; fn[2] = 1.0f;
        }

        uint32_t base = static_cast<uint32_t>(flatVertices.size());
        for (int k = 0; k < 3; ++k) {
            SurfaceVertex nv = oldVertices[tri.indices[k]];
            nv.normal[0] = fn[0];
            nv.normal[1] = fn[1];
            nv.normal[2] = fn[2];
            flatVertices.push_back(nv);
        }
        flatTriangles.push_back({{base, base + 1, base + 2}});
    }

    mesh.Vertices() = std::move(flatVertices);
    mesh.Triangles() = std::move(flatTriangles);
}

} // namespace

void SurfaceMeshGenerator::AssignVertexColors(SurfaceMesh& mesh, pointcloud::PointCloud& cloud) {
    if (!cloud.Root()) return;

    auto* xyzChannel = cloud.Root()->channels().GetChannel(pointcloud::ChannelId::XYZ);
    auto* rgbChannel = cloud.Root()->channels().GetChannel(pointcloud::ChannelId::RGB);
    auto* clsChannel = cloud.Root()->channels().GetChannel(pointcloud::ChannelId::Classification);
    if (!xyzChannel || !rgbChannel) return;

    const size_t pointCount = xyzChannel->Count();
    auto& vertices = mesh.Vertices();
    if (pointCount == 0 || vertices.empty()) return;

    size_t stride = 1;
    if (pointCount > kColorLookupPointBudget) {
        stride = pointCount / kColorLookupPointBudget + 1;
    }

    // Decode every sampled point's XYZ+RGB into flat arrays ONCE up front.
    // The grid-query loop below previously re-decoded XYZ (via a per-call
    // channel lookup + 3 memcpys) for every candidate point it examined in
    // each vertex's 27-cell neighbourhood search - for a few million source
    // points and tens of thousands of mesh vertices that's on the order of
    // a hundred million redundant channel decodes, which is what actually
    // made surface generation take over a minute (triangulation itself was
    // a few seconds). Flat-array indexing turns each of those lookups into
    // a single memory read.
    //
    // Also fixes a real correctness bug: LasFileReader stores RGB as
    // Float32 (see LasFileReader.cpp), but this used to call the uint8
    // ReadRGB accessor, which just returns raw bytes of the float data
    // reinterpreted as color components - not the actual decoded colour.
    std::vector<float> px, py, pz;
    std::vector<uint8_t> pr, pg, pb, classifications;
    size_t reserveHint = pointCount / stride + 1;
    px.reserve(reserveHint); py.reserve(reserveHint); pz.reserve(reserveHint);
    pr.reserve(reserveHint); pg.reserve(reserveHint); pb.reserve(reserveHint);
    classifications.reserve(reserveHint);

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double minZ = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    double maxZ = std::numeric_limits<double>::lowest();

    auto& channels = cloud.Root()->channels();
    for (size_t i = 0; i < pointCount; i += stride) {
        double xyz[3];
        if (!channels.ReadXYZ(i, xyz)) continue;

        float rgbf[3];
        uint8_t rgb8[3];
        if (channels.ReadRGBFloat(i, rgbf)) {
            rgb8[0] = static_cast<uint8_t>(std::clamp(rgbf[0], 0.0f, 1.0f) * 255.0f);
            rgb8[1] = static_cast<uint8_t>(std::clamp(rgbf[1], 0.0f, 1.0f) * 255.0f);
            rgb8[2] = static_cast<uint8_t>(std::clamp(rgbf[2], 0.0f, 1.0f) * 255.0f);
        } else if (!channels.ReadRGB(i, rgb8)) {
            continue;
        }

        px.push_back(static_cast<float>(xyz[0]));
        py.push_back(static_cast<float>(xyz[1]));
        pz.push_back(static_cast<float>(xyz[2]));
        pr.push_back(rgb8[0]); pg.push_back(rgb8[1]); pb.push_back(rgb8[2]);

        uint8_t cls = 0;
        if (clsChannel) {
            channels.ReadClassification(i, cls);
        }
        classifications.push_back(cls);

        minX = std::min(minX, xyz[0]); maxX = std::max(maxX, xyz[0]);
        minY = std::min(minY, xyz[1]); maxY = std::max(maxY, xyz[1]);
        minZ = std::min(minZ, xyz[2]); maxZ = std::max(maxZ, xyz[2]);
    }

    const size_t n = px.size();
    if (n == 0) return;
    double dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
    double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (diag <= 0.0) return;

    // Cell size ~ twice the average sample spacing so each cell holds tens of
    // points; queries then cover far more than the true nearest neighbourhood.
    ColorLookupGrid grid;
    grid.cellSize = diag / std::cbrt(static_cast<double>(n)) * 2.0;
    grid.cells.reserve(n / 8 + 64);

    for (size_t i = 0; i < n; ++i) {
        int64_t gx = static_cast<int64_t>(std::floor((px[i] - minX) / grid.cellSize));
        int64_t gy = static_cast<int64_t>(std::floor((py[i] - minY) / grid.cellSize));
        int64_t gz = static_cast<int64_t>(std::floor((pz[i] - minZ) / grid.cellSize));
        grid.cells[ColorLookupGrid::Key(gx, gy, gz)].push_back(static_cast<uint32_t>(i));
    }

    // Query: nearest source point in the 27-cell neighbourhood.
    for (auto& v : vertices) {
        double vx = v.position[0], vy = v.position[1], vz = v.position[2];
        int64_t gx = static_cast<int64_t>(std::floor((vx - minX) / grid.cellSize));
        int64_t gy = static_cast<int64_t>(std::floor((vy - minY) / grid.cellSize));
        int64_t gz = static_cast<int64_t>(std::floor((vz - minZ) / grid.cellSize));

        double bestDist = std::numeric_limits<double>::max();
        uint32_t bestIdx = 0;
        bool found = false;

        for (int64_t ox = -1; ox <= 1; ++ox) {
            for (int64_t oy = -1; oy <= 1; ++oy) {
                for (int64_t oz = -1; oz <= 1; ++oz) {
                    auto it = grid.cells.find(ColorLookupGrid::Key(gx + ox, gy + oy, gz + oz));
                    if (it == grid.cells.end()) continue;
                    for (uint32_t idx : it->second) {
                        double ddx = px[idx] - vx, ddy = py[idx] - vy, ddz = pz[idx] - vz;
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

        if (found) {
            v.color[0] = pr[bestIdx] / 255.0f;
            v.color[1] = pg[bestIdx] / 255.0f;
            v.color[2] = pb[bestIdx] / 255.0f;
            v.classificationID = classifications[bestIdx];
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
    ts.maxElevationJump = params.maxElevationJump;

    const auto tTri = std::chrono::steady_clock::now();
    auto mesh = triangulator_.Triangulate(points, ts);
    mesh.ComputeBounds();
    lastStats_.triangulationTimeMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tTri).count();

    // Record triangle validation stats
    lastStats_.trianglesBeforeValidation = triangulator_.GetTrianglesBeforeValidation();
    lastStats_.trianglesAfterEdgeFilter = triangulator_.GetTrianglesAfterEdgeFilter();
    lastStats_.trianglesAfterZFilter = triangulator_.GetTrianglesAfterZFilter();

    fprintf(stderr,
        "[SurfaceGen] Triangles before validation: %zu\n"
        "[SurfaceGen] Triangles after edge filter: %zu\n"
        "[SurfaceGen] Triangles after Z-filter:   %zu\n",
        lastStats_.trianglesBeforeValidation,
        lastStats_.trianglesAfterEdgeFilter,
        lastStats_.trianglesAfterZFilter);
    fflush(stderr);

    if (params.computeNormals) {
        // Flat per-face normals (with vertex duplication) rather than
        // NormalEstimator's kNN-smoothed per-vertex normals: this is what
        // produces the faceted hillshade look instead of a smoothly lit
        // blob. Edges must be recomputed afterwards since triangle vertex
        // indices change.
        const auto tNormal = std::chrono::steady_clock::now();
        FlattenFaceNormals(mesh);
        lastStats_.normalTimeMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tNormal).count();
    }
    mesh.ComputeEdges();

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

    fprintf(stderr, "[SurfaceGen] Input points: %zu\n", lastStats_.inputPointCount);
    fprintf(stderr, "[SurfaceGen] Filtered points: %zu\n", lastStats_.filteredPointCount);
    fprintf(stderr, "[SurfaceGen] Output vertices: %u\n", lastStats_.vertexCount);
    fprintf(stderr, "[SurfaceGen] Triangles: %u\n", lastStats_.triangleCount);
    fprintf(stderr, "[SurfaceGen] Triangles before validation: %zu\n", lastStats_.trianglesBeforeValidation);
    fprintf(stderr, "[SurfaceGen] Triangles rejected (edge): %zu\n",
            lastStats_.trianglesBeforeValidation - lastStats_.trianglesAfterEdgeFilter);
    fprintf(stderr, "[SurfaceGen] Triangles rejected (Z-jump): %zu\n",
            lastStats_.trianglesAfterEdgeFilter - lastStats_.trianglesAfterZFilter);
    fprintf(stderr, "[SurfaceGen] Normals computed: %s\n", params.computeNormals ? "yes" : "no");
    const auto& b = mesh.GetBounds();
    fprintf(stderr, "[SurfaceGen] Bounds: min(%.2f, %.2f, %.2f) max(%.2f, %.2f, %.2f)\n",
            b.minX, b.minY, b.minZ, b.maxX, b.maxY, b.maxZ);
    fprintf(stderr, "[SurfaceGen] Timing: total=%.1fms triang=%.1fms normals=%.1fms color=%.1fms\n",
            lastStats_.generationTimeMs, lastStats_.triangulationTimeMs,
            lastStats_.normalTimeMs, lastStats_.colorTimeMs);
    fflush(stderr);

    return mesh;
}

} // namespace surface
} // namespace workstation
