#include "workstation/scene/RayPicker.h"
#include <algorithm>
#include <limits>

namespace workstation {
namespace scene {

Ray RayPicker::ScreenToWorldRay(int mouseX, int mouseY,
                                  int viewportWidth, int viewportHeight,
                                  const renderer::Camera& camera) {
    double ndcX = (2.0 * mouseX / viewportWidth) - 1.0;
    double ndcY = 1.0 - (2.0 * mouseY / viewportHeight);

    auto& cam = const_cast<renderer::Camera&>(camera);
    auto invProj = cam.GetProjectionMatrix();
    auto invView = cam.GetViewMatrix();

    math::Point4d clipCoords(ndcX, ndcY, -1.0, 1.0);

    math::Point4d eyeCoords;
    double invM[4][4];
    double pm[4][4];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            pm[r][c] = invProj(r, c);

    double det = pm[0][0] * (pm[1][1] * (pm[2][2] * pm[3][3] - pm[2][3] * pm[3][2])
                            - pm[1][2] * (pm[2][1] * pm[3][3] - pm[2][3] * pm[3][1])
                            + pm[1][3] * (pm[2][1] * pm[3][2] - pm[2][2] * pm[3][1]))
               - pm[0][1] * (pm[1][0] * (pm[2][2] * pm[3][3] - pm[2][3] * pm[3][2])
                            - pm[1][2] * (pm[2][0] * pm[3][3] - pm[2][3] * pm[3][0])
                            + pm[1][3] * (pm[2][0] * pm[3][2] - pm[2][2] * pm[3][0]))
               + pm[0][2] * (pm[1][0] * (pm[2][1] * pm[3][3] - pm[2][3] * pm[3][1])
                            - pm[1][1] * (pm[2][0] * pm[3][3] - pm[2][3] * pm[3][0])
                            + pm[1][3] * (pm[2][0] * pm[3][1] - pm[2][1] * pm[3][0]))
               - pm[0][3] * (pm[1][0] * (pm[2][1] * pm[3][2] - pm[2][2] * pm[3][1])
                            - pm[1][1] * (pm[2][0] * pm[3][2] - pm[2][2] * pm[3][0])
                            + pm[1][2] * (pm[2][0] * pm[3][1] - pm[2][1] * pm[3][0]));

    if (std::abs(det) < EPSILON) {
        Ray r;
        r.origin = camera.GetPosition();
        r.direction = camera.GetForward();
        return r;
    }

    invM[0][0] =  (pm[1][1]*(pm[2][2]*pm[3][3] - pm[2][3]*pm[3][2])
                  - pm[1][2]*(pm[2][1]*pm[3][3] - pm[2][3]*pm[3][1])
                  + pm[1][3]*(pm[2][1]*pm[3][2] - pm[2][2]*pm[3][1])) / det;
    invM[0][1] = -(pm[0][1]*(pm[2][2]*pm[3][3] - pm[2][3]*pm[3][2])
                  - pm[0][2]*(pm[2][1]*pm[3][3] - pm[2][3]*pm[3][1])
                  + pm[0][3]*(pm[2][1]*pm[3][2] - pm[2][2]*pm[3][1])) / det;
    invM[0][2] =  (pm[0][1]*(pm[1][2]*pm[3][3] - pm[1][3]*pm[3][2])
                  - pm[0][2]*(pm[1][1]*pm[3][3] - pm[1][3]*pm[3][1])
                  + pm[0][3]*(pm[1][1]*pm[3][2] - pm[1][2]*pm[3][1])) / det;
    invM[0][3] = -(pm[0][1]*(pm[1][2]*pm[2][3] - pm[1][3]*pm[2][2])
                  - pm[0][2]*(pm[1][1]*pm[2][3] - pm[1][3]*pm[2][1])
                  + pm[0][3]*(pm[1][1]*pm[2][2] - pm[1][2]*pm[2][1])) / det;

    invM[1][0] = -(pm[1][0]*(pm[2][2]*pm[3][3] - pm[2][3]*pm[3][2])
                  - pm[1][2]*(pm[2][0]*pm[3][3] - pm[2][3]*pm[3][0])
                  + pm[1][3]*(pm[2][0]*pm[3][2] - pm[2][2]*pm[3][0])) / det;
    invM[1][1] =  (pm[0][0]*(pm[2][2]*pm[3][3] - pm[2][3]*pm[3][2])
                  - pm[0][2]*(pm[2][0]*pm[3][3] - pm[2][3]*pm[3][0])
                  + pm[0][3]*(pm[2][0]*pm[3][2] - pm[2][2]*pm[3][0])) / det;
    invM[1][2] = -(pm[0][0]*(pm[1][2]*pm[3][3] - pm[1][3]*pm[3][2])
                  - pm[0][2]*(pm[1][0]*pm[3][3] - pm[1][3]*pm[3][0])
                  + pm[0][3]*(pm[1][0]*pm[3][2] - pm[1][2]*pm[3][0])) / det;
    invM[1][3] =  (pm[0][0]*(pm[1][2]*pm[2][3] - pm[1][3]*pm[2][2])
                  - pm[0][2]*(pm[1][0]*pm[2][3] - pm[1][3]*pm[2][0])
                  + pm[0][3]*(pm[1][0]*pm[2][2] - pm[1][2]*pm[2][0])) / det;

    invM[2][0] =  (pm[1][0]*(pm[2][1]*pm[3][3] - pm[2][3]*pm[3][1])
                  - pm[1][1]*(pm[2][0]*pm[3][3] - pm[2][3]*pm[3][0])
                  + pm[1][3]*(pm[2][0]*pm[3][1] - pm[2][1]*pm[3][0])) / det;
    invM[2][1] = -(pm[0][0]*(pm[2][1]*pm[3][3] - pm[2][3]*pm[3][1])
                  - pm[0][1]*(pm[2][0]*pm[3][3] - pm[2][3]*pm[3][0])
                  + pm[0][3]*(pm[2][0]*pm[3][1] - pm[2][1]*pm[3][0])) / det;
    invM[2][2] =  (pm[0][0]*(pm[1][1]*pm[3][3] - pm[1][3]*pm[3][1])
                  - pm[0][1]*(pm[1][0]*pm[3][3] - pm[1][3]*pm[3][0])
                  + pm[0][3]*(pm[1][0]*pm[3][1] - pm[1][1]*pm[3][0])) / det;
    invM[2][3] = -(pm[0][0]*(pm[1][1]*pm[2][3] - pm[1][3]*pm[2][1])
                  - pm[0][1]*(pm[1][0]*pm[2][3] - pm[1][3]*pm[2][0])
                  + pm[0][3]*(pm[1][0]*pm[2][1] - pm[1][1]*pm[2][0])) / det;

    invM[3][0] = -(pm[1][0]*(pm[2][1]*pm[3][2] - pm[2][2]*pm[3][1])
                  - pm[1][1]*(pm[2][0]*pm[3][2] - pm[2][2]*pm[3][0])
                  + pm[1][2]*(pm[2][0]*pm[3][1] - pm[2][1]*pm[3][0])) / det;
    invM[3][1] =  (pm[0][0]*(pm[2][1]*pm[3][2] - pm[2][2]*pm[3][1])
                  - pm[0][1]*(pm[2][0]*pm[3][2] - pm[2][2]*pm[3][0])
                  + pm[0][2]*(pm[2][0]*pm[3][1] - pm[2][1]*pm[3][0])) / det;
    invM[3][2] = -(pm[0][0]*(pm[1][1]*pm[3][2] - pm[1][2]*pm[3][1])
                  - pm[0][1]*(pm[1][0]*pm[3][2] - pm[1][2]*pm[3][0])
                  + pm[0][2]*(pm[1][0]*pm[3][1] - pm[1][1]*pm[3][0])) / det;
    invM[3][3] =  (pm[0][0]*(pm[1][1]*pm[2][2] - pm[1][2]*pm[2][1])
                  - pm[0][1]*(pm[1][0]*pm[2][2] - pm[1][2]*pm[2][0])
                  + pm[0][2]*(pm[1][0]*pm[2][1] - pm[1][1]*pm[2][0])) / det;

    double ex = invM[0][0]*clipCoords.x + invM[0][1]*clipCoords.y
              + invM[0][2]*clipCoords.z + invM[0][3]*clipCoords.w;
    double ey = invM[1][0]*clipCoords.x + invM[1][1]*clipCoords.y
              + invM[1][2]*clipCoords.z + invM[1][3]*clipCoords.w;
    double ez = invM[2][0]*clipCoords.x + invM[2][1]*clipCoords.y
              + invM[2][2]*clipCoords.z + invM[2][3]*clipCoords.w;

    math::Point3d eyePoint(ex, ey, ez);

    double vx = invView(0,0)*eyePoint.x + invView(0,1)*eyePoint.y
              + invView(0,2)*eyePoint.z + invView(0,3);
    double vy = invView(1,0)*eyePoint.x + invView(1,1)*eyePoint.y
              + invView(1,2)*eyePoint.z + invView(1,3);
    double vz = invView(2,0)*eyePoint.x + invView(2,1)*eyePoint.y
              + invView(2,2)*eyePoint.z + invView(2,3);

    math::Point3d worldNear(vx, vy, vz);

    double fx = invM[0][0]*clipCoords.x + invM[0][1]*clipCoords.y
              + invM[0][2]*(-1.0) + invM[0][3];
    double fy = invM[1][0]*clipCoords.x + invM[1][1]*clipCoords.y
              + invM[1][2]*(-1.0) + invM[1][3];
    double fz = invM[2][0]*clipCoords.x + invM[2][1]*clipCoords.y
              + invM[2][2]*(-1.0) + invM[2][3];

    math::Point3d eyeFar(fx, fy, fz);

    double wx = invView(0,0)*eyeFar.x + invView(0,1)*eyeFar.y
              + invView(0,2)*eyeFar.z + invView(0,3);
    double wy = invView(1,0)*eyeFar.x + invView(1,1)*eyeFar.y
              + invView(1,2)*eyeFar.z + invView(1,3);
    double wz = invView(2,0)*eyeFar.x + invView(2,1)*eyeFar.y
              + invView(2,2)*eyeFar.z + invView(2,3);

    math::Point3d worldFar(wx, wy, wz);

    Ray ray;
    ray.origin = worldNear;
    ray.direction = {worldFar.x - worldNear.x,
                     worldFar.y - worldNear.y,
                     worldFar.z - worldNear.z};
    double len = std::sqrt(ray.direction.x * ray.direction.x +
                           ray.direction.y * ray.direction.y +
                           ray.direction.z * ray.direction.z);
    if (len > EPSILON) {
        ray.direction.x /= len;
        ray.direction.y /= len;
        ray.direction.z /= len;
    }
    return ray;
}

RayHit RayPicker::IntersectAABB(const Ray& ray, const spatial::BoundingBox& box) {
    RayHit hit;
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax = std::numeric_limits<double>::infinity();

    auto testAxis = [&](double origin, double dir, double bmin, double bmax) {
        if (std::abs(dir) < EPSILON) {
            if (origin < bmin || origin > bmax) return false;
        } else {
            double t1 = (bmin - origin) / dir;
            double t2 = (bmax - origin) / dir;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
        return true;
    };

    if (!testAxis(ray.origin.x, ray.direction.x, box.minX, box.maxX)) return hit;
    if (!testAxis(ray.origin.y, ray.direction.y, box.minY, box.maxY)) return hit;
    if (!testAxis(ray.origin.z, ray.direction.z, box.minZ, box.maxZ)) return hit;

    if (tmax < 0) return hit;

    hit.hit = true;
    hit.distance = tmin >= 0 ? tmin : tmax;
    hit.position = ray.GetPoint(hit.distance);
    return hit;
}

RayHit RayPicker::IntersectSphere(const Ray& ray, const math::Point3d& center, double radius) {
    RayHit hit;
    double dx = ray.origin.x - center.x;
    double dy = ray.origin.y - center.y;
    double dz = ray.origin.z - center.z;

    double a = ray.direction.x * ray.direction.x +
              ray.direction.y * ray.direction.y +
              ray.direction.z * ray.direction.z;
    double b = 2.0 * (dx * ray.direction.x + dy * ray.direction.y + dz * ray.direction.z);
    double c = dx * dx + dy * dy + dz * dz - radius * radius;

    double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0) return hit;

    double sqrtD = std::sqrt(discriminant);
    double t1 = (-b - sqrtD) / (2.0 * a);
    double t2 = (-b + sqrtD) / (2.0 * a);

    double t = (t1 >= 0) ? t1 : t2;
    if (t < 0) return hit;

    hit.hit = true;
    hit.distance = t;
    hit.position = ray.GetPoint(t);
    return hit;
}

RayHit RayPicker::IntersectTriangle(const Ray& ray,
                                      const math::Point3d& v0,
                                      const math::Point3d& v1,
                                      const math::Point3d& v2) {
    RayHit hit;
    math::Point3d edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    math::Point3d edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};

