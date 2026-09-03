#include "workstation/surface/NormalEstimator.h"
#include <cmath>
#include <algorithm>
#include <numeric>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace workstation {
namespace surface {

static constexpr double EPSILON = 1e-12;

float NormalEstimator::ComputeTriangleArea(const math::Point3d& p0,
                                            const math::Point3d& p1,
                                            const math::Point3d& p2) {
    double ax = p1.x - p0.x, ay = p1.y - p0.y, az = p1.z - p0.z;
    double bx = p2.x - p0.x, by = p2.y - p0.y, bz = p2.z - p0.z;
    double cx = ay * bz - az * by;
    double cy = az * bx - ax * bz;
    double cz = ax * by - ay * bx;
    return static_cast<float>(std::sqrt(cx * cx + cy * cy + cz * cz) * 0.5);
}

void NormalEstimator::ComputePlaneNormal(const math::Point3d& p0,
                                          const math::Point3d& p1,
                                          const math::Point3d& p2,
                                          float outNormal[3]) {
    double ax = p1.x - p0.x, ay = p1.y - p0.y, az = p1.z - p0.z;
    double bx = p2.x - p0.x, by = p2.y - p0.y, bz = p2.z - p0.z;
    double nx = ay * bz - az * by;
    double ny = az * bx - ax * bz;
    double nz = ax * by - ay * bx;
    double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > EPSILON) {
        outNormal[0] = static_cast<float>(nx / len);
        outNormal[1] = static_cast<float>(ny / len);
        outNormal[2] = static_cast<float>(nz / len);
    } else {
        outNormal[0] = 0; outNormal[1] = 0; outNormal[2] = 1.0f;
    }
}

std::vector<NormalEstimator::Neighbor> NormalEstimator::FindNeighbors(
    const std::vector<SurfaceVertex>& vertices,
    uint32_t vertexIndex,
    int maxNeighbors,
    double maxRadius) {

    std::vector<Neighbor> neighbors;
    const auto& vp = vertices[vertexIndex].position;

    for (uint32_t i = 0; i < static_cast<uint32_t>(vertices.size()); ++i) {
        if (i == vertexIndex) continue;
        const auto& np = vertices[i].position;
        double dx = vp[0] - np[0];
        double dy = vp[1] - np[1];
        double dz = vp[2] - np[2];
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist < maxRadius) {
            neighbors.push_back({i, dist});
        }
    }

    std::sort(neighbors.begin(), neighbors.end(),
              [](const Neighbor& a, const Neighbor& b) { return a.distance < b.distance; });

    if (static_cast<int>(neighbors.size()) > maxNeighbors) {
        neighbors.resize(maxNeighbors);
    }

    return neighbors;
}

