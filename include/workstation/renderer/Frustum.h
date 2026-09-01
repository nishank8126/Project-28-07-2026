#pragma once
#include "workstation/math/Matrix4d.h"
#include "workstation/spatial/BoundingBox.h"

#include <array>
#include <cmath>

namespace workstation {
namespace renderer {

struct Plane {
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;

    Plane() = default;
    Plane(double na, double nb, double nc, double nd) : a(na), b(nb), c(nc), d(nd) {}

    void Normalize() {
        double len = std::sqrt(a * a + b * b + c * c);
        if (len > 1e-12) {
            double inv = 1.0 / len;
            a *= inv;
            b *= inv;
            c *= inv;
            d *= inv;
        }
    }

    double DistanceToPoint(double x, double y, double z) const {
        return a * x + b * y + c * z + d;
    }

    double DistanceToPoint(const math::Point3d& p) const {
        return a * p.x + b * p.y + c * p.z + d;
    }
};

class Frustum {
public:
    enum PlaneID { Left = 0, Right, Top, Bottom, Near, Far, Count };

    Frustum() = default;

    void ExtractFromVP(const math::Matrix4d& vp) {
        planes_[Left].a   = vp(3, 0) + vp(0, 0);
        planes_[Left].b   = vp(3, 1) + vp(0, 1);
        planes_[Left].c   = vp(3, 2) + vp(0, 2);
        planes_[Left].d   = vp(3, 3) + vp(0, 3);

        planes_[Right].a  = vp(3, 0) - vp(0, 0);
        planes_[Right].b  = vp(3, 1) - vp(0, 1);
        planes_[Right].c  = vp(3, 2) - vp(0, 2);
        planes_[Right].d  = vp(3, 3) - vp(0, 3);

        planes_[Top].a    = vp(3, 0) - vp(1, 0);
        planes_[Top].b    = vp(3, 1) - vp(1, 1);
        planes_[Top].c    = vp(3, 2) - vp(1, 2);
        planes_[Top].d    = vp(3, 3) - vp(1, 3);

        planes_[Bottom].a = vp(3, 0) + vp(1, 0);
        planes_[Bottom].b = vp(3, 1) + vp(1, 1);
        planes_[Bottom].c = vp(3, 2) + vp(1, 2);
        planes_[Bottom].d = vp(3, 3) + vp(1, 3);

        planes_[Near].a   = vp(3, 0) + vp(2, 0);
        planes_[Near].b   = vp(3, 1) + vp(2, 1);
        planes_[Near].c   = vp(3, 2) + vp(2, 2);
        planes_[Near].d   = vp(3, 3) + vp(2, 3);

        planes_[Far].a    = vp(3, 0) - vp(2, 0);
        planes_[Far].b    = vp(3, 1) - vp(2, 1);
        planes_[Far].c    = vp(3, 2) - vp(2, 2);
        planes_[Far].d    = vp(3, 3) - vp(2, 3);

        for (auto& p : planes_) {
            p.Normalize();
        }
    }

    const Plane& GetPlane(PlaneID id) const { return planes_[id]; }
    const std::array<Plane, Count>& GetPlanes() const { return planes_; }

    bool TestAABB(const spatial::BoundingBox& box) const {
        for (const auto& plane : planes_) {
            double px = (plane.a > 0.0) ? box.maxX : box.minX;
            double py = (plane.b > 0.0) ? box.maxY : box.minY;
            double pz = (plane.c > 0.0) ? box.maxZ : box.minZ;

            if (plane.DistanceToPoint(px, py, pz) < 0.0) {
                return false;
            }
        }
        return true;
    }

    bool TestSphere(double cx, double cy, double cz, double radius) const {
        for (const auto& plane : planes_) {
            if (plane.DistanceToPoint(cx, cy, cz) < -radius) {
                return false;
            }
        }
        return true;
    }

private:
    std::array<Plane, Count> planes_;
};

} // namespace renderer
} // namespace workstation
