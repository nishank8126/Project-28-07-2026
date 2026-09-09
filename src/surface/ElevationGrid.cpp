#include "workstation/surface/ElevationGrid.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/surface/SurfaceLog.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <queue>

namespace workstation {
namespace surface {

static double ElapsedMs(auto start, auto end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ---------------------------------------------------------------------------
// Streaming spatial binning — O(n) single pass, zero extra allocation.
// Points are read directly from the cloud's channels and binned into grid
// cells on the fly. No intermediate vector<Point3d> is created, so even
// 70M points use only a few KB of working memory.
// ---------------------------------------------------------------------------
void ElevationGrid::Generate(const pointcloud::PointCloud& cloud,
                              const ElevationGridParams& params) {
    if (!cloud.Root()) return;

    // Guard against degenerate resolution
    if (params.resolution < 2) {
        SLOG_WARN("ElevationGrid: resolution %u too small, clamping to 2",
                  params.resolution);
    }

    const auto& channels = cloud.Root()->channels();

    auto* xyzChannel = channels.GetChannel(pointcloud::ChannelId::XYZ);
    if (!xyzChannel || xyzChannel->Count() == 0) return;

    auto bounds = cloud.Root()->bounds();

    // Guard against degenerate resolution
    uint32_t res = std::max(params.resolution, 2u);

    // --- Grid setup (same as the vector overload) ---
    auto t0 = std::chrono::steady_clock::now();
    stats_ = {};

    width_ = res;
    height_ = res;
    originX_ = static_cast<float>(bounds.minX);
    originY_ = static_cast<float>(bounds.minY);
    double dx = bounds.maxX - bounds.minX;
    double dy = bounds.maxY - bounds.minY;
    cellSize_ = static_cast<float>(std::max(dx, dy) / width_);

    cells_.assign(width_ * height_, {});

    // Classification channel for filtering
    bool hasClass = channels.GetChannel(pointcloud::ChannelId::Classification) != nullptr;

    // --- Streaming binning: read each point and bin immediately ---
    double xyz[3];
    const size_t totalPoints = xyzChannel->Count();

    // Collect raw LAS ground point statistics for comparison
    double lasMinZ = std::numeric_limits<double>::max();
    double lasMaxZ = std::numeric_limits<double>::lowest();
    double lasSumZ = 0.0;
    double lasSumSqZ = 0.0;
    size_t lasGroundCount = 0;

    for (size_t i = 0; i < totalPoints; ++i) {
        if (!channels.ReadXYZ(i, xyz)) continue;

        // Source mode filtering (read classification on demand, 1 byte)
        if (params.sourceMode != ElevationSourceMode::AllPoints) {
            if (!hasClass) continue;
            uint8_t cls = 0;
            channels.ReadClassification(i, cls);

            switch (params.sourceMode) {
                case ElevationSourceMode::GroundOnly:
                    if (cls != 2) continue;
                    break;
                case ElevationSourceMode::HighestReturn:
                    // Include all points; highest Z per cell is tracked
                    break;
                case ElevationSourceMode::UserClass: {
                    bool found = false;
                    for (uint8_t c : params.selectedClasses) {
                        if (cls == c) { found = true; break; }
                    }
                    if (!found) continue;
                    break;
                }
                default:
                    break;
            }
        }

        // Track LAS ground point statistics
        lasSumZ += xyz[2];
        lasSumSqZ += xyz[2] * xyz[2];
        lasGroundCount++;
        if (xyz[2] < lasMinZ) lasMinZ = xyz[2];
        if (xyz[2] > lasMaxZ) lasMaxZ = xyz[2];

        // Bin into grid cell
        int32_t gx = static_cast<int32_t>((xyz[0] - bounds.minX) / cellSize_);
        int32_t gy = static_cast<int32_t>((xyz[1] - bounds.minY) / cellSize_);
        if (gx < 0 || gx >= static_cast<int32_t>(width_)) continue;
        if (gy < 0 || gy >= static_cast<int32_t>(height_)) continue;

        auto& cell = cells_[gy * width_ + gx];
        float z = static_cast<float>(xyz[2]);
        cell.maxZ = std::max(cell.maxZ, z);
        cell.minZ = std::min(cell.minZ, z);
        cell.sumZ += z;
        cell.count++;
        cell.valid = true;

        // Track classification histogram for dominant class
        if (hasClass) {
            uint8_t cls = 0;
            channels.ReadClassification(i, cls);
            cell.classCounts[cls]++;
            // Update dominant class
            if (cell.classCounts[cls] > cell.classCounts[cell.dominantClass]) {
                cell.dominantClass = cls;
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();

    // --- Interpolate empty cells (BFS flood-fill) ---
    InterpolateEmptyCells();
    auto t2 = std::chrono::steady_clock::now();

    // --- Compute terrain normals from neighboring cell elevations ---
    ComputeNormals();

    // --- Compute elevation range from REAL data cells only ---
    minElevation_ = std::numeric_limits<float>::max();
    maxElevation_ = std::numeric_limits<float>::lowest();
    uint32_t occupied = 0;
    for (const auto& cell : cells_) {
        if (cell.valid) {
            float avgZ = cell.AverageElevation();
            minElevation_ = std::min(minElevation_, avgZ);
            maxElevation_ = std::max(maxElevation_, avgZ);
            occupied++;
        }
    }
    if (occupied == 0) {
        minElevation_ = 0.0f;
        maxElevation_ = 0.0f;
    }

    stats_.gridWidth = width_;
    stats_.gridHeight = height_;
    stats_.totalCells = width_ * height_;
    stats_.occupiedCells = occupied;
    stats_.minElevation = minElevation_;
    stats_.maxElevation = maxElevation_;
    stats_.binningTimeMs = ElapsedMs(t0, t1);
    stats_.interpolateTimeMs = ElapsedMs(t1, t2);
    stats_.totalTimeMs = ElapsedMs(t0, t2);

    // [ELEVATION GRID QUALITY] - terrain variance analysis
    {
        double sumZ = 0.0, sumSq = 0.0;
        uint32_t count = 0;
        float maxSlope = 0.0f;
        float meanSlope = 0.0f;
        uint32_t slopeCount = 0;
        double sumRoughness = 0.0;
        uint32_t roughnessCount = 0;
        for (uint32_t y = 0; y < height_; ++y) {
            for (uint32_t x = 0; x < width_; ++x) {
                const auto& cell = cells_[y * width_ + x];
                if (!cell.valid) continue;
                float z = cell.AverageElevation();
                sumZ += z;
                sumSq += z * z;
                count++;
                // Slope from normal
                float nz = cell.normalZ;
                if (nz > 0.01f) {
                    float slopeDeg = std::acos(std::min(nz, 1.0f)) * 180.0f / 3.14159f;
                    meanSlope += slopeDeg;
                    if (slopeDeg > maxSlope) maxSlope = slopeDeg;
                    slopeCount++;
                }
                // Terrain roughness: mean abs Z difference from 4-connected neighbors
                float absDiffSum = 0.0;
                uint32_t neighbors = 0;
                if (x > 0 && cells_[y * width_ + (x-1)].valid) {
                    absDiffSum += std::abs(z - cells_[y * width_ + (x-1)].AverageElevation());
                    neighbors++;
                }
                if (x + 1 < width_ && cells_[y * width_ + (x+1)].valid) {
                    absDiffSum += std::abs(z - cells_[y * width_ + (x+1)].AverageElevation());
                    neighbors++;
                }
                if (y > 0 && cells_[(y-1) * width_ + x].valid) {
                    absDiffSum += std::abs(z - cells_[(y-1) * width_ + x].AverageElevation());
                    neighbors++;
                }
                if (y + 1 < height_ && cells_[(y+1) * width_ + x].valid) {
                    absDiffSum += std::abs(z - cells_[(y+1) * width_ + x].AverageElevation());
                    neighbors++;
                }
                if (neighbors > 0) {
                    sumRoughness += absDiffSum / neighbors;
                    roughnessCount++;
                }
            }
        }
        double meanZ = (count > 0) ? sumZ / count : 0.0;
        double stddevZ = (count > 0) ? std::sqrt(sumSq / count - meanZ * meanZ) : 0.0;
        meanSlope = (slopeCount > 0) ? meanSlope / slopeCount : 0.0f;
        float roughness = (roughnessCount > 0) ? static_cast<float>(sumRoughness / roughnessCount) : 0.0f;

        stats_.meanSlopeDeg = meanSlope;
        stats_.maxSlopeDeg = maxSlope;
        stats_.zStddev = static_cast<float>(stddevZ);
        stats_.terrainRoughness = roughness;

        // LAS ground point statistics for comparison
        double lasMeanZ = (lasGroundCount > 0) ? lasSumZ / lasGroundCount : 0.0;
        double lasStddevZ = (lasGroundCount > 0) ? std::sqrt(lasSumSqZ / lasGroundCount - lasMeanZ * lasMeanZ) : 0.0;

        fprintf(stderr,
            "[SURFACE QUALITY]\n"
            "  LAS ground points: %zu\n"
            "  LAS Z range: [%.3f, %.3f] = %.3f\n"
            "  LAS Z mean: %.3f, stddev: %.3f\n"
            "  Grid: %ux%u, cellSize=%.3f\n"
            "  Occupied: %u/%u (%.0f%%)\n"
            "  Grid Z range: [%.3f, %.3f] = %.3f\n"
            "  Grid Z mean: %.3f, stddev: %.3f\n"
            "  Z preservation: %.1f%% range, %.1f%% stddev\n"
            "  Mean slope: %.1f deg, max slope: %.1f deg\n"
            "  Terrain roughness: %.3f m\n"
            "  Timing: bin=%.1fms interp=%.1fms total=%.1fms\n",
            lasGroundCount,
            lasMinZ, lasMaxZ, lasMaxZ - lasMinZ,
            lasMeanZ, lasStddevZ,
            width_, height_, cellSize_,
            occupied, width_ * height_,
            100.0 * occupied / (width_ * height_),
            minElevation_, maxElevation_, maxElevation_ - minElevation_,
            meanZ, stddevZ,
            (lasMaxZ - lasMinZ > 0) ? 100.0 * (maxElevation_ - minElevation_) / (lasMaxZ - lasMinZ) : 0.0,
            (lasStddevZ > 0) ? 100.0 * stddevZ / lasStddevZ : 0.0,
            meanSlope, maxSlope,
            roughness,
            stats_.binningTimeMs, stats_.interpolateTimeMs, stats_.totalTimeMs);
        fflush(stderr);
    }
}

// ---------------------------------------------------------------------------
// Overload from pre-extracted points (used by LOD pipeline).
// ---------------------------------------------------------------------------
void ElevationGrid::Generate(const std::vector<math::Point3d>& points,
                              const spatial::BoundingBox& bounds,
                              const ElevationGridParams& params) {
    auto t0 = std::chrono::steady_clock::now();
    stats_ = {};

    uint32_t res = std::max(params.resolution, 2u);
    width_ = res;
    height_ = res;
    originX_ = static_cast<float>(bounds.minX);
    originY_ = static_cast<float>(bounds.minY);
    double dx = bounds.maxX - bounds.minX;
    double dy = bounds.maxY - bounds.minY;
    cellSize_ = static_cast<float>(std::max(dx, dy) / width_);

    cells_.assign(width_ * height_, {});

    // Phase 1: Spatial binning
    for (const auto& p : points) {
        int32_t gx = static_cast<int32_t>((p.x - bounds.minX) / cellSize_);
        int32_t gy = static_cast<int32_t>((p.y - bounds.minY) / cellSize_);
        if (gx < 0 || gx >= static_cast<int32_t>(width_)) continue;
        if (gy < 0 || gy >= static_cast<int32_t>(height_)) continue;

        auto& cell = cells_[gy * width_ + gx];
        float z = static_cast<float>(p.z);
        cell.maxZ = std::max(cell.maxZ, z);
        cell.minZ = std::min(cell.minZ, z);
        cell.sumZ += z;
        cell.count++;
        cell.valid = true;
    }

    auto t1 = std::chrono::steady_clock::now();

    // Phase 2: Interpolate empty cells
    InterpolateEmptyCells();
    auto t2 = std::chrono::steady_clock::now();

    // Phase 3: Compute elevation range from valid cells only
    minElevation_ = std::numeric_limits<float>::max();
    maxElevation_ = std::numeric_limits<float>::lowest();
    uint32_t occupied = 0;
    for (const auto& cell : cells_) {
        if (cell.valid) {
            float avgZ = cell.AverageElevation();
            minElevation_ = std::min(minElevation_, avgZ);
            maxElevation_ = std::max(maxElevation_, avgZ);
            occupied++;
        }
    }
    if (occupied == 0) {
        minElevation_ = 0.0f;
        maxElevation_ = 0.0f;
    }

    stats_.gridWidth = width_;
    stats_.gridHeight = height_;
    stats_.totalCells = width_ * height_;
    stats_.occupiedCells = occupied;
    stats_.minElevation = minElevation_;
    stats_.maxElevation = maxElevation_;
    stats_.binningTimeMs = ElapsedMs(t0, t1);
    stats_.interpolateTimeMs = ElapsedMs(t1, t2);
    stats_.totalTimeMs = ElapsedMs(t0, t2);

    SLOG_INFO("ElevationGrid: %ux%u grid, %u/%u occupied, "
              "elev=[%.1f, %.1f], bin=%.1fms interp=%.1fms",
              width_, height_, occupied, width_ * height_,
              minElevation_, maxElevation_,
              stats_.binningTimeMs, stats_.interpolateTimeMs);
}

// ---------------------------------------------------------------------------
// Fill empty cells by averaging valid neighbours (BFS flood-fill).
// Produces smooth terrain in gaps without extrapolating beyond the data
// extent. O(W*H) worst case but typically much less.
// Interpolated cells are marked with interpolated=true so that CreateMesh()
// can skip triangles that touch them (no false terrain in data gaps).
// ---------------------------------------------------------------------------
void ElevationGrid::InterpolateEmptyCells() {
    if (cells_.empty()) return;

    struct QueueEntry { uint32_t x; uint32_t y; };

    std::vector<QueueEntry> frontier;
    frontier.reserve(cells_.size() / 4);

    // Seed with all valid cells
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            if (cells_[y * width_ + x].valid) {
                frontier.push_back({x, y});
            }
        }
    }

    // BFS: each invalid cell that neighbours a valid cell gets filled,
    // then becomes a seed for further propagation.
    uint32_t filled = 0;
    size_t head = 0;
    while (head < frontier.size()) {
        auto [cx, cy] = frontier[head++];
        float cxZ = cells_[cy * width_ + cx].AverageElevation();

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int32_t nx = static_cast<int32_t>(cx) + dx;
                int32_t ny = static_cast<int32_t>(cy) + dy;
                if (nx < 0 || nx >= static_cast<int32_t>(width_)) continue;
                if (ny < 0 || ny >= static_cast<int32_t>(height_)) continue;

                auto& neighbour = cells_[ny * width_ + nx];
                if (neighbour.valid) continue;

                // Fill with the seed cell's average elevation
                neighbour.maxZ = cxZ;
                neighbour.minZ = cxZ;
                neighbour.sumZ = cxZ;
                neighbour.count = 1;
                neighbour.valid = true;
                neighbour.interpolated = true;
                filled++;
                frontier.push_back({static_cast<uint32_t>(nx),
                                    static_cast<uint32_t>(ny)});
            }
        }
    }

    if (filled > 0) {
        SLOG_INFO("InterpolateEmptyCells: filled %u empty cells via BFS", filled);
    }
}
 
// ---------------------------------------------------------------------------
// Compute terrain normals from neighboring cell elevations using central
// differences. For Z-up coordinates:
//   dzdx = (zRight - zLeft) / (2 * cellSize)
//   dzdy = (zUp - zDown) / (2 * cellSize)
//   normal = normalize(-dzdx, -dzdy, 1.0)
// Boundaries use one-sided differences.
// ---------------------------------------------------------------------------
void ElevationGrid::ComputeNormals() {
    if (cells_.empty() || width_ < 2 || height_ < 2) return;
    const float inv2dx = 1.0f / (2.0f * cellSize_);

    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            auto& cell = cells_[y * width_ + x];
            if (!cell.valid) continue;

            float zL = 0.0f, zR = 0.0f, zD = 0.0f, zU = 0.0f;
            bool hasL = false, hasR = false, hasD = false, hasU = false;

            // Left (x-1)
            if (x > 0) {
                const auto& cL = cells_[y * width_ + (x - 1)];
                if (cL.valid) { zL = cL.AverageElevation(); hasL = true; }
            }
            // Right (x+1)
            if (x + 1 < width_) {
                const auto& cR = cells_[y * width_ + (x + 1)];
                if (cR.valid) { zR = cR.AverageElevation(); hasR = true; }
            }
            // Down (y-1) - grid Y increases downward
            if (y > 0) {
                const auto& cD = cells_[(y - 1) * width_ + x];
                if (cD.valid) { zD = cD.AverageElevation(); hasD = true; }
            }
            // Up (y+1)
            if (y + 1 < height_) {
                const auto& cU = cells_[(y + 1) * width_ + x];
                if (cU.valid) { zU = cU.AverageElevation(); hasU = true; }
            }

            float dzdx = 0.0f, dzdy = 0.0f;

            if (hasL && hasR) {
                dzdx = (zR - zL) * inv2dx;
            } else if (hasR) {
                // Forward difference
                dzdx = (zR - cell.AverageElevation()) / cellSize_;
            } else if (hasL) {
                // Backward difference
                dzdx = (cell.AverageElevation() - zL) / cellSize_;
            }

            if (hasD && hasU) {
                dzdy = (zU - zD) * inv2dx;
            } else if (hasU) {
                dzdy = (zU - cell.AverageElevation()) / cellSize_;
            } else if (hasD) {
                dzdy = (cell.AverageElevation() - zD) / cellSize_;
            }

            // Z-up: normal = normalize(-dzdx, -dzdy, 1.0)
            float nx = -dzdx;
            float ny = -dzdy;
            float nz = 1.0f;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6) {
                cell.normalX = nx / len;
                cell.normalY = ny / len;
                cell.normalZ = nz / len;
            } else {
                cell.normalX = 0.0f;
                cell.normalY = 0.0f;
                cell.normalZ = 1.0f;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Create a SurfaceMesh with shared vertices from the elevation grid.
// Triangles touching ANY invalid (un-interpolated) cell are skipped.
// Uses pre-computed terrain normals from ComputeNormals().
// ---------------------------------------------------------------------------
SurfaceMesh ElevationGrid::CreateMesh() {
    auto t0 = std::chrono::steady_clock::now();

    SurfaceMesh mesh;
    auto& vertices = mesh.Vertices();
    auto& triangles = mesh.Triangles();

    // One shared vertex per grid cell
    vertices.reserve(width_ * height_);
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            const auto& cell = cells_[y * width_ + x];
            SurfaceVertex v = {};

            v.position[0] = GridToWorldX(x);
            v.position[1] = GridToWorldY(y);
            v.position[2] = cell.valid ? cell.AverageElevation()
                                       : 0.0f;

            // Pre-computed terrain normal from grid elevation differences
            v.normal[0] = cell.normalX;
            v.normal[1] = cell.normalY;
            v.normal[2] = cell.normalZ;

            // Classification ID from dominant class in cell (for PTC surface coloring)
            v.classificationID = cell.dominantClass;

            // Neutral grey — actual colour from shader elevation modes
            v.color[0] = 0.5f;
            v.color[1] = 0.5f;
            v.color[2] = 0.5f;

            vertices.push_back(v);
        }
    }

