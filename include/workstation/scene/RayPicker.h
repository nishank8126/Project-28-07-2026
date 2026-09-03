#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/renderer/Camera.h"
#include "workstation/spatial/BoundingBox.h"

#include <cstdint>
#include <cmath>

namespace workstation {
namespace scene {

struct Ray {
    math::Point3d origin = {0, 0, 0};
    math::Point3d direction = {0, 0, -1};

    math::Point3d GetPoint(double t) const {
        return {origin.x + direction.x * t,
                origin.y + direction.y * t,
                origin.z + direction.z * t};
    }
};

struct RayHit {
    bool hit = false;
    double distance = 0.0;
    math::Point3d position = {0, 0, 0};
    uint32_t hitIndex = 0;
};

class RayPicker {
public:
    static Ray ScreenToWorldRay(int mouseX, int mouseY,
                                 int viewportWidth, int viewportHeight,
                                 const renderer::Camera& camera);

    static RayHit IntersectAABB(const Ray& ray, const spatial::BoundingBox& box);
    static RayHit IntersectSphere(const Ray& ray, const math::Point3d& center, double radius);
    static RayHit IntersectTriangle(const Ray& ray,
                                     const math::Point3d& v0,
                                     const math::Point3d& v1,
                                     const math::Point3d& v2);

    static RayHit IntersectLineSegment(const Ray& ray,
                                        const math::Point3d& p0,
                                        const math::Point3d& p1,
                                        double tolerance = 0.05);

    static RayHit IntersectCircle(const Ray& ray,
                                   const math::Point3d& center,
                                   const math::Point3d& normal,
                                   double radius);

    static double PointToLineDistance(const math::Point3d& point,
                                      const math::Point3d& lineStart,
                                      const math::Point3d& lineEnd);

    static math::Point3d ProjectPointToRay(const math::Point3d& point, const Ray& ray);

private:
    static constexpr double EPSILON = 1e-9;
};

} // namespace scene
} // namespace workstation
