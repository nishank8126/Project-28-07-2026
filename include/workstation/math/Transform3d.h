#pragma once
#include "workstation/math/Matrix4d.h"
#include <cmath>

namespace workstation {
namespace math {

// VERIFIED WORKSTATION TYPE (Piece 4A / EVIDENCE M6).
// Affine transform, 3x4 arrangement of 12 doubles:
//   | t00 t01 t02 t03 |
//   | t10 t11 t12 t13 |
//   | t20 t21 t22 t23 |
// Structurally the top three rows of a Matrix4d. No inverse, no composition:
// those operations are NOT in the verified M1-M6 slice (RE_PENDING).
class Transform3d {
    double m_[3][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

public:
    Transform3d() = default;

    // Explicit construction from the 12 row values (M6 layout).
    Transform3d(double t00, double t01, double t02, double t03,
                double t10, double t11, double t12, double t13,
                double t20, double t21, double t22, double t23) {
        m_[0][0] = t00; m_[0][1] = t01; m_[0][2] = t02; m_[0][3] = t03;
        m_[1][0] = t10; m_[1][1] = t11; m_[1][2] = t12; m_[1][3] = t13;
        m_[2][0] = t20; m_[2][1] = t21; m_[2][2] = t22; m_[2][3] = t23;
    }

    double operator()(int r, int c) const { return m_[r][c]; }
    double& operator()(int r, int c) { return m_[r][c]; }

    const double* Row(int r) const { return m_[r]; }

    // EVIDENCE M6: conversion to Matrix4d copies all 12 transform doubles into
    // the top three rows unchanged, then writes the bottom row [0,0,0,1].
    // No transpose, no scaling, no normalization, no validation, no tolerance.
    Matrix4d ToMatrix4d() const {
        return Matrix4d(
            m_[0][0], m_[0][1], m_[0][2], m_[0][3],
            m_[1][0], m_[1][1], m_[1][2], m_[1][3],
            m_[2][0], m_[2][1], m_[2][2], m_[2][3],
            0.0,     0.0,     0.0,     1.0);
    }

    bool operator==(const Transform3d& o) const {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                if (m_[r][c] != o.m_[r][c]) return false;
        return true;
    }
    bool operator!=(const Transform3d& o) const { return !(*this == o); }

    // EVIDENCE M7: Matrix4d -> Transform3d conversion
    // (bsiTransform_initFromDMatrix4d / bsiTransform_initFromHMatrix, 0x45a9b0 /
    // 0x45acf0). Exact algorithm; tolerance constant 1.0e-12 recovered from
    // DAT_54695610 (0x3D719799812DEA11). No production tolerance helper is added;
    // the constant is used inline below exactly as recovered.
    static bool TryFromMatrix4d(const Matrix4d& matrix, Transform3d& out);
};

inline bool Transform3d::TryFromMatrix4d(const Matrix4d& matrix, Transform3d& out) {
    const double w = matrix(3, 3);

    if (w == 0.0) {
        // CASE 1: output becomes identity affine; return false; no division.
        out = Transform3d(1, 0, 0, 0,
                          0, 1, 0, 0,
                          0, 0, 1, 0);
        return false;
    }

    if (w == 1.0) {
        // CASE 2: copy upper 3x4 exactly.
        out = Transform3d(matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(0, 3),
                          matrix(1, 0), matrix(1, 1), matrix(1, 2), matrix(1, 3),
                          matrix(2, 0), matrix(2, 1), matrix(2, 2), matrix(2, 3));
    } else {
        // CASE 3: w != 0 and w != 1 -> exact divide of upper 3x4 by w.
        const double q = 1.0 / w;
        out = Transform3d(matrix(0, 0) * q, matrix(0, 1) * q, matrix(0, 2) * q, matrix(0, 3) * q,
                          matrix(1, 0) * q, matrix(1, 1) * q, matrix(1, 2) * q, matrix(1, 3) * q,
                          matrix(2, 0) * q, matrix(2, 1) * q, matrix(2, 2) * q, matrix(2, 3) * q);
    }

    // SUCCESS CONDITION (exact): sum(abs(m30..m32)) < 1.0e-12 * ORIGINAL m33.
    // Strict '<' (no <=), uses original m33 (not normalized), sum of all three.
    const double kTol = 1.0e-12;
    const double sum = std::abs(matrix(3, 0)) + std::abs(matrix(3, 1)) + std::abs(matrix(3, 2));
    return sum < (kTol * matrix(3, 3));
}

} // namespace math
} // namespace workstation
