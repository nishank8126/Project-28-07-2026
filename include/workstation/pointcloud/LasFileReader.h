#pragma once
#include <string>

namespace workstation {
namespace pointcloud {

class PointCloud;

// Loads a .las or .laz file (via LASzip) into outCloud. Returns false and
// (if provided) fills errorMessage on failure.
bool LoadLasFile(const std::string& filepath, PointCloud& outCloud,
                  std::string* errorMessage = nullptr);

} // namespace pointcloud
} // namespace workstation
