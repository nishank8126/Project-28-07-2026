#include "workstation/surface/DelaunayTriangulator.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

namespace workstation {
namespace surface {

static constexpr double EPSILON = 1e-10;
static constexpr double SUPER_TRIANGLE_SCALE = 1000.0;

void DelaunayTriangulator::RemoveDuplicatePoints(std::vector<math::Point3d>& points,
                                                   double threshold) {
    std::sort(points.begin(), points.end(), [](const math::Point3d& a, const math::Point3d& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });
    auto last = std::unique(points.begin(), points.end(),
        [threshold](const math::Point3d& a, const math::Point3d& b) {
            return std::abs(a.x - b.x) < threshold &&
                   std::abs(a.y - b.y) < threshold &&
                   std::abs(a.z - b.z) < threshold;
        });
    points.erase(last, points.end());
}

void DelaunayTriangulator::CreateSuperTriangle(const std::vector<math::Point3d>& points,
                                                 math::Point3d& st0, math::Point3d& st1, math::Point3d& st2) {
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

    double dx = maxX - minX;
    double dy = maxY - minY;
    double deltaMax = std::max(dx, dy);
    double midX = (minX + maxX) * 0.5;
    double midY = (minY + maxY) * 0.5;

    // Wound CCW as seen from +Z (same three geometric points as the previous
    // CW order, only the vertex order changes). Bowyer-Watson propagates the
    // super-triangle winding to every output triangle, so terrain face
    // normals end up pointing up -- what the Phong surface shader expects.
    st0 = {midX - SUPER_TRIANGLE_SCALE * deltaMax, midY - deltaMax, 0};
    st1 = {midX + SUPER_TRIANGLE_SCALE * deltaMax, midY - deltaMax, 0};
    st2 = {midX, midY + SUPER_TRIANGLE_SCALE * deltaMax, 0};
}

void DelaunayTriangulator::ComputeCircumcircle(const math::Point3d& p0,
                                                 const math::Point3d& p1,
                                                 const math::Point3d& p2,
                                                 double& cx, double& cy, double& r) {
    double ax = p1.x - p0.x, ay = p1.y - p0.y;
    double bx = p2.x - p0.x, by = p2.y - p0.y;
    double denom = 2.0 * (ax * by - ay * bx);

    if (std::abs(denom) < EPSILON) {
        cx = (p0.x + p1.x + p2.x) / 3.0;
        cy = (p0.y + p1.y + p2.y) / 3.0;
        double dx = p0.x - cx, dy = p0.y - cy;
        r = std::sqrt(dx * dx + dy * dy);
        return;
    }

    double ux = (by * (ax * ax + ay * ay) - ay * (bx * bx + by * by)) / denom;
    double uy = (ax * (bx * bx + by * by) - bx * (ax * ax + ay * ay)) / denom;

    cx = p0.x + ux;
    cy = p0.y + uy;
    r = std::sqrt(ux * ux + uy * uy);
}

bool DelaunayTriangulator::InCircumcircle(const math::Point3d& p,
                                            double cx, double cy, double r) {
    double dx = p.x - cx;
    double dy = p.y - cy;
    return (dx * dx + dy * dy) <= (r * r + EPSILON);
}

bool DelaunayTriangulator::SharesSuperTriangleVertex(const Triangle& tri,
                                                       uint32_t st0, uint32_t st1, uint32_t st2) {
    for (int i = 0; i < 3; ++i) {
        if (tri.v[i] == st0 || tri.v[i] == st1 || tri.v[i] == st2) return true;
    }
    return false;
}

bool DelaunayTriangulator::EdgeLengthExceeded(const math::Point3d& p0,
                                                const math::Point3d& p1,
                                                const math::Point3d& p2,
                                                double maxEdge) {
    auto edgeLen = [](const math::Point3d& a, const math::Point3d& b) {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        double dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    return edgeLen(p0, p1) > maxEdge || edgeLen(p1, p2) > maxEdge || edgeLen(p2, p0) > maxEdge;
}

bool DelaunayTriangulator::ElevationJumpExceeded(const math::Point3d& p0,
                                                 const math::Point3d& p1,
                                                 const math::Point3d& p2,
                                                 double maxJump) {
    if (maxJump <= 0.0) return false;
    auto zDiff = [](const math::Point3d& a, const math::Point3d& b) {
        return std::abs(a.z - b.z);
    };
    return zDiff(p0, p1) > maxJump || zDiff(p1, p2) > maxJump || zDiff(p2, p0) > maxJump;
}

SurfaceMesh DelaunayTriangulator::Triangulate(const std::vector<math::Point3d>& inputPoints,
                                                const TriangulationSettings& settings) {
    settings_ = settings;
    stats_ = {};  // Reset stats
    SurfaceMesh mesh;
    if (inputPoints.size() < 3) return mesh;

    std::vector<math::Point3d> points = inputPoints;
    if (settings_.removeDuplicatePoints) {
        RemoveDuplicatePoints(points, settings_.duplicateThreshold);
    }
    if (points.size() < 3) return mesh;

    std::sort(points.begin(), points.end(), [](const math::Point3d& a, const math::Point3d& b) {
        return a.z < b.z;
    });

    math::Point3d st0, st1, st2;
    CreateSuperTriangle(points, st0, st1, st2);

    uint32_t stIdx0 = static_cast<uint32_t>(points.size());
    uint32_t stIdx1 = static_cast<uint32_t>(points.size() + 1);
    uint32_t stIdx2 = static_cast<uint32_t>(points.size() + 2);
    points.push_back(st0);
    points.push_back(st1);
    points.push_back(st2);

    std::vector<Triangle> triangles;
    Triangle superTri;
    superTri.v[0] = stIdx0; superTri.v[1] = stIdx1; superTri.v[2] = stIdx2;
    ComputeCircumcircle(points[stIdx0], points[stIdx1], points[stIdx2],
                         superTri.circumCenterX, superTri.circumCenterY, superTri.circumRadius);
    triangles.push_back(superTri);

    for (size_t p = 0; p < points.size() - 3; ++p) {
        const auto& point = points[p];
        std::vector<Triangle> badTriangles;
        std::vector<Edge> polygon;

        for (const auto& tri : triangles) {
            if (InCircumcircle(point, tri.circumCenterX, tri.circumCenterY, tri.circumRadius)) {
                badTriangles.push_back(tri);
            }
        }

        for (size_t i = 0; i < badTriangles.size(); ++i) {
            for (int e = 0; e < 3; ++e) {
                Edge edge = {badTriangles[i].v[e], badTriangles[i].v[(e + 1) % 3]};
                bool shared = false;
                for (size_t j = 0; j < badTriangles.size(); ++j) {
                    if (i == j) continue;
                    for (int f = 0; f < 3; ++f) {
                        Edge other = {badTriangles[j].v[f], badTriangles[j].v[(f + 1) % 3]};
                        if (edge == other) { shared = true; break; }
                    }
                    if (shared) break;
                }
                if (!shared) {
                    bool found = false;
                    for (const auto& pe : polygon) {
                        if (pe == edge) { found = true; break; }
                    }
                    if (!found) polygon.push_back(edge);
                }
            }
        }

        triangles.erase(
            std::remove_if(triangles.begin(), triangles.end(), [&badTriangles](const Triangle& t) {
                for (const auto& bt : badTriangles) {
                    if (t.v[0] == bt.v[0] && t.v[1] == bt.v[1] && t.v[2] == bt.v[2]) return true;
                }
                return false;
            }),
            triangles.end());

        for (const auto& edge : polygon) {
            Triangle newTri;
            newTri.v[0] = edge.v0;
            newTri.v[1] = edge.v1;
            newTri.v[2] = static_cast<uint32_t>(p);
            ComputeCircumcircle(points[edge.v0], points[edge.v1], points[p],
                                 newTri.circumCenterX, newTri.circumCenterY, newTri.circumRadius);
            triangles.push_back(newTri);
        }
    }

    triangles.erase(
        std::remove_if(triangles.begin(), triangles.end(),
                        [this, stIdx0, stIdx1, stIdx2](const Triangle& t) {
                            return SharesSuperTriangleVertex(t, stIdx0, stIdx1, stIdx2);
                        }),
        triangles.end());
    stats_.trianglesBeforeValidation = triangles.size();

    if (settings_.maxEdgeLength > 0.0) {
        triangles.erase(
            std::remove_if(triangles.begin(), triangles.end(),
                            [this, &points](const Triangle& t) {
                                return EdgeLengthExceeded(
                                    points[t.v[0]], points[t.v[1]], points[t.v[2]],
                                    settings_.maxEdgeLength);
                            }),
            triangles.end());
    }
    stats_.trianglesAfterEdgeFilter = triangles.size();

    if (settings_.maxElevationJump > 0.0) {
        triangles.erase(
            std::remove_if(triangles.begin(), triangles.end(),
                            [this, &points](const Triangle& t) {
                                return ElevationJumpExceeded(
                                    points[t.v[0]], points[t.v[1]], points[t.v[2]],
                                    settings_.maxElevationJump);
                            }),
            triangles.end());
    }
    stats_.trianglesAfterZFilter = triangles.size();

    // Validate: reject degenerate (zero-area) triangles and out-of-bounds indices.
    uint32_t vertexCount = static_cast<uint32_t>(points.size() - 3);
    triangles.erase(
        std::remove_if(triangles.begin(), triangles.end(),
                        [this, &points, vertexCount](const Triangle& t) {
                            for (int i = 0; i < 3; ++i) {
                                if (t.v[i] >= vertexCount) return true;
                            }
                            const auto& p0 = points[t.v[0]];
                            const auto& p1 = points[t.v[1]];
                            const auto& p2 = points[t.v[2]];
                            double ex1 = p1.x - p0.x, ey1 = p1.y - p0.y;
                            double ex2 = p2.x - p0.x, ey2 = p2.y - p0.y;
                            double area = std::abs(ex1 * ey2 - ey1 * ex2);
                            return area < 1e-20;
                        }),
        triangles.end());

    mesh.Vertices().resize(points.size() - 3);
    for (size_t i = 0; i < points.size() - 3; ++i) {
        mesh.Vertices()[i].position[0] = static_cast<float>(points[i].x);
        mesh.Vertices()[i].position[1] = static_cast<float>(points[i].y);
        mesh.Vertices()[i].position[2] = static_cast<float>(points[i].z);
        mesh.Vertices()[i].normal[0] = 0;
        mesh.Vertices()[i].normal[1] = 0;
        mesh.Vertices()[i].normal[2] = 1.0f;
        mesh.Vertices()[i].color[0] = 0.7f;
        mesh.Vertices()[i].color[1] = 0.7f;
        mesh.Vertices()[i].color[2] = 0.7f;
    }

    mesh.Triangles().reserve(triangles.size());
    for (const auto& tri : triangles) {
        SurfaceTriangle st;
        st.indices[0] = tri.v[0];
        st.indices[1] = tri.v[1];
        st.indices[2] = tri.v[2];
        mesh.Triangles().push_back(st);
    }

    mesh.ComputeBounds();
    return mesh;
}

} // namespace surface
} // namespace workstation
