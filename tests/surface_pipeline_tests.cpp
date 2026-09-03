// Surface reconstruction pipeline validation (Phase 12).
//
// Exercises the CPU-side surface pipeline end-to-end without a GPU:
//   * SurfaceMesh data model + unique-edge computation (wireframe support)
//   * SurfaceMeshGenerator on a synthetic terrain point cloud
//   * AdaptiveTriangulator spatial filtering + max-edge-length rejection
//   * NormalEstimator plane-normal convergence (OpenMP code path)
//   * SurfaceMeshCache reuse / no-regeneration behaviour

#include "workstation/surface/SurfaceMeshGenerator.h"
#include "workstation/surface/AdaptiveTriangulator.h"
#include "workstation/surface/DelaunayTriangulator.h"
#include "workstation/surface/NormalEstimator.h"
#include "workstation/surface/SurfaceMeshCache.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointChannelManager.h"

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include <memory>
#include <string>

static int g_fail = 0;
static void check(bool c, const char* m) {
    if (!c) { printf("  FAIL: %s\n", m); ++g_fail; }
    else    { printf("  PASS: %s\n", m); }
}

using workstation::pointcloud::PointAttributeChannel;
using workstation::pointcloud::ChannelId;
using workstation::pointcloud::PointFormat;
using workstation::pointcloud::BoundingBox;

