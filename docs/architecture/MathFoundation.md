# Math Foundation (Piece 4A)

Clean-room math for WorkstationCAD. This document strictly separates what is
**VERIFIED NOW** (traceable to recovered workstation evidence M1–M6) from what
is **RE_PENDING** (not yet reversed; must not be implemented from textbook math).

This is **Piece 4A — Verified Workstation Matrix / Affine Transform Core** only.
The broader "Piece 4 math foundation" is NOT claimed complete.

## VERIFIED NOW

### Types (M1–M6)
- `workstation::math::Point3d` — exactly `x, y, z` (double). No vector ops.
- `workstation::math::Point4d` — exactly `x, y, z, w` (double).
- `workstation::math::Matrix4d` — `double m_[4][4]`, row-major
  `| m00 m01 m02 m03 | / | m10 ... | / | m20 ... | / | m30 m31 m32 m33 |`
  (M4). Point convention is mathematical COLUMN vector: `P' = M * P` (M3).
- `workstation::math::Transform3d` — `double m_[3][4]` affine (3x4) layout (M6).
  No inverse, no composition, no vector multiply.

### Algorithms (M1–M6)
- **Point4d raw multiply (M3):** `X = row0·P`, `Y = row1·P`, `Z = row2·P`,
  `W = row3·P` (full 4x4, column-vector). Exact in-place alias supported.
- **Point3d affine multiply (M2):** implicit `w = 1`; only first three rows used;
  fourth row ignored; no W, no normalization, no validation. `count <= 0` no-op.
  Exact in-place alias supported.
- **Point3d multiply-and-renormalize (M1):** compute `X,Y,Z,W` as full 4x4 with
  implicit `w = 1`, then **exactly**:
  - `if (W != 1.0 && W != 0.0) { q = 1.0 / W; X *= q; Y *= q; Z *= q; }`
  - `W == 0` → XYZ returned as raw numerators (NO division, NO failure).
  - `W == 1` → no division.
  - NO epsilon, NO near-zero rejection, NO failure status. `count <= 0` no-op.
    Exact in-place alias supported.
- **Matrix product (M5):** `C = A * B`, `C[i][j] = Σ_k A[i][k]·B[k][j]`. Right-hand
  matrix acts first on points: `(A*B)*P = A*(B*P)`. Computed into temporary storage
  so the destination may alias either operand (`A.SetProduct(A,B)` and
  `B.SetProduct(A,B)` both valid).
- **Transform3d → Matrix4d (M6):** copies the 12 transform doubles into the top
  three rows unchanged, writes bottom row `[0,0,0,1]`. No transpose/scale/
  normalize/validate/tolerance. `t03,t13,t23` become `m03,m13,m23`.
- **Matrix4d → Transform3d (M7):** `Transform3d::TryFromMatrix4d(matrix, out)`
  returns `bool`. `w = m33`. CASE1 `w==0` → output identity affine, return false,
  no division. CASE2 `w==1` → copy upper 3x4 exactly. CASE3 otherwise → divide
  upper 3x4 by `w` exactly. Success condition (exact, strict `<`):
  `abs(m30)+abs(m31)+abs(m32) < 1.0e-12 * m33` using the ORIGINAL `m33` (not
  normalized) and the SUM of all three terms. No production tolerance helper;
  the `1.0e-12` constant is used inline exactly as recovered (DAT_54695610).
  Failure case 2 (perspective test fails) preserves the populated upper 3x4
  output; it does NOT reset to identity.
- **Affine vector multiply (M8):** `Matrix4d::MultiplyAffineVectors(dst, src, count)`
  and single-value wrapper `Point3d MultiplyAffineVectors(const Point3d&) const`.
  Per input `(x,y,z)`: `X = m00*x + m01*y + m02*z`, `Y = m10*x + m11*y + m12*z`,
  `Z = m20*x + m21*y + m22*z`. Translation slots (`m03,m13,m23`) and the entire
  fourth row (`m30..m33`) are NOT read. No translation, no W, no normalization,
  no division, no tolerance, no failure result. `count <= 0` no-op. Exact
  in-place alias (`dst == src`) supported. Point3d retained as vector carrier;
  no `Vector3d` introduced.

### Piece 3 integration
- `IViewOutput::PushTransformClip` accepts `const math::Transform3d*`.
- `RetainedSubmissionRequest::transform` is `std::optional<math::Transform3d>`.
- `TransformToken` was removed; `ClipVolumeToken` retained (clipping not reversed).

## RE_PENDING

The following are **NOT** implemented and must not be added without confirmed
reverse-engineering evidence:

- Matrix3d determinant / inverse (`Matrix3d` type removed from Piece 4A).
- Matrix4d inverse / QR inverse / `QrInverseOf`.
- Vector normalization (`Vector3d`/`Vector2d` removed from Piece 4A).
- Vector magnitude, dot, cross (not in M1–M6).
- Transform3d inverse, composition (`compose`), `rotate`, `scale`, translation
  factory, normalized basis, orientation routines.
- Range algorithms (`Range3d`, range transform) — not reversed.
- Clipping algorithms (clipping math not reversed).
- View-metric final implementation (`EstimateWorldUnitsPerViewUnit` removed).
- Generic tolerance comparison policy (`NearlyEqual` / `NearlyZero` removed).
- Identity constructor, `IsAffine`, `IsIdentity`, transpose, perspective-matrix
  creation, QR factorization.

## Evidence
See `docs/reverse_engineering/EvidenceLedger.md` entries **M1–M6** (target
function, address, inputs, outputs, exact equations, branch behavior, alias
behavior, confidence, clean mapping, derived tests).
