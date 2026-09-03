#include "workstation/pointcloud/NormalEstimator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_map>

namespace workstation { namespace pointcloud {

namespace {

struct CellKey {
    int32_t x, y, z;
    bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct CellKeyHash {
    size_t operator()(const CellKey& k) const {
        int64_t h = (static_cast<int64_t>(k.x) * 73856093) ^
                    (static_cast<int64_t>(k.y) * 19349663) ^
                    (static_cast<int64_t>(k.z) * 83492791);
        return std::hash<int64_t>()(h);
    }
};

// Diagonalizes a symmetric 3x3 matrix in-place via cyclic Jacobi rotations.
// Eigenvalues end up on the diagonal of `a`; the corresponding eigenvectors
// are the columns of `eigenvectors`.
void JacobiEigen3x3(double a[3][3], double eigenvectors[3][3]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            eigenvectors[i][j] = (i == j) ? 1.0 : 0.0;

    for (int sweep = 0; sweep < 30; ++sweep) {
        double off = std::fabs(a[0][1]) + std::fabs(a[0][2]) + std::fabs(a[1][2]);
        if (off < 1e-12) break;

        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(a[p][q]) < 1e-15) continue;

                double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                double t = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                double c = 1.0 / std::sqrt(t * t + 1.0);
                double s = t * c;

                double app = a[p][p], aqq = a[q][q], apq = a[p][q];
                a[p][p] = c * c * app - 2 * s * c * apq + s * s * aqq;
                a[q][q] = s * s * app + 2 * s * c * apq + c * c * aqq;
                a[p][q] = a[q][p] = 0.0;

                for (int i = 0; i < 3; ++i) {
                    if (i == p || i == q) continue;
                    double aip = a[i][p], aiq = a[i][q];
                    a[i][p] = a[p][i] = c * aip - s * aiq;
                    a[i][q] = a[q][i] = s * aip + c * aiq;
                }
                for (int i = 0; i < 3; ++i) {
                    double vip = eigenvectors[i][p], viq = eigenvectors[i][q];
                    eigenvectors[i][p] = c * vip - s * viq;
                    eigenvectors[i][q] = s * vip + c * viq;
                }
            }
        }
    }
}

} // namespace