namespace {

// Builds an n x n terrain grid centred near the origin. z follows a gentle
// sine surface so the mesh is non-trivial. RGB channel is included so the
// colour-assignment path also runs. The cloud takes ownership of its node.
workstation::pointcloud::PointCloud MakeTerrainCloud(int n, double spacing) {
    std::vector<float> xyz;
    std::vector<uint8_t> rgb;
    xyz.reserve(static_cast<size_t>(n) * n * 3);
    rgb.reserve(static_cast<size_t>(n) * n * 3);

    double minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double x = (i - n / 2.0) * spacing;
            double y = (j - n / 2.0) * spacing;
            double z = 1.0 * std::sin(x * 0.25) * std::cos(y * 0.2);
            xyz.push_back(static_cast<float>(x));
            xyz.push_back(static_cast<float>(y));
            xyz.push_back(static_cast<float>(z));
            rgb.push_back(static_cast<uint8_t>(40 + (i * 37) % 200));
            rgb.push_back(static_cast<uint8_t>(80 + (j * 53) % 160));
            rgb.push_back(150);
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
            minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
        }
    }

    BoundingBox bb;
    bb.minX = minX; bb.minY = minY; bb.minZ = minZ;
    bb.maxX = maxX; bb.maxY = maxY; bb.maxZ = maxZ;

    workstation::pointcloud::PointCloud cloud;
    auto node = std::make_unique<workstation::pointcloud::PointCloudNode>();
    node->setBounds(bb);
    node->channels().AddChannel(
        workstation::pointcloud::CreateChannel(ChannelId::XYZ, PointFormat::Float32,
                                               xyz.size() / 3, xyz.data()));
    node->channels().AddChannel(
        workstation::pointcloud::CreateChannel(ChannelId::RGB, PointFormat::UInt8,
                                               rgb.size() / 3, rgb.data()));
    cloud.SetRoot(node.release());
    cloud.Finalize();
    return cloud;
}

} // namespace
int main() {
    using namespace workstation;
    printf("== NakshaPointEngine :: Surface Reconstruction Pipeline Tests ==\n\n");

    // TEST 1: ComputeEdges deduplicates interior edges.
    {
        surface::SurfaceMesh m;
        m.Vertices().resize(4);
        m.Triangles().push_back({0, 1, 2});
        m.Triangles().push_back({0, 2, 3}); // shares edge 0-2
        m.ComputeEdges();
        // 2 triangles * 3 edges = 6 raw; shared edge (0,2) dedups -> 5 unique
        // edges = 10 indices.
        check(m.EdgeCount() == 10, "TEST1 shared triangle edge removed (10 indices)");

        bool valid = m.EdgeCount() == 10;
        for (size_t e = 0; e + 1 < m.GetEdgeIndexBuffer().size(); e += 2) {
            uint32_t a = m.GetEdgeIndexBuffer()[e];
            uint32_t b = m.GetEdgeIndexBuffer()[e + 1];
            if (a == b) valid = false;
            if (a >= 4 || b >= 4) valid = false;
        }
        check(valid, "TEST1 edge indices reference existing vertices");
    }

    // TEST 2: AdaptiveTriangulator::SpatialFilter thins dense clusters.
    {
        surface::AdaptiveTriangulator tri;
        std::vector<math::Point3d> dense; // 10x10 cluster of near-coincident points
        for (int i = 0; i < 10; ++i)
            for (int j = 0; j < 10; ++j)
                dense.push_back({i * 0.01, j * 0.01, 0.0});
        auto filtered = tri.SpatialFilter(dense, 0.1, 1);
        check(filtered.size() < dense.size(), "TEST2 spatial filter reduces point count");
        check(!filtered.empty(), "TEST2 spatial filter keeps representatives");
    }
// TEST 3: Max-edge-length constraint rejects bridge triangles across a gap.
    {
        constexpr int N = 20;
        constexpr double spacing = 1.0;
        std::vector<math::Point3d> pts;
        // Grid with a horizontal band removed (a scan gap ~4x the spacing).
        for (int i = 0; i < N; ++i) {
            if (i >= 8 && i <= 11) continue; // gap
            for (int j = 0; j < N; ++j) {
                pts.push_back({i * spacing, j * spacing, 0.0});
            }
        }

        surface::TriangulationSettings unlimited;
        unlimited.adaptive = false;
        unlimited.maxEdgeLength = 0.0;

        surface::TriangulationSettings limited = unlimited;
        limited.maxEdgeLength = spacing * 2.5;

        surface::DelaunayTriangulator delaunay;
        auto meshFree = delaunay.Triangulate(pts, unlimited);
        auto meshLimited = delaunay.Triangulate(pts, limited);

        check(meshLimited.TriangleCount() < meshFree.TriangleCount(),
              "TEST3 max edge length rejects bridge triangles over the gap");
        check(meshFree.TriangleCount() > 0 && meshLimited.TriangleCount() > 0,
              "TEST3 both meshes generated");
    }

    // TEST 4: End-to-end terrain surface generation.
    {
        surface::SurfaceGenerationParams params;
        params.maxPoints = 100000;
        params.removeDuplicates = true;
        params.computeNormals = true;
        params.normalNeighborRadius = 1.6; // spacing 1.0 -> include grid neighbours
        params.normalNeighborCount = 12;
        params.adaptiveTriangulation = true;
        params.maxEdgeLength = 2.5;
        params.pointSpacing = 1.0;

        auto cloud = MakeTerrainCloud(24, 1.0);
        surface::SurfaceMeshGenerator gen;
        auto mesh = gen.Generate(cloud, params);

        check(!mesh.IsEmpty(), "TEST4 terrain mesh generated");
        check(mesh.VertexCount() == 24 * 24, "TEST4 all terrain points become vertices");
        check(mesh.TriangleCount() > 0, "TEST4 mesh has triangles");
        check(mesh.EdgeCount() > mesh.TriangleCount(), "TEST4 unique edge set computed");

        auto& b = mesh.GetBounds();
        check(b.maxX > b.minX && b.maxY > b.minY && b.maxZ >= b.minZ,
              "TEST4 mesh bounds valid");

        // Normals: after averaging over a horizontal grid they must be quasi-vertical.
        int upNormals = 0;
        for (const auto& v : mesh.Vertices()) {
            double len = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] +
                                   v.normal[2] * v.normal[2]);
            if (len > 0.8 && std::fabs(v.normal[2] / len) > 0.9) ++upNormals;
        }
        check(upNormals == static_cast<int>(mesh.VertexCount()),
              "TEST4 all vertex normals quasi-vertical after averaging");

        // Colours: RGB channel assigned through the spatial-hash lookup.
        int colored = 0;
        for (const auto& v : mesh.Vertices()) {
            if (v.color[0] > 0.05f && v.color[1] > 0.1f) ++colored;
        }
        check(colored == static_cast<int>(mesh.VertexCount()),
              "TEST4 vertex colours assigned from cloud RGB");
    }
