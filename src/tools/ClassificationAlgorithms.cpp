#include "workstation/tools/ClassificationAlgorithms.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace workstation {
namespace tools {

namespace {

constexpr size_t kProgressChunk = 50000; // points between progress/cancel checks
constexpr double kPi = 3.14159265358979323846;

// Uniform 3-D bucket grid over a point cloud's XYZ, sized so a radius query
// only has to inspect the 3x3x3 neighbourhood of cells around the query
// point. Same approach as SurfaceMeshGenerator's ColorLookupGrid, generalized
// to arbitrary query radius (cell size == radius, so a true r-radius query
// is a 27-cell scan + an exact distance filter).
struct PointGrid {
    double cellSize = 1.0;
    double minX = 0, minY = 0, minZ = 0;
    std::unordered_map<uint64_t, std::vector<uint32_t>> cells;

    static uint64_t Key(int64_t gx, int64_t gy, int64_t gz) {
        uint64_t ux = static_cast<uint64_t>(gx);
        uint64_t uy = static_cast<uint64_t>(gy);
        uint64_t uz = static_cast<uint64_t>(gz);
        return ux * 73856093ull ^ uy * 19349663ull ^ uz * 83492791ull;
    }

    void Cell(double x, double y, double z, int64_t& gx, int64_t& gy, int64_t& gz) const {
        gx = static_cast<int64_t>(std::floor((x - minX) / cellSize));
        gy = static_cast<int64_t>(std::floor((y - minY) / cellSize));
        gz = static_cast<int64_t>(std::floor((z - minZ) / cellSize));
    }

    void Build(const std::vector<double>& xs, const std::vector<double>& ys,
               const std::vector<double>& zs, double cellSizeIn) {
        cellSize = cellSizeIn > 1e-9 ? cellSizeIn : 1.0;
        size_t n = xs.size();
        minX = std::numeric_limits<double>::max();
        minY = std::numeric_limits<double>::max();
        minZ = std::numeric_limits<double>::max();
        for (size_t i = 0; i < n; ++i) {
            minX = std::min(minX, xs[i]);
            minY = std::min(minY, ys[i]);
            minZ = std::min(minZ, zs[i]);
        }
        cells.reserve(n / 4 + 64);
        for (size_t i = 0; i < n; ++i) {
            int64_t gx, gy, gz;
            Cell(xs[i], ys[i], zs[i], gx, gy, gz);
            cells[Key(gx, gy, gz)].push_back(static_cast<uint32_t>(i));
        }
    }