void EstimateNormalsFromPositions(const float* positions, size_t count,
                                   std::vector<float>& outNormals) {
    outNormals.assign(count * 3, 0.0f);
    if (count == 0 || !positions) return;

    double minX = positions[0], maxX = positions[0];
    double minY = positions[1], maxY = positions[1];
    double minZ = positions[2], maxZ = positions[2];
    for (size_t i = 1; i < count; ++i) {
        double x = positions[i * 3 + 0], y = positions[i * 3 + 1], z = positions[i * 3 + 2];
        if (x < minX) minX = x; else if (x > maxX) maxX = x;
        if (y < minY) minY = y; else if (y > maxY) maxY = y;
        if (z < minZ) minZ = z; else if (z > maxZ) maxZ = z;
    }

    double dx = std::max(maxX - minX, 1e-6);
    double dy = std::max(maxY - minY, 1e-6);
    double dz = std::max(maxZ - minZ, 1e-6);
    double volume = dx * dy * dz;
    double avgSpacing = std::cbrt(volume / static_cast<double>(count));
    double cellSize = std::max(avgSpacing * 2.0, 1e-4);

    auto cellOf = [&](double x, double y, double z) {
        return CellKey{
            static_cast<int32_t>(std::floor((x - minX) / cellSize)),
            static_cast<int32_t>(std::floor((y - minY) / cellSize)),
            static_cast<int32_t>(std::floor((z - minZ) / cellSize))};
    };

    std::unordered_map<CellKey, std::vector<uint32_t>, CellKeyHash> grid;
    grid.reserve(count / 8 + 1);
    for (size_t i = 0; i < count; ++i) {
        grid[cellOf(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2])]
            .push_back(static_cast<uint32_t>(i));
    }

    constexpr int kNeighbors = 12;
    size_t fallbackCount = 0;
    double minCosUp = 1.0, maxCosUp = -1.0;

    // Parallelized like surface::NormalEstimator (which already uses OpenMP
    // for this same kind of per-point neighbor-search + PCA work): each
    // point's normal is independent of every other point's, so this scales
    // near-linearly with core count. `candidates` is declared inside the
    // loop body (not hoisted above it) specifically so each iteration - and
    // therefore each thread - gets its own vector instead of racing on a
    // shared one.
    #pragma omp parallel for schedule(dynamic, 256) \
        reduction(+:fallbackCount) reduction(min:minCosUp) reduction(max:maxCosUp)
    for (int64_t i = 0; i < static_cast<int64_t>(count); ++i) {
        std::vector<std::pair<double, uint32_t>> candidates;
        double px = positions[i * 3 + 0], py = positions[i * 3 + 1], pz = positions[i * 3 + 2];
        CellKey center = cellOf(px, py, pz);

        int radius = 1;
        for (;;) {
            candidates.clear();
            for (int ox = -radius; ox <= radius; ++ox)
                for (int oy = -radius; oy <= radius; ++oy)
                    for (int oz = -radius; oz <= radius; ++oz) {
                        auto it = grid.find(CellKey{center.x + ox, center.y + oy, center.z + oz});
                        if (it == grid.end()) continue;
                        for (uint32_t idx : it->second) {
                            if (idx == i) continue;
                            double ddx = positions[idx * 3 + 0] - px;
                            double ddy = positions[idx * 3 + 1] - py;
                            double ddz = positions[idx * 3 + 2] - pz;
                            candidates.emplace_back(ddx * ddx + ddy * ddy + ddz * ddz, idx);
                        }
                    }
            if (static_cast<int>(candidates.size()) >= kNeighbors || radius >= 6) break;
            ++radius;
        }

        if (candidates.size() < 3) {
            // Too sparse a neighborhood to fit a plane; fall back to a
            // default up-facing normal rather than leaving garbage/NaNs.
            outNormals[i * 3 + 0] = 0.0f;
            outNormals[i * 3 + 1] = 0.0f;
            outNormals[i * 3 + 2] = 1.0f;
            ++fallbackCount;
            continue;
        }

        size_t k = std::min<size_t>(kNeighbors, candidates.size());
        std::partial_sort(candidates.begin(), candidates.begin() + k, candidates.end(),
                           [](const auto& a, const auto& b) { return a.first < b.first; });

        double cx = 0, cy = 0, cz = 0;
        for (size_t n = 0; n < k; ++n) {
            uint32_t idx = candidates[n].second;
            cx += positions[idx * 3 + 0];
            cy += positions[idx * 3 + 1];
            cz += positions[idx * 3 + 2];
        }
        cx /= static_cast<double>(k);
        cy /= static_cast<double>(k);
        cz /= static_cast<double>(k);

        double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        for (size_t n = 0; n < k; ++n) {
            uint32_t idx = candidates[n].second;
            double ddx = positions[idx * 3 + 0] - cx;
            double ddy = positions[idx * 3 + 1] - cy;
            double ddz = positions[idx * 3 + 2] - cz;
            cov[0][0] += ddx * ddx; cov[0][1] += ddx * ddy; cov[0][2] += ddx * ddz;
            cov[1][1] += ddy * ddy; cov[1][2] += ddy * ddz;
            cov[2][2] += ddz * ddz;
        }
        cov[1][0] = cov[0][1]; cov[2][0] = cov[0][2]; cov[2][1] = cov[1][2];

        double eigenvectors[3][3];
        JacobiEigen3x3(cov, eigenvectors);

        int minIdx = 0;
        for (int e = 1; e < 3; ++e)
            if (cov[e][e] < cov[minIdx][minIdx]) minIdx = e;

        double nx = eigenvectors[0][minIdx];
        double ny = eigenvectors[1][minIdx];
        double nz = eigenvectors[2][minIdx];
        double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len < 1e-12) {
            nx = 0; ny = 0; nz = 1;
        } else {
            nx /= len; ny /= len; nz /= len;
        }

        // Source data is Z-up; orient normals to face +Z (the sky) by
        // convention, since there's no scanner-origin metadata here to
        // orient against more precisely.
        if (nz < 0) { nx = -nx; ny = -ny; nz = -nz; }

        if (nz < minCosUp) minCosUp = nz;
        if (nz > maxCosUp) maxCosUp = nz;

        outNormals[i * 3 + 0] = static_cast<float>(nx);
        outNormals[i * 3 + 1] = static_cast<float>(ny);
        outNormals[i * 3 + 2] = static_cast<float>(nz);
    }

    fprintf(stderr, "[EstimateNormals] cellSize=%.4f fallback=%zu/%zu (%.1f%%) "
                     "computedNormalZ range=[%.3f, %.3f]\n",
            cellSize, fallbackCount, count, 100.0 * fallbackCount / count, minCosUp, maxCosUp);
}

} // namespace pointcloud
} // namespace workstation
