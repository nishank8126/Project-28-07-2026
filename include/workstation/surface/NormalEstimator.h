#pragma once
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/math/Point3d.h"

#include <vector>
#include <cstdint>

namespace workstation {
namespace surface {

struct NormalEstimationParams {
    int neighborCount = 10;
    double neighborRadius = 1.0;
    bool useOpenMP = true;
    int threadCount = 0;
};

class NormalEstimator {
public:
    void ComputeNormals(SurfaceMesh& mesh, const NormalEstimationParams& params = {});

    void ComputeNormalsFromPoints(const std::vector<math::Point3d>& points,
                                   std::vector<float>& normals,
                                   const NormalEstimationParams& params = {});

private:
    struct Neighbor {
        uint32_t index;
        double distance;
    };

    std::vector<Neighbor> FindNeighbors(const std::vector<SurfaceVertex>& vertices,
                                         uint32_t vertexIndex,
                                         int maxNeighbors,
                                         double maxRadius);

    void ComputePlaneNormal(const math::Point3d& p0, const math::Point3d& p1,
                             const math::Point3d& p2, float outNormal[3]);

    float ComputeTriangleArea(const math::Point3d& p0, const math::Point3d& p1,
                               const math::Point3d& p2);
};

} // namespace surface
} // namespace workstation
