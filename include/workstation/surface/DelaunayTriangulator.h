#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/surface/SurfaceMesh.h"

#include <vector>
#include <cstdint>

namespace workstation {
namespace surface {

struct TriangulationSettings {
    double maxEdgeLength = 0.0;
    double pointSpacing = 0.0;
    bool adaptive = true;
    bool removeDuplicatePoints = true;
    double duplicateThreshold = 0.001;
    double boundaryMargin = 100.0;
};

class DelaunayTriangulator {
public:
    SurfaceMesh Triangulate(const std::vector<math::Point3d>& points,
                            const TriangulationSettings& settings = {});

    void SetBoundaryMargin(double margin) { settings_.boundaryMargin = margin; }
    void SetMaxEdgeLength(double length) { settings_.maxEdgeLength = length; }
    void SetSettings(const TriangulationSettings& s) { settings_ = s; }
    const TriangulationSettings& GetSettings() const { return settings_; }

private:
    struct Edge {
        uint32_t v0, v1;
        bool operator==(const Edge& o) const {
            return (v0 == o.v0 && v1 == o.v1) || (v0 == o.v1 && v1 == o.v0);
        }
    };

    struct Triangle {
        uint32_t v[3];
        double circumCenterX, circumCenterY, circumRadius;
    };

    void ComputeCircumcircle(const math::Point3d& p0, const math::Point3d& p1,
                              const math::Point3d& p2,
                              double& cx, double& cy, double& r);

    bool InCircumcircle(const math::Point3d& p, double cx, double cy, double r);

    void RemoveDuplicatePoints(std::vector<math::Point3d>& points,
                               double threshold);

    void CreateSuperTriangle(const std::vector<math::Point3d>& points,
                              math::Point3d& st0, math::Point3d& st1, math::Point3d& st2);

    bool SharesSuperTriangleVertex(const Triangle& tri,
                                    uint32_t st0, uint32_t st1, uint32_t st2);

    bool EdgeLengthExceeded(const math::Point3d& p0, const math::Point3d& p1,
                            const math::Point3d& p2, double maxEdge);

    TriangulationSettings settings_;
};

} // namespace surface
} // namespace workstation
