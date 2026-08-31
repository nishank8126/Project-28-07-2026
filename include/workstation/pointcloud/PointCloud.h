#pragma once
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/VoxelNode.h"
#include "workstation/pointcloud/PointAttributeMask.h"
#include "workstation/pointcloud/PointChannelManager.h"
#include "workstation/pointcloud/PointStorage.h"
#include <cstdint>

namespace workstation { namespace pointcloud {

// A single point cloud: an owning root node plus aggregate stats built by
// Finalize() (FUN_18007d990).
class PointCloud {
public:
    PointCloud();

    void SetName(const char* n);
    const char* Name() const { return name_; }

    void SetRoot(PointCloudNode* root) { root_ = root; }
    PointCloudNode* Root() const { return root_; }

    uint64_t PointCount() const { return pointCount_; }
    PointAttributeMask Attributes() const { return attributes_; }
    size_t MemoryBytes() const { return memoryBytes_; }
    uint32_t Id() const { return id_; }

    // Build aggregate stats and bind every node's owner to this cloud.
    void Finalize();

private:
    static uint32_t nextId();
    char name_[64] = {0};
    PointCloudNode* root_ = nullptr;
    uint64_t pointCount_ = 0;
    PointAttributeMask attributes_;
    size_t memoryBytes_ = 0;
    uint32_t id_ = 0;
};

// ---- Clean-room factory API (free functions) ----

// CreateVoxelNode (FUN_180081100)
std::unique_ptr<VoxelNode> CreateVoxelNode(const BoundingBox& bounds, double density = 1.0);
// CreatePointNode (FUN_1800764c0)
std::unique_ptr<PointCloudNode> CreatePointNode(const BoundingBox& bounds);
// FinalizeCloud (FUN_18007d990)
void FinalizeCloud(PointCloud& cloud);

} // namespace pointcloud
} // namespace workstation