    math::Point3d h = {ray.direction.y * edge2.z - ray.direction.z * edge2.y,
                       ray.direction.z * edge2.x - ray.direction.x * edge2.z,
                       ray.direction.x * edge2.y - ray.direction.y * edge2.x};

    double a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;
    if (std::abs(a) < EPSILON) return hit;

    double f = 1.0 / a;
    math::Point3d s = {ray.origin.x - v0.x, ray.origin.y - v0.y, ray.origin.z - v0.z};
    double u = f * (s.x * h.x + s.y * h.y + s.z * h.z);
    if (u < 0.0 || u > 1.0) return hit;

    math::Point3d q = {s.y * edge1.z - s.z * edge1.y,
                       s.z * edge1.x - s.x * edge1.z,
                       s.x * edge1.y - s.y * edge1.x};
    double v = f * (ray.direction.x * q.x + ray.direction.y * q.y + ray.direction.z * q.z);
    if (v < 0.0 || u + v > 1.0) return hit;

    double t = f * (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z);
    if (t > EPSILON) {
        hit.hit = true;
        hit.distance = t;
        hit.position = ray.GetPoint(t);
    }
    return hit;
}

RayHit RayPicker::IntersectLineSegment(const Ray& ray,
                                         const math::Point3d& p0,
                                         const math::Point3d& p1,
                                         double tolerance) {
    RayHit hit;
    math::Point3d u = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
    math::Point3d v = ray.direction;

    double dotUU = u.x * u.x + u.y * u.y + u.z * u.z;
    double dotVV = v.x * v.x + v.y * v.y + v.z * v.z;
    double dotUV = u.x * v.x + u.y * v.y + u.z * v.z;

    math::Point3d w0 = {ray.origin.x - p0.x, ray.origin.y - p0.y, ray.origin.z - p0.z};
    double dotW0U = w0.x * u.x + w0.y * u.y + w0.z * u.z;
    double dotW0V = w0.x * v.x + w0.y * v.y + w0.z * v.z;

    double denom = dotUU * dotVV - dotUV * dotUV;
    double sN, tN;

    if (std::abs(denom) < EPSILON) {
        sN = 0.0;
        tN = dotW0V;
    } else {
        sN = (dotUV * dotW0V - dotVV * dotW0U);
        tN = (dotUU * dotW0V - dotUV * dotW0U);
    }

    double s = (std::abs(denom) < EPSILON) ? 0.0 : sN / denom;
    double t = (std::abs(denom) < EPSILON) ? 0.0 : tN / dotVV;

    s = std::max(0.0, std::min(1.0, s));
    t = std::max(0.0, t);

    math::Point3d closest = {p0.x + s * u.x, p0.y + s * u.y, p0.z + s * u.z};
    math::Point3d onRay = ray.GetPoint(t);

    double dx = closest.x - onRay.x;
    double dy = closest.y - onRay.y;
    double dz = closest.z - onRay.z;
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist <= tolerance) {
        hit.hit = true;
        hit.distance = t;
        hit.position = closest;
    }
    return hit;
}