    // Indexed triangles — skip ANY triangle that touches an invalid cell
    triangles.reserve(2 * (width_ - 1) * (height_ - 1));
    for (uint32_t y = 0; y < height_ - 1; ++y) {
        for (uint32_t x = 0; x < width_ - 1; ++x) {
            uint32_t tl = y * width_ + x;
            uint32_t tr = tl + 1;
            uint32_t bl = (y + 1) * width_ + x;
            uint32_t br = bl + 1;

            // Skip if ANY corner is invalid (no real data + no interpolation)
            if (!cells_[tl].valid || !cells_[tr].valid ||
                !cells_[bl].valid) continue;
            if (!cells_[tr].valid || !cells_[br].valid ||
                !cells_[bl].valid) continue;

            triangles.push_back({tl, tr, bl});
            triangles.push_back({tr, br, bl});
        }
    }

    mesh.ComputeBounds();

    auto t1 = std::chrono::steady_clock::now();
    stats_.meshGenTimeMs = ElapsedMs(t0, t1);
    stats_.totalTimeMs += stats_.meshGenTimeMs;

    SLOG_INFO("ElevationGrid: Mesh %zu verts, %zu tris, "
              "elev=[%.1f, %.1f], mesh=%.1fms",
              vertices.size(), triangles.size(),
              minElevation_, maxElevation_, stats_.meshGenTimeMs);

