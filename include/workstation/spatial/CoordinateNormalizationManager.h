#pragma once
#include "workstation/spatial/BoundingBox.h"
#include <cstdint>

namespace workstation { namespace spatial {

// Centralized coordinate normalization for large-coordinate point clouds.
//
// Real-world LiDAR data often uses UTM or State Plane coordinates with values
// in the millions of metres.  Storing these as float32 (which has ~7 digits of
// precision) causes jitter for coordinates with many leading digits.  The
// solution is to subtract a shared world-space "origin" and store only the
// local (small) offsets.
//
// Every loaded point cloud and vector overlay applies the *same* origin so that
// all geometry is consistently registered.  The origin can be set manually
// (e.g. to a project datum) or computed automatically from the bounding box
// centre of all loaded data.
//
// Coordinate flow:
//   world-space (double)  --subtract origin-->  local-space (float)
//   local-space (float)   --add origin-->       world-space (double)
//
class CoordinateNormalizationManager {
public:
    CoordinateNormalizationManager() = default;

    // ---- Origin management ----

    // Set the origin explicitly (e.g. to a project datum or survey point).
    void SetOrigin(double x, double y, double z);

    // Get the current world-space origin.
    void GetOrigin(double& x, double& y, double& z) const;

    // Recompute the origin from the centre of the combined bounding box of all
    // loaded data.  Call after loading a new file or when the user requests a
    // "recenter" operation.
    void RecomputeFromBoundingBox(const BoundingBox& combinedBounds);

    // Reset the origin to (0, 0, 0) — effectively disabling normalization.
    void Reset();

    bool IsNormalized() const { return normalized_; }

    // ---- Coordinate transforms ----

    // Transform a world-space coordinate to local (rendering) space.
    void WorldToLocal(double wx, double wy, double wz,
                      float& lx, float& ly, float& lz) const;

    // Transform a local (rendering) space coordinate back to world-space.
    void LocalToWorld(float lx, float ly, float lz,
                      double& wx, double& wy, double& wz) const;

    // ---- Bounding-box helpers ----

    // Shift a bounding box from world-space to local-space.
    BoundingBox WorldToLocal(const BoundingBox& world) const;

    // Shift a bounding box from local-space to world-space.
    BoundingBox LocalToWorld(const BoundingBox& local) const;

private:
    double originX_ = 0.0;
    double originY_ = 0.0;
    double originZ_ = 0.0;
    bool normalized_ = false;
};

} // namespace spatial
} // namespace workstation
