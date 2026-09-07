#pragma once
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/surface/AdaptiveTriangulator.h"
#include "workstation/surface/NormalEstimator.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/math/Point3d.h"

#include <vector>
#include <cstdint>

namespace workstation {
namespace surface {

struct SurfaceGenerationParams {
    double spatialFilterRadius = 0.0;
    // DelaunayTriangulator::Triangulate is O(n^2) - it has no spatial
    // acceleration for point location, so every inserted point does a linear
    // scan over every triangle built so far. AdaptiveTriangulator is meant to
    // thin the input first, but its "0 = automatic" grid-size derivation
    // (see AdaptiveTriangulator::Triangulate) never actually computes a
    // default when maxEdgeLength/pointSpacing are left at 0 (the default
    // here), so by default nothing gets thinned before hitting the O(n^2)
    // core. Until that's fixed, this cap is the only thing standing between
    // "completes in seconds" and "may not finish in any reasonable time" -
    // keep it conservative. 500,000 previously here took a genuinely
    // unbounded amount of time in practice.
    uint32_t maxPoints = 15000;
    bool removeDuplicates = true;
    double duplicateThreshold = 0.001;
    bool sortByZ = true;
    bool computeNormals = true;
    double normalNeighborRadius = 1.0;
    int normalNeighborCount = 10;

    // Adaptive triangulation controls (see AdaptiveTriangulator).
    bool adaptiveTriangulation = true;
    double maxEdgeLength = 0.0;   // 0 = automatic (derived from point spacing)
    double pointSpacing = 0.0;    // 0 = automatic (derived from extent / density)
    // Maximum Z difference allowed for triangle edges (meters).
    // Prevents ground-vegetation/building triangles (spikes).
    // 0 = disabled. Recommended: 1.0-2.0 for LiDAR.
    double maxElevationJump = 1.0;
};

// Phase 12 performance metrics captured during the last Generate() call
// (wall-clock, per pipeline stage). Read via GetLastStats().
struct SurfaceGenerationStats {
    size_t inputPointCount = 0;      // points extracted from the cloud
    size_t filteredPointCount = 0;   // after dedup + density downsampling
    size_t vertexCount = 0;
    size_t triangleCount = 0;
    size_t trianglesBeforeValidation = 0; // triangles after initial Delaunay
    size_t trianglesAfterEdgeFilter = 0;  // after maxEdgeLength filter
    size_t trianglesAfterZFilter = 0;     // after maxElevationJump filter
    double generationTimeMs = 0.0;   // total Generate() wall time
    double triangulationTimeMs = 0.0;
    double normalTimeMs = 0.0;
    double colorTimeMs = 0.0;
};

class SurfaceMeshGenerator {
public:
    SurfaceMesh Generate(pointcloud::PointCloud& cloud,
                         const SurfaceGenerationParams& params = {});

    void SetParams(const SurfaceGenerationParams& params) { params_ = params; }
    const SurfaceGenerationParams& GetParams() const { return params_; }

    // Metrics from the most recent Generate() call.
    const SurfaceGenerationStats& GetLastStats() const { return lastStats_; }

private:
    std::vector<math::Point3d> ExtractPoints(pointcloud::PointCloud& cloud);
    std::vector<math::Point3d> FilterPoints(const std::vector<math::Point3d>& points,
                                             const SurfaceGenerationParams& params);
    void AssignVertexColors(SurfaceMesh& mesh, pointcloud::PointCloud& cloud);

    SurfaceGenerationParams params_;
    SurfaceGenerationStats lastStats_;
    AdaptiveTriangulator triangulator_;
    NormalEstimator normalEstimator_;
};

} // namespace surface
} // namespace workstation
