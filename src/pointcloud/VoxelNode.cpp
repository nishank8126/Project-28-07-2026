#include "workstation/pointcloud/VoxelNode.h"

namespace workstation { namespace pointcloud {

void VoxelNode::AddChild(std::unique_ptr<PointCloudNode> child) {
    if (!child) return;
    std::lock_guard<std::mutex> lock(mtx_);
    child->setParent(this);
    children_.push_back(std::move(child));
}

size_t VoxelNode::ChildCount() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return children_.size();
}

PointCloudNode* VoxelNode::Child(size_t i) const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (i >= children_.size()) return nullptr;
    return children_[i].get();
}

} // namespace pointcloud
} // namespace workstation