    // Visits every point index in the 3x3x3 (or 3x3 if z ignored) cell
    // neighbourhood around (x,y,z). Caller applies the exact distance test.
    template <typename Fn>
    void ForEachInNeighborhood(double x, double y, double z, bool ignoreZ, Fn&& fn) const {
        int64_t gx, gy, gz;
        Cell(x, y, z, gx, gy, gz);
        int64_t zr = ignoreZ ? 0 : 1;
        for (int64_t ox = -1; ox <= 1; ++ox) {
            for (int64_t oy = -1; oy <= 1; ++oy) {
                for (int64_t oz = -zr; oz <= zr; ++oz) {
                    auto it = cells.find(Key(gx + ox, gy + oy, gz + oz));
                    if (it == cells.end()) continue;
                    for (uint32_t idx : it->second) fn(idx);
                }
            }
        }
    }
};

// Reads XYZ for every point into flat arrays, applying the same lazy
// point-index model the rest of the surface/render pipeline uses (single
// root node holds every point - see LasFileReader).
bool ExtractXYZ(pointcloud::PointCloud& cloud, std::vector<double>& xs,
                 std::vector<double>& ys, std::vector<double>& zs) {
    auto* root = cloud.Root();
    if (!root) return false;
    auto& channels = root->channels();
    size_t n = channels.PointCount();
    if (n == 0) return false;
    xs.resize(n); ys.resize(n); zs.resize(n);
    double xyz[3];
    for (size_t i = 0; i < n; ++i) {
        if (channels.ReadXYZ(i, xyz)) {
            xs[i] = xyz[0]; ys[i] = xyz[1]; zs[i] = xyz[2];
        } else {
            xs[i] = ys[i] = zs[i] = 0.0;
        }
    }
    return true;
}

bool CheckCancelled(const ClassifyProgress& progress, size_t done, size_t total) {
    if (progress.onProgress && total > 0) {
        progress.onProgress(static_cast<float>(done) / static_cast<float>(total));
    }
    return progress.shouldCancel && progress.shouldCancel();
}

// Applies {index -> newClass} pairs to the cloud, recording the prior value
// of each touched point into the result for undo.
ClassifyResult CommitClasses(pointcloud::PointCloud& cloud,
                              const std::vector<std::pair<uint32_t, uint8_t>>& edits) {
    ClassifyResult result;
    auto* root = cloud.Root();
    if (!root) return result;
    auto& channels = root->channels();
    result.changedIndices.reserve(edits.size());
    result.previousClasses.reserve(edits.size());
    for (const auto& [idx, newClass] : edits) {
        uint8_t prev = 0;
        if (!channels.ReadClassification(idx, prev)) continue;
        if (prev == newClass) continue;
        if (!channels.WriteClassification(idx, newClass)) continue;
        result.changedIndices.push_back(idx);
        result.previousClasses.push_back(prev);
    }
    return result;
}

} // namespace

ClassifyResult ClassifyIsolatedPoints(pointcloud::PointCloud& cloud,
                                       const IsolatedPointsParams& params,
                                       const ClassifyProgress& progress) {
    std::vector<double> xs, ys, zs;
    if (!ExtractXYZ(cloud, xs, ys, zs)) return {};
    size_t n = xs.size();

    PointGrid grid;
    grid.Build(xs, ys, zs, std::max(params.radius, 1e-6));

    double r2 = params.radius * params.radius;
    std::vector<std::pair<uint32_t, uint8_t>> edits;

    for (size_t i = 0; i < n; ++i) {
        if (i % kProgressChunk == 0 && CheckCancelled(progress, i, n)) {
            ClassifyResult cancelled;
            cancelled.cancelled = true;
            return cancelled;
        }
        int neighborCount = 0;
        grid.ForEachInNeighborhood(xs[i], ys[i], zs[i], false, [&](uint32_t j) {
            if (j == i) return;
            double dx = xs[j] - xs[i], dy = ys[j] - ys[i], dz = zs[j] - zs[i];
            if (dx * dx + dy * dy + dz * dz <= r2) ++neighborCount;
        });
        if (neighborCount < params.minNeighbors) {
            edits.emplace_back(static_cast<uint32_t>(i), params.targetClass);
        }
    }
    if (progress.onProgress) progress.onProgress(1.0f);
    return CommitClasses(cloud, edits);
}

ClassifyResult ClassifyLowPoints(pointcloud::PointCloud& cloud,
                                  const LowPointsParams& params,
                                  const ClassifyProgress& progress) {
    std::vector<double> xs, ys, zs;
    if (!ExtractXYZ(cloud, xs, ys, zs)) return {};
    size_t n = xs.size();

    PointGrid grid;
    grid.Build(xs, ys, zs, std::max(params.radius, 1e-6));

    double r2 = params.radius * params.radius;
    std::vector<std::pair<uint32_t, uint8_t>> edits;

    for (size_t i = 0; i < n; ++i) {
        if (i % kProgressChunk == 0 && CheckCancelled(progress, i, n)) {
            ClassifyResult cancelled;
            cancelled.cancelled = true;
            return cancelled;
        }
        double neighborMinZ = std::numeric_limits<double>::max();
        bool any = false;
        grid.ForEachInNeighborhood(xs[i], ys[i], zs[i], /*ignoreZ=*/true, [&](uint32_t j) {
            if (j == i) return;
            double dx = xs[j] - xs[i], dy = ys[j] - ys[i];
            if (dx * dx + dy * dy <= r2) {
                neighborMinZ = std::min(neighborMinZ, zs[j]);
                any = true;
            }
        });
        // Point sits more than heightThreshold below every neighbour in its
        // 2-D radius: a plausible below-ground/multipath outlier.
        if (any && (neighborMinZ - zs[i]) > params.heightThreshold) {
            edits.emplace_back(static_cast<uint32_t>(i), params.targetClass);
        }
    }
    if (progress.onProgress) progress.onProgress(1.0f);
    return CommitClasses(cloud, edits);
}

ClassifyResult ClassifyGroundPTD(pointcloud::PointCloud& cloud,
                                  const GroundPTDParams& params,
                                  const ClassifyProgress& progress) {
    std::vector<double> xs, ys, zs;
    if (!ExtractXYZ(cloud, xs, ys, zs)) return {};
    size_t n = xs.size();
    if (n == 0) return {};

    // Seed: lowest point per grid cell becomes an initial ground candidate.
    double minX = std::numeric_limits<double>::max(), minY = minX;
    for (size_t i = 0; i < n; ++i) { minX = std::min(minX, xs[i]); minY = std::min(minY, ys[i]); }
    double cell = std::max(params.gridCellSize, 1e-6);

    std::unordered_map<uint64_t, uint32_t> cellSeedIdx; // cell key -> lowest point index
    auto cellKey = [&](double x, double y) -> uint64_t {
        int64_t gx = static_cast<int64_t>(std::floor((x - minX) / cell));
        int64_t gy = static_cast<int64_t>(std::floor((y - minY) / cell));
        return (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) |
               static_cast<uint64_t>(static_cast<uint32_t>(gy));
    };
    for (size_t i = 0; i < n; ++i) {
        uint64_t k = cellKey(xs[i], ys[i]);
        auto it = cellSeedIdx.find(k);
        if (it == cellSeedIdx.end() || zs[i] < zs[it->second]) {
            cellSeedIdx[k] = static_cast<uint32_t>(i);
        }
    }

    std::vector<uint8_t> isGround(n, 0);
    std::vector<uint32_t> groundIndices;
    groundIndices.reserve(cellSeedIdx.size());
    for (auto& [k, idx] : cellSeedIdx) {
        isGround[idx] = 1;
        groundIndices.push_back(idx);
    }

    double angleTanThresh = std::tan(params.angleThresholdDeg * kPi / 180.0);

    for (int iter = 0; iter < params.maxIterations; ++iter) {
        if (CheckCancelled(progress, static_cast<size_t>(iter), static_cast<size_t>(params.maxIterations))) {
            ClassifyResult cancelled;
            cancelled.cancelled = true;
            return cancelled;
        }

        // Build a fresh grid over the current ground set for nearest-neighbour
        // lookups (cell size == gridCellSize keeps the neighbourhood scan tight).
        std::vector<double> gxs, gys, gzs;
        gxs.reserve(groundIndices.size());
        gys.reserve(groundIndices.size());
        gzs.reserve(groundIndices.size());
        for (uint32_t idx : groundIndices) {
            gxs.push_back(xs[idx]); gys.push_back(ys[idx]); gzs.push_back(zs[idx]);
        }
        PointGrid groundGrid;
        groundGrid.Build(gxs, gys, gzs, cell);

        double distThresh = params.initialDistance + iter * params.iterationDistance;
        std::vector<uint32_t> promoted;

        for (size_t i = 0; i < n; ++i) {
            if (isGround[i]) continue;
            double bestDist2 = std::numeric_limits<double>::max();
            double bestDz = 0.0;
            bool found = false;
            groundGrid.ForEachInNeighborhood(xs[i], ys[i], zs[i], true, [&](uint32_t gj) {
                double dx = gxs[gj] - xs[i], dy = gys[gj] - ys[i];
                double d2 = dx * dx + dy * dy;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    bestDz = zs[i] - gzs[gj];
                    found = true;
                }
            });
            if (!found) continue;
            double horizDist = std::sqrt(bestDist2);
            double vertDist = std::abs(bestDz);
            // Below the local ground estimate (not above it - only add points
            // that sit at/near the surface, never floating vegetation/roofs)
            // and within the iteration's distance/slope tolerance.
            if (bestDz <= 0.0 || vertDist > distThresh) continue;
            double slopeTan = horizDist > 1e-9 ? vertDist / horizDist : 0.0;
            if (slopeTan > angleTanThresh) continue;
            promoted.push_back(static_cast<uint32_t>(i));
        }

        if (promoted.empty()) break;
        for (uint32_t idx : promoted) {
            isGround[idx] = 1;
            groundIndices.push_back(idx);
        }
    }

    std::vector<std::pair<uint32_t, uint8_t>> edits;
    edits.reserve(groundIndices.size());
    for (uint32_t idx : groundIndices) {
        edits.emplace_back(idx, params.groundClass);
    }
    if (progress.onProgress) progress.onProgress(1.0f);
    return CommitClasses(cloud, edits);
}

void UndoClassifyResult(pointcloud::PointCloud& cloud, const ClassifyResult& result) {
    auto* root = cloud.Root();
    if (!root) return;
    auto& channels = root->channels();
    for (size_t i = 0; i < result.changedIndices.size(); ++i) {
        channels.WriteClassification(result.changedIndices[i], result.previousClasses[i]);
    }
}

} // namespace tools
} // namespace workstation