RayHit RayPicker::IntersectCircle(const Ray& ray,
                                    const math::Point3d& center,
                                    const math::Point3d& normal,
                                    double radius) {
    RayHit hit;
    double denom = normal.x * ray.direction.x +
                   normal.y * ray.direction.y +
                   normal.z * ray.direction.z;
    if (std::abs(denom) < EPSILON) return hit;

    math::Point3d oc = {ray.origin.x - center.x,
                        ray.origin.y - center.y,
                        ray.origin.z - center.z};
    double t = -(normal.x * oc.x + normal.y * oc.y + normal.z * oc.z) / denom;
    if (t < 0) return hit;

    math::Point3d p = ray.GetPoint(t);
    double dx = p.x - center.x;
    double dy = p.y - center.y;
    double dz = p.z - center.z;
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist <= radius) {
        hit.hit = true;
        hit.distance = t;
        hit.position = p;
    }
    return hit;
}

double RayPicker::PointToLineDistance(const math::Point3d& point,
                                       const math::Point3d& lineStart,
                                       const math::Point3d& lineEnd) {
    math::Point3d ab = {lineEnd.x - lineStart.x, lineEnd.y - lineStart.y, lineEnd.z - lineStart.z};
    math::Point3d ap = {point.x - lineStart.x, point.y - lineStart.y, point.z - lineStart.z};

    double abLen2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (abLen2 < EPSILON) {
        double dx = point.x - lineStart.x;
        double dy = point.y - lineStart.y;
        double dz = point.z - lineStart.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    double t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / abLen2;
    t = std::max(0.0, std::min(1.0, t));

    math::Point3d closest = {lineStart.x + t * ab.x,
                             lineStart.y + t * ab.y,
                             lineStart.z + t * ab.z};
    double dx = point.x - closest.x;
    double dy = point.y - closest.y;
    double dz = point.z - closest.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

math::Point3d RayPicker::ProjectPointToRay(const math::Point3d& point, const Ray& ray) {
    math::Point3d op = {point.x - ray.origin.x, point.y - ray.origin.y, point.z - ray.origin.z};
    double t = op.x * ray.direction.x + op.y * ray.direction.y + op.z * ray.direction.z;
    return ray.GetPoint(t);
}

} // namespace scene
} // namespace workstation
