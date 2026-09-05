#pragma once
#include "workstation/spatial/BoundingBox.h"
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/math/Point3d.h"

#include <vector>
#include <cstdint>
#include <limits>

namespace workstation {
namespace surface {

enum class ElevationSourceMode {
    AllPoints = 0,   // Use every point (DSM-like)
    GroundOnly = 1,  // Classification 2 = Ground (DTM)
    HighestReturn = 2, // Highest Z per cell
    UserClass = 3    // Filter by user-selected classification codes
};

struct ElevationCell {
    float maxZ = -std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float sumZ = 0.0f;
    uint32_t count = 0;
    bool valid = false;
    bool interpolated = false;
    uint32_t classCounts[256] = {0};
    uint32_t dominantClass = 0;

    float AverageElevation() const {
        return (count > 0) ? (sumZ / count) : 0.0f;
    }
};

struct ElevationGridParams {
    uint32_t resolution = 2048;
    ElevationSourceMode sourceMode = ElevationSourceMode::AllPoints;
    // For UserClass mode: which classification codes to include
    std::vector<uint8_t> selectedClasses;
};

struct ElevationGridStats {
    uint32_t gridWidth = 0;
    uint32_t gridHeight = 0;
    uint32_t occupiedCells = 0;
    uint32_t totalCells = 0;
    float minElevation = 0.0f;
    float maxElevation = 0.0f;
    double binningTimeMs = 0.0;
    double interpolateTimeMs = 0.0;
    double meshGenTimeMs = 0.0;
    double totalTimeMs = 0.0;
};

class ElevationGrid {
public:
    ElevationGrid() = default;
    ~ElevationGrid() = default;

    // Generate from a point cloud (reads XYZ + classification internally).
    void Generate(const pointcloud::PointCloud& cloud,
                  const ElevationGridParams& params = {});

    // Generate from pre-extracted points (shared across LOD levels).
    void Generate(const std::vector<math::Point3d>& points,
                  const spatial::BoundingBox& bounds,
                  const ElevationGridParams& params = {});

    // Fill empty cells by interpolating from neighbours.
    void InterpolateEmptyCells();

    // Convert to SurfaceMesh for rendering (shared vertices, indexed tris).
    SurfaceMesh CreateMesh();

    // Accessors
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    float GetCellSize() const { return cellSize_; }
    float GetOriginX() const { return originX_; }
    float GetOriginY() const { return originY_; }
    float GetMinElevation() const { return minElevation_; }
    float GetMaxElevation() const { return maxElevation_; }
    bool IsEmpty() const { return cells_.empty(); }
    const ElevationCell& GetCell(uint32_t x, uint32_t y) const {
        return cells_[y * width_ + x];
    }
    const std::vector<ElevationCell>& GetCells() const { return cells_; }
    const ElevationGridStats& GetStats() const { return stats_; }

    // Grid-to-world coordinate conversion
    float GridToWorldX(uint32_t gx) const {
        return originX_ + (gx + 0.5f) * cellSize_;
    }
    float GridToWorldY(uint32_t gy) const {
        return originY_ + (gy + 0.5f) * cellSize_;
    }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    float cellSize_ = 0.0f;
    float originX_ = 0.0f;
    float originY_ = 0.0f;
    float minElevation_ = 0.0f;
    float maxElevation_ = 0.0f;
    std::vector<ElevationCell> cells_;
    ElevationGridStats stats_;
};

} // namespace surface
} // namespace workstation
