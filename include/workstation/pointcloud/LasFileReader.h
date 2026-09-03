#pragma once
#include <string>

namespace workstation {
namespace spatial { class CoordinateNormalizationManager; }
namespace pointcloud {

class PointCloud;

// Loads a .las or .laz file (via LASzip) into outCloud. Returns false and
// (if provided) fills errorMessage on failure.
//
// Points are recentered to keep float32 storage precise (LAS coordinates are
// commonly large UTM/State Plane values). If `sharedNormalizer` is given and
// already has an origin (e.g. established by a previously loaded SNT
// attachment or point cloud), that shared origin is reused so this cloud
// registers correctly against that existing geometry instead of recentering
// around its own file-local midpoint. If the normalizer has no origin yet,
// this file's own midpoint is used and written back into the normalizer so
// subsequent loads can share it.
bool LoadLasFile(const std::string& filepath, PointCloud& outCloud,
                  std::string* errorMessage = nullptr,
                  spatial::CoordinateNormalizationManager* sharedNormalizer = nullptr);

} // namespace pointcloud
} // namespace workstation
