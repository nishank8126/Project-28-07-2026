#pragma once
#include "workstation/pointcloud/BoundingBox.h"
#include "workstation/pointcloud/PointStorage.h"
#include "workstation/pointcloud/PointAttributeMask.h"
#include <cstdint>

namespace workstation { namespace pointcloud {

class PointCloud;

// Base node of the point-cloud scene graph (clean-room; no proprietary name).
// Holds bounds, parent link, owning cloud, and the node's attribute channels.
class PointCloudNode {
public:
    PointCloudNode() = default;
    virtual ~PointCloudNode() = default;

    const BoundingBox& bounds() const { return bounds_; }
    void setBounds(const BoundingBox& b) { bounds_ = b; }

    PointCloudNode* parent() const { return parent_; }
    void setParent(PointCloudNode* p) { parent_ = p; }

    PointCloud* owner() const { return owner_; }
    void setOwner(PointCloud* o) { owner_ = o; }

    PointStorage& channels() { return channels_; }
    const PointStorage& channels() const { return channels_; }

    virtual bool IsVoxel() const { return false; }

    // Point count derived from the node's XYZ channel (if present).
    uint64_t PointCount() const { return channels_.PointCount(); }
    // Attribute flags derived from the node's channels.
    PointAttributeMask Attributes() const { return channels_.Attributes(); }
    // Approximate memory footprint of the node's channel buffers.
    size_t MemoryBytes() const { return channels_.MemoryBytes(); }

protected:
    BoundingBox bounds_;
    PointCloudNode* parent_ = nullptr;
    PointCloud* owner_ = nullptr;
    PointStorage channels_;
};

} // namespace pointcloud
} // namespace workstation
