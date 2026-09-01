#include "workstation/pointcloud/LasFileReader.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/BoundingBox.h"

#include <laszip_api.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <vector>

namespace workstation {
namespace pointcloud {

namespace {

bool PointFormatHasColor(laszip_U8 pointDataFormat) {
    switch (pointDataFormat) {
        case 2: case 3: case 5: case 7: case 8: case 10:
            return true;
        default:
            return false;
    }
}

std::string BaseName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

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

// Estimates a per-point surface normal from each point's local neighborhood
// (grid-bucketed nearest-neighbor search + PCA plane fit -- the standard
// technique for raw LiDAR data, which has no true per-point normal the way a
// mesh does). Brute-force O(n^2) search is infeasible at LiDAR scale (tens of
// millions of points), so points are bucketed into a uniform grid sized for
// roughly a dozen points per cell, and neighbor search only scans nearby
// cells.
void EstimateNormals(const std::vector<double>& rawCoords, size_t count,
                      double minX, double minY, double minZ,
                      double maxX, double maxY, double maxZ,
                      std::vector<float>& outNormals) {
    outNormals.assign(count * 3, 0.0f);
    if (count == 0) return;

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
        grid[cellOf(rawCoords[i * 3 + 0], rawCoords[i * 3 + 1], rawCoords[i * 3 + 2])]
            .push_back(static_cast<uint32_t>(i));
    }

    constexpr int kNeighbors = 12;
    std::vector<std::pair<double, uint32_t>> candidates;
    size_t fallbackCount = 0;
    double minCosUp = 1.0, maxCosUp = -1.0;

    for (size_t i = 0; i < count; ++i) {
        double px = rawCoords[i * 3 + 0], py = rawCoords[i * 3 + 1], pz = rawCoords[i * 3 + 2];
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
                            double ddx = rawCoords[idx * 3 + 0] - px;
                            double ddy = rawCoords[idx * 3 + 1] - py;
                            double ddz = rawCoords[idx * 3 + 2] - pz;
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
            cx += rawCoords[idx * 3 + 0];
            cy += rawCoords[idx * 3 + 1];
            cz += rawCoords[idx * 3 + 2];
        }
        cx /= static_cast<double>(k);
        cy /= static_cast<double>(k);
        cz /= static_cast<double>(k);

