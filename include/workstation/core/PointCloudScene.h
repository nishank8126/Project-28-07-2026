#pragma once
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/BoundingBox.h"
#include <memory>
#include <vector>

namespace workstation { namespace core {

// Clean-room point cloud scene: holds the clouds in a document and tracks the
// single active cloud (match: the workstation edits one active cloud at a time).
class PointCloudScene {
public:
    static PointCloudScene& instance();

    pointcloud::PointCloud* CreateCloud(const char* name);
    void AddCloud(std::unique_ptr<pointcloud::PointCloud> cloud);

    pointcloud::PointCloud* ActiveCloud() const { return active_; }
    void SetActiveCloud(pointcloud::PointCloud* c) { active_ = c; }

    size_t CloudCount() const { return clouds_.size(); }

    // Union of all cloud root bounds.
    pointcloud::BoundingBox SceneBounds() const;

private:
    PointCloudScene() = default;
    std::vector<std::unique_ptr<pointcloud::PointCloud>> clouds_;
    pointcloud::PointCloud* active_ = nullptr;
};

} // namespace core
} // namespace workstation
