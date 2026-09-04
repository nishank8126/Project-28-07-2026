#pragma once
#include "workstation/pointcloud/PointCloud.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace workstation {
namespace tools {

// Reported periodically during a classification pass; onProgress gets a
// value in [0,1], shouldCancel is polled between chunks so a long pass on a
// large cloud can be aborted from the UI thread that queued it.
struct ClassifyProgress {
    std::function<void(float)> onProgress;
    std::function<bool()> shouldCancel;
};

// Result of one classification pass. Carries enough to undo: the indices
// that were actually changed and what they were before, so UndoClassifyResult
// can restore exactly those points without needing a full-cloud snapshot.
struct ClassifyResult {
    std::vector<uint32_t> changedIndices;
    std::vector<uint8_t> previousClasses;
    bool cancelled = false;
    size_t PointsChanged() const { return changedIndices.size(); }
};

// TerraScan-style "Isolated Points": a point with fewer than minNeighbors
// other points within radius (3-D) is almost certainly atmospheric/sensor
// noise rather than a real surface, so it gets reassigned to targetClass.
struct IsolatedPointsParams {
    double radius = 2.0;
    int minNeighbors = 3;
    uint8_t targetClass = 7; // ASPRS Low Point (noise)
};

// TerraScan-style "Low Points": flags points sitting well below every point
// in their local 2-D neighbourhood (multipath/below-ground noise). This is a
// simplified single-neighbourhood variant of the reference algorithm (which
// additionally groups nearby low points into tiers separated by a vertical
// gap) - it catches the common case (isolated low outliers) without the
// tiering/grouping pass.
struct LowPointsParams {
    double radius = 5.0;
    double heightThreshold = 1.0;
    uint8_t targetClass = 7;
};

// Simplified Progressive TIN Densification ground classifier: seed the
// lowest point per grid cell as initial ground, then iteratively promote
// points that are within distance/angle thresholds of their nearest current
// ground point, tightening thresholds each iteration. A real PTD interpolates
// a local TIN plane from several nearby ground points per candidate; this
// nearest-ground-point variant is cheaper and works well on fairly flat/
// gently sloped terrain, but is less accurate on steep or highly irregular
// ground than a full per-triangle plane fit.
struct GroundPTDParams {
    double gridCellSize = 10.0;
    double initialDistance = 0.5;
    double iterationDistance = 0.25;
    double angleThresholdDeg = 6.0;
    int maxIterations = 8;
    uint8_t groundClass = 2;
};

ClassifyResult ClassifyIsolatedPoints(pointcloud::PointCloud& cloud,
                                       const IsolatedPointsParams& params,
                                       const ClassifyProgress& progress = {});

ClassifyResult ClassifyLowPoints(pointcloud::PointCloud& cloud,
                                  const LowPointsParams& params,
                                  const ClassifyProgress& progress = {});

ClassifyResult ClassifyGroundPTD(pointcloud::PointCloud& cloud,
                                  const GroundPTDParams& params,
                                  const ClassifyProgress& progress = {});

// Restores every point touched by `result` to its previousClasses value.
void UndoClassifyResult(pointcloud::PointCloud& cloud, const ClassifyResult& result);

} // namespace tools
} // namespace workstation