void NormalEstimator::ComputeNormals(SurfaceMesh& mesh, const NormalEstimationParams& params) {
    auto& vertices = mesh.Vertices();
    const auto& triangles = mesh.Triangles();
    if (vertices.size() < 3) return;

    const uint32_t vertexCount = static_cast<uint32_t>(vertices.size());

    // ------------------------------------------------------------------
    // Vertex -> incident-triangle adjacency (CSR layout). Built once here
    // and treated as read-only inside the parallel loop below.
    // ------------------------------------------------------------------
    std::vector<uint32_t> offsets(vertexCount + 1, 0);
    for (const auto& tri : triangles) {
        for (int k = 0; k < 3; ++k) {
            if (tri.indices[k] < vertexCount) ++offsets[tri.indices[k] + 1];
        }
    }
    for (uint32_t i = 0; i < vertexCount; ++i) offsets[i + 1] += offsets[i];
    std::vector<uint32_t> incident(offsets[vertexCount]);
    {
        std::vector<uint32_t> cursor(offsets.begin(), offsets.end() - 1);
        for (uint32_t t = 0; t < triangles.size(); ++t) {
            for (int k = 0; k < 3; ++k) {
                const uint32_t v = triangles[t].indices[k];
                if (v < vertexCount) incident[cursor[v]++] = t;
            }
        }
    }

    int threads = params.threadCount;
    if (threads <= 0) {
#ifdef _OPENMP
        threads = omp_get_max_threads();
#else
        threads = 1;
#endif
    }
#ifdef _OPENMP
    if (params.useOpenMP) {
        omp_set_num_threads(threads);
    }
#endif

    std::vector<float> normals(vertexCount * 3, 0.0f);

    // ------------------------------------------------------------------
    // Phase 4 algorithm, OpenMP parallel over vertices:
    //   1. gather the mesh faces incident to the vertex,
    //   2. accumulate their face normals (area weighted),
    //   3. normalise the sum into the vertex normal.
    //
    // The Delaunay output is consistently wound, so face normals of a
    // coherent surface reinforce instead of cancelling -- this replaces the
    // previous neighbour-ring average whose arbitrarily wound face normals
    // cancelled on gentle terrain and produced near-horizontal normals.
    // Vertices whose incident triangles were all rejected by the max-edge
    // constraint fall back to the neighbour-ring estimate below.
    // ------------------------------------------------------------------
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 64) if(params.useOpenMP)
#endif
    for (int idx = 0; idx < static_cast<int>(vertexCount); ++idx) {
        double nx = 0.0, ny = 0.0, nz = 0.0;
        bool meshed = false;

        for (uint32_t o = offsets[idx]; o < offsets[idx + 1]; ++o) {
            const SurfaceTriangle& tri = triangles[incident[o]];
            const float* A = vertices[tri.indices[0]].position;
            const float* B = vertices[tri.indices[1]].position;
            const float* C = vertices[tri.indices[2]].position;
            double ax = B[0] - A[0], ay = B[1] - A[1], az = B[2] - A[2];
            double bx = C[0] - A[0], by = C[1] - A[1], bz = C[2] - A[2];
            // Raw (unnormalised) cross product == face normal scaled by the
            // triangle area, i.e. the standard area-weighted accumulation.
            nx += ay * bz - az * by;
            ny += az * bx - ax * bz;
            nz += ax * by - ay * bx;
            meshed = true;
        }

        if (!meshed) {
            const auto neighbors = FindNeighbors(vertices, static_cast<uint32_t>(idx),
                                                 params.neighborCount, params.neighborRadius);
            double refX = 0.0, refY = 0.0, refZ = 0.0;
            bool haveRef = false;
            for (size_t i = 0; i + 1 < neighbors.size(); ++i) {
                const float* p0 = vertices[idx].position;
                const float* p1 = vertices[neighbors[i].index].position;
                const float* p2 = vertices[neighbors[i + 1].index].position;
                float fn[3];
                ComputePlaneNormal({p0[0], p0[1], p0[2]},
                                   {p1[0], p1[1], p1[2]},
                                   {p2[0], p2[1], p2[2]}, fn);
                // The neighbour ring lies on one local surface: align every
                // ring face normal with the first non-degenerate one so that
                // mixed windings cannot cancel the sum.
                if (!haveRef &&
                    (std::fabs(fn[0]) + std::fabs(fn[1]) + std::fabs(fn[2])) > 0.5f) {
                    refX = fn[0]; refY = fn[1]; refZ = fn[2];
                    haveRef = true;
                }
                if (haveRef && (fn[0] * refX + fn[1] * refY + fn[2] * refZ) < 0.0) {
                    fn[0] = -fn[0]; fn[1] = -fn[1]; fn[2] = -fn[2];
                }
                float area = ComputeTriangleArea({p0[0], p0[1], p0[2]},
                                                 {p1[0], p1[1], p1[2]},
                                                 {p2[0], p2[1], p2[2]});
                nx += fn[0] * area;
                ny += fn[1] * area;
                nz += fn[2] * area;
            }
        }

        double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > EPSILON) {
            normals[idx * 3 + 0] = static_cast<float>(nx / len);
            normals[idx * 3 + 1] = static_cast<float>(ny / len);
            normals[idx * 3 + 2] = static_cast<float>(nz / len);
        } else {
            normals[idx * 3 + 0] = 0.0f;
            normals[idx * 3 + 1] = 0.0f;
            normals[idx * 3 + 2] = 1.0f;
        }
    }

    for (uint32_t i = 0; i < vertexCount; ++i) {
        vertices[i].normal[0] = normals[i * 3 + 0];
        vertices[i].normal[1] = normals[i * 3 + 1];
        vertices[i].normal[2] = normals[i * 3 + 2];
    }
}

void NormalEstimator::ComputeNormalsFromPoints(const std::vector<math::Point3d>& points,
                                                 std::vector<float>& normals,
                                                 const NormalEstimationParams& params) {
    size_t count = points.size();
    normals.resize(count * 3, 0.0f);
    if (count < 3) return;

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 64) if(params.useOpenMP)
#endif
    for (int idx = 0; idx < static_cast<int>(count); ++idx) {
        const auto& cp = points[idx];

        std::vector<std::pair<size_t, double>> neighbors;
        for (size_t j = 0; j < count; ++j) {
            if (j == static_cast<size_t>(idx)) continue;
            double dx = cp.x - points[j].x;
            double dy = cp.y - points[j].y;
            double dz = cp.z - points[j].z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < params.neighborRadius) {
                neighbors.push_back({j, dist});
            }
        }

        std::sort(neighbors.begin(), neighbors.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        int maxN = std::min(params.neighborCount, static_cast<int>(neighbors.size()));
        if (maxN < 2) {
            normals[idx * 3 + 2] = 1.0f;
            continue;
        }

        double nx = 0, ny = 0, nz = 0;
        double refX = 0, refY = 0, refZ = 0;
        bool haveRef = false;
        for (int i = 0; i + 1 < maxN; ++i) {
            float fn[3];
            ComputePlaneNormal(cp, points[neighbors[i].first], points[neighbors[i + 1].first], fn);
            // Align every ring face normal with the first non-degenerate one:
            // the ring lies on one local surface, so mixed windings would
            // otherwise cancel and yield a near-zero (wrong) average.
            if (!haveRef &&
                (std::fabs(fn[0]) + std::fabs(fn[1]) + std::fabs(fn[2])) > 0.5f) {
                refX = fn[0]; refY = fn[1]; refZ = fn[2];
                haveRef = true;
            }
            if (haveRef && (fn[0] * refX + fn[1] * refY + fn[2] * refZ) < 0.0) {
                fn[0] = -fn[0]; fn[1] = -fn[1]; fn[2] = -fn[2];
            }
            nx += fn[0];
            ny += fn[1];
            nz += fn[2];
        }

        double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > EPSILON) {
            normals[idx * 3 + 0] = static_cast<float>(nx / len);
            normals[idx * 3 + 1] = static_cast<float>(ny / len);
            normals[idx * 3 + 2] = static_cast<float>(nz / len);
        } else {
            normals[idx * 3 + 2] = 1.0f;
        }
    }
}
} // namespace surface
} // namespace workstation