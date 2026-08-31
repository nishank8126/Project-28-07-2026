#pragma once
#include "workstation/pointcloud/PointCloudNode.h"
#include <memory>
#include <mutex>
#include <vector>

namespace workstation { namespace pointcloud {

// Voxel node: an interior node that may contain child nodes. Inherits the base
// node. Density defaults to 1.0 (matches the confirmed query density). Child
// access is guarded by a mutex; a loading-state flag tracks streaming state.
class VoxelNode : public PointCloudNode {
public:
    VoxelNode() = default;

    bool IsVoxel() const override { return true; }

    double density() const { return density_; }
    void setDensity(double d) { density_ = d; }

    void AddChild(std::unique_ptr<PointCloudNode> child);
    size_t ChildCount() const;
    PointCloudNode* Child(size_t i) const;

    // Loading / streaming state (reset by default; not auto-populated).
    bool IsLoading() const { return loading_; }
    void SetLoading(bool v) { loading_ = v; }

    template <typename F>
    void ForEachChild(F f) const {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& c : children_) f(c.get());
    }

private:
    double density_ = 1.0;
    bool loading_ = false;
    mutable std::mutex mtx_;
    std::vector<std::unique_ptr<PointCloudNode>> children_;
};

} // namespace pointcloud
} // namespace workstation