        double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        for (size_t n = 0; n < k; ++n) {
            uint32_t idx = candidates[n].second;
            double ddx = rawCoords[idx * 3 + 0] - cx;
            double ddy = rawCoords[idx * 3 + 1] - cy;
            double ddz = rawCoords[idx * 3 + 2] - cz;
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
    for (size_t i = 0; i < std::min<size_t>(5, count); ++i) {
        fprintf(stderr, "  normal[%zu] = (%.3f, %.3f, %.3f)\n", i,
                outNormals[i * 3 + 0], outNormals[i * 3 + 1], outNormals[i * 3 + 2]);
    }
}

} // namespace

bool LoadLasFile(const std::string& filepath, PointCloud& outCloud, std::string* errorMessage) {
    laszip_POINTER reader = nullptr;
    if (laszip_create(&reader)) {
        if (errorMessage) *errorMessage = "laszip_create failed";
        return false;
    }

    laszip_BOOL isCompressed = 0;
    if (laszip_open_reader(reader, filepath.c_str(), &isCompressed)) {
        laszip_CHAR* err = nullptr;
        laszip_get_error(reader, &err);
        if (errorMessage) *errorMessage = err ? err : "failed to open LAS/LAZ file";
        laszip_destroy(reader);
        return false;
    }

    laszip_header_struct* header = nullptr;
    laszip_get_header_pointer(reader, &header);
    laszip_point_struct* point = nullptr;
    laszip_get_point_pointer(reader, &point);

    if (!header || !point) {
        if (errorMessage) *errorMessage = "failed to read header";
        laszip_close_reader(reader);
        laszip_destroy(reader);
        return false;
    }

    // laszip_get_point_count() reports points read so far (starts at 0), not
    // the file's total record count -- that comes from the header itself.
    laszip_I64 pointCount64 = header->number_of_point_records;
    if (pointCount64 == 0) {
        pointCount64 = static_cast<laszip_I64>(header->extended_number_of_point_records);
    }
    size_t pointCount = static_cast<size_t>(pointCount64);

    if (pointCount == 0) {
        if (errorMessage) *errorMessage = "file has no points";
        laszip_close_reader(reader);
        laszip_destroy(reader);
        return false;
    }

    bool hasColor = PointFormatHasColor(header->point_data_format);

    // Read raw (un-recentered) coordinates first and track the actual data
    // bounds ourselves -- some LAS/LAZ writers leave the header's
    // min_x/max_x/min_y/max_y/min_z/max_z degenerate (or stale), which would
    // otherwise silently recenter around the wrong origin and leave every
    // point far outside the camera's view.
    std::vector<double> rawCoords(pointCount * 3);
    std::vector<float> colors(pointCount * 3);
    std::vector<float> intensities(pointCount);
    std::vector<uint8_t> classifications(pointCount);

    double minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
    double coords[3] = {};
    size_t readCount = 0;
    for (; readCount < pointCount; ++readCount) {
        if (laszip_read_point(reader)) break;
        laszip_get_coordinates(reader, coords);

        rawCoords[readCount * 3 + 0] = coords[0];
        rawCoords[readCount * 3 + 1] = coords[1];
        rawCoords[readCount * 3 + 2] = coords[2];

        if (readCount == 0) {
            minX = maxX = coords[0];
            minY = maxY = coords[1];
            minZ = maxZ = coords[2];
        } else {
            if (coords[0] < minX) minX = coords[0]; else if (coords[0] > maxX) maxX = coords[0];
            if (coords[1] < minY) minY = coords[1]; else if (coords[1] > maxY) maxY = coords[1];
            if (coords[2] < minZ) minZ = coords[2]; else if (coords[2] > maxZ) maxZ = coords[2];
        }

        intensities[readCount] = point->intensity / 65535.0f;
        classifications[readCount] = point->classification;

        if (hasColor) {
            colors[readCount * 3 + 0] = point->rgb[0] / 65535.0f;
            colors[readCount * 3 + 1] = point->rgb[1] / 65535.0f;
            colors[readCount * 3 + 2] = point->rgb[2] / 65535.0f;
        } else {
            colors[readCount * 3 + 0] = 0.6f;
            colors[readCount * 3 + 1] = 0.6f;
            colors[readCount * 3 + 2] = 0.6f;
        }
    }

    laszip_close_reader(reader);
    laszip_destroy(reader);

    if (readCount == 0) {
        if (errorMessage) *errorMessage = "failed to read any points";
        return false;
    }

    // Points in a LAS file are typically large UTM-style coordinates;
    // recenter around the actual data's midpoint so float32 storage keeps
    // precision.
    double originX = (minX + maxX) * 0.5;
    double originY = (minY + maxY) * 0.5;
    double originZ = (minZ + maxZ) * 0.5;

    std::vector<float> positions(readCount * 3);
    for (size_t i = 0; i < readCount; ++i) {
        positions[i * 3 + 0] = static_cast<float>(rawCoords[i * 3 + 0] - originX);
        positions[i * 3 + 1] = static_cast<float>(rawCoords[i * 3 + 1] - originY);
        positions[i * 3 + 2] = static_cast<float>(rawCoords[i * 3 + 2] - originZ);
    }

    BoundingBox bounds;
    bounds.minX = minX - originX;
    bounds.minY = minY - originY;
    bounds.minZ = minZ - originZ;
    bounds.maxX = maxX - originX;
    bounds.maxY = maxY - originY;
    bounds.maxZ = maxZ - originZ;

    auto normalsStart = std::chrono::steady_clock::now();
    std::vector<float> normals;
    EstimateNormals(rawCoords, readCount, minX, minY, minZ, maxX, maxY, maxZ, normals);
    double normalsMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - normalsStart).count();
    fprintf(stderr, "[LasFileReader] Estimated normals for %zu points in %.1f ms\n",
            readCount, normalsMs);

    auto node = std::make_unique<PointCloudNode>();
    node->setBounds(bounds);

    node->channels().AddChannel(CreateChannel(
        ChannelId::XYZ, PointFormat::Float32, readCount, positions.data()));
    node->channels().AddChannel(CreateChannel(
        ChannelId::RGB, PointFormat::Float32, readCount, colors.data()));
    node->channels().AddChannel(CreateChannel(
        ChannelId::Intensity, PointFormat::Float32, readCount, intensities.data()));
    node->channels().AddChannel(CreateChannel(
        ChannelId::Classification, PointFormat::UInt8, readCount, classifications.data()));
    node->channels().AddChannel(CreateChannel(
        ChannelId::Normals, PointFormat::Float32, readCount, normals.data()));

    outCloud.SetName(BaseName(filepath).c_str());
    outCloud.SetRoot(node.release());
    outCloud.Finalize();

    fprintf(stderr, "[LasFileReader] Loaded %zu points from %s\n", readCount, filepath.c_str());
    fprintf(stderr, "[LasFileReader] First XYZ: (%.3f, %.3f, %.3f)\n",
            positions[0], positions[1], positions[2]);
    fprintf(stderr, "[LasFileReader] Bounds: min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)\n",
            bounds.minX, bounds.minY, bounds.minZ, bounds.maxX, bounds.maxY, bounds.maxZ);
    fprintf(stderr, "[LasFileReader] First RGB: (%.3f, %.3f, %.3f)\n",
            colors[0], colors[1], colors[2]);
    return true;
}

} // namespace pointcloud
} // namespace workstation