    // [ELEVATION GRID DEBUG] - verify Z preservation and terrain quality
    if (!vertices.empty()) {
        float meshMinZ = vertices[0].position[2];
        float meshMaxZ = vertices[0].position[2];
        double meshSumZ = 0.0;
        uint32_t validVerts = 0;
        double sumNz = 0.0;
        float minNz = 1.0f;
        uint32_t normalCount = 0;
        for (const auto& v : vertices) {
            float z = v.position[2];
            if (z != 0.0f) {
                if (z < meshMinZ) meshMinZ = z;
                if (z > meshMaxZ) meshMaxZ = z;
                meshSumZ += z;
                validVerts++;
            }
            float nz = v.normal[2];
            sumNz += nz;
            if (nz < minNz) minNz = nz;
            normalCount++;
        }
        float meanNz = (normalCount > 0) ? static_cast<float>(sumNz / normalCount) : 1.0f;
        // Z variance: how much terrain detail is preserved
        double meshMeanZ = (validVerts > 0) ? meshSumZ / validVerts : 0.0;
        double sumSq = 0.0;
        for (const auto& v : vertices) {
            double dz = v.position[2] - meshMeanZ;
            sumSq += dz * dz;
        }
        double meshStddevZ = (validVerts > 0) ? std::sqrt(sumSq / validVerts) : 0.0;

        fprintf(stderr,
            "[SURFACE QUALITY]\n"
            "  Grid: %ux%u, cellSize=%.3f\n"
            "  Vertices: %zu (valid: %u)\n"
            "  Triangles: %zu\n"
            "  Min Z: %.3f\n"
            "  Max Z: %.3f\n"
            "  Z Range: %.3f\n"
            "  Mean Z: %.3f\n"
            "  Std deviation: %.3f\n"
            "  Mean Nz: %.4f (1.0=flat, <1.0=sloped)\n"
            "  Min Nz: %.4f (max slope)\n"
            "  Mesh timing: %.1fms\n",
            width_, height_, cellSize_,
            vertices.size(), validVerts,
            triangles.size(),
            meshMinZ, meshMaxZ, meshMaxZ - meshMinZ,
            meshMeanZ, meshStddevZ,
            meanNz, minNz,
            stats_.meshGenTimeMs);
        fflush(stderr);
    }

    return mesh;
}

} // namespace surface
} // namespace workstation
