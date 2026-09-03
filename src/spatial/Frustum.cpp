#include "workstation/spatial/Frustum.h"
#include <algorithm>
#include <cmath>

namespace workstation { namespace spatial {

void Frustum::BuildFromViewProjection(const double vp[16]) {
    // Extract frustum planes from the combined view-projection matrix.
    // vp is stored column-major: vp[col*4 + row].
    //
    // Left:   vp[3] + vp[0],  vp[7] + vp[4],  vp[11] + vp[8],  vp[15] + vp[12]
    // Right:  vp[3] - vp[0],  vp[7] - vp[4],  vp[11] - vp[8],  vp[15] - vp[12]
    // Bottom: vp[3] + vp[1],  vp[7] + vp[5],  vp[11] + vp[9],  vp[15] + vp[13]
    // Top:    vp[3] - vp[1],  vp[7] - vp[5],  vp[11] - vp[9],  vp[15] - vp[13]
    // Near:   vp[3] + vp[2],  vp[7] + vp[6],  vp[11] + vp[10], vp[15] + vp[14]
    // Far:    vp[3] - vp[2],  vp[7] - vp[6],  vp[11] - vp[10], vp[15] - vp[14]

    auto extract = [&](int row, int sign) {
        return sign;
    };

    // Left
    planes[0][0] = vp[3]  + vp[0];
    planes[0][1] = vp[7]  + vp[4];
    planes[0][2] = vp[11] + vp[8];
    planes[0][3] = vp[15] + vp[12];
    // Right
    planes[1][0] = vp[3]  - vp[0];
    planes[1][1] = vp[7]  - vp[4];
    planes[1][2] = vp[11] - vp[8];
    planes[1][3] = vp[15] - vp[12];
    // Bottom
    planes[2][0] = vp[3]  + vp[1];
    planes[2][1] = vp[7]  + vp[5];
    planes[2][2] = vp[11] + vp[9];
    planes[2][3] = vp[15] + vp[13];
    // Top
    planes[3][0] = vp[3]  - vp[1];
    planes[3][1] = vp[7]  - vp[5];
    planes[3][2] = vp[11] - vp[9];
    planes[3][3] = vp[15] - vp[13];
    // Near
    planes[4][0] = vp[3]  + vp[2];
    planes[4][1] = vp[7]  + vp[6];
    planes[4][2] = vp[11] + vp[10];
    planes[4][3] = vp[15] + vp[14];
    // Far
    planes[5][0] = vp[3]  - vp[2];
    planes[5][1] = vp[7]  - vp[6];
    planes[5][2] = vp[11] - vp[10];
    planes[5][3] = vp[15] - vp[14];

    // Normalize each plane.
    for (auto& p : planes) {
        double len = std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
        if (len > 0.0) {
            double invLen = 1.0 / len;
            p[0] *= invLen;
            p[1] *= invLen;
            p[2] *= invLen;
            p[3] *= invLen;
        }
    }
}

bool Frustum::IntersectsBoundingBox(const BoundingBox& box) const {
    for (const auto& p : planes) {
        // Compute the p-vertex (the vertex of the box most in the direction
        // of the plane normal).
        double px = (p[0] >= 0.0) ? box.maxX : box.minX;
        double py = (p[1] >= 0.0) ? box.maxY : box.minY;
        double pz = (p[2] >= 0.0) ? box.maxZ : box.minZ;

        // If the p-vertex is outside the plane, the box is entirely outside.
        if (p[0]*px + p[1]*py + p[2]*pz + p[3] < 0.0) {
            return false;
        }
    }
    return true;
}

bool Frustum::ContainsPoint(double x, double y, double z) const {
    for (const auto& p : planes) {
        if (p[0]*x + p[1]*y + p[2]*z + p[3] < 0.0) {
            return false;
        }
    }
    return true;
}

} // namespace spatial
} // namespace workstation