// TEST 5: NormalEstimator converges on a coplanar neighbourhood.
    {
        surface::NormalEstimator est;
        std::vector<math::Point3d> plane = {
            {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1},
            {-1, 0, 1}, {0, -1, 1}, {2, 0, 1}, {0, 2, 1},
        };
        std::vector<float> normals;
        surface::NormalEstimationParams np;
        np.neighborCount = 8;
        np.neighborRadius = 10.0;
        np.useOpenMP = true;
        est.ComputeNormalsFromPoints(plane, normals, np);
        check(normals.size() == plane.size() * 3, "TEST5 normals buffer sized per vertex");
        bool vertical = true;
        for (size_t i = 0; i < plane.size(); ++i) {
            double z = normals[i * 3 + 2];
            if (std::fabs(z) < 0.95) vertical = false;
        }
        check(vertical, "TEST5 coplanar neighbourhood produces Z normals");
    }

    // TEST 6: SurfaceMeshCache avoids regeneration on repeat calls.
    {
        auto cloud = MakeTerrainCloud(8, 1.0);
        surface::SurfaceMeshCache cache; // allocator == nullptr -> CPU-only path
        surface::SurfaceGenerationParams params;
        params.adaptiveTriangulation = true;
        params.pointSpacing = 1.0;
        params.maxEdgeLength = 2.0;

        cache.SetCurrentFrame(42);
        auto* first = cache.GetOrCreate(cloud.Id(), cloud, params);
        check(first != nullptr && first->isValid, "TEST6 cache entry created");

        auto* meshPtr1 = first ? &first->mesh : nullptr;
        auto* second = cache.GetOrCreate(cloud.Id(), cloud, params);
        check(second == first, "TEST6 cache hit returns the same entry");
        check(second && meshPtr1 && meshPtr1 == &second->mesh,
              "TEST6 cached mesh not regenerated");
        check(second && second->mesh.EdgeCount() > 0,
              "TEST6 cached mesh has wireframe edges");

        cache.Evict(10); // max age 10 frames
        check(cache.GetCacheSize() == 1, "TEST6 fresh entry survives eviction");

        cache.Remove(cloud.Id());
        check(cache.GetCacheSize() == 0, "TEST6 Remove clears the entry");
    }

    // TEST 7: Phase 12 performance metrics are captured by the generator.
    {
        auto cloud = MakeTerrainCloud(20, 1.0); // 400 points
        surface::SurfaceMeshGenerator gen;
        surface::SurfaceGenerationParams params;
        params.computeNormals = true;
        params.adaptiveTriangulation = true;
        params.pointSpacing = 1.0;
        params.maxEdgeLength = 2.5;

        auto mesh = gen.Generate(cloud, params);
        const auto& st = gen.GetLastStats();

        check(st.inputPointCount == 400, "TEST7 input point count recorded");
        check(st.filteredPointCount == 400, "TEST7 filtered point count recorded");
        check(st.vertexCount == mesh.VertexCount(),
              "TEST7 stats vertex count matches mesh");
        check(st.triangleCount == mesh.TriangleCount(),
              "TEST7 stats triangle count matches mesh");
        check(st.generationTimeMs > 0.0, "TEST7 total generation time measured");
        check(st.triangulationTimeMs > 0.0, "TEST7 triangulation time measured");
        check(st.normalTimeMs > 0.0, "TEST7 normal calculation time measured");
        check(st.generationTimeMs >= st.triangulationTimeMs + st.normalTimeMs,
              "TEST7 total time >= stage times");
        check(st.triangleCount > 0 && st.triangleCount / st.generationTimeMs > 0.0,
              "TEST7 throughput computable");
    }

    printf("\n%s\n", g_fail == 0 ? "SURFACE_PIPELINE_OK" : "SURFACE_PIPELINE_FAIL");
    return g_fail;
}