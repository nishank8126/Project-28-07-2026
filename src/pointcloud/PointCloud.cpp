#include "workstation/pointcloud/PointCloud.h"
#include <atomic>
#include <cstring>
#include <functional>

namespace workstation { namespace pointcloud {

namespace {
    std::atomic<uint32_t> g_nextCloudId{1};
}

uint32_t PointCloud::nextId() { return g_nextCloudId.fetch_add(1); }

PointCloud::PointCloud() : id_(nextId()) {}

void PointCloud::SetName(const char* n) {
    if (!n) { name_[0] = '\0'; return; }
    std::strncpy(name_, n, sizeof(name_) - 1);
    name_[sizeof(name_) - 1] = '\0';
}

void PointCloud::Finalize() {
    pointCount_ = 0;
    attributes_ = PointAttributeMask();
    memoryBytes_ = 0;
    if (!root_) return;

    std::function<void(PointCloudNode*)> visit = [&](PointCloudNode* n) {
        if (!n) return;
        n->setOwner(this);
        pointCount_ += n->PointCount();
        attributes_ = attributes_ | n->Attributes();
        memoryBytes_ += n->MemoryBytes();
        if (VoxelNode* v = dynamic_cast<VoxelNode*>(n)) {
            for (size_t i = 0; i < v->ChildCount(); ++i)
                visit(v->Child(i));
        }
    };
    visit(root_);
}

std::unique_ptr<VoxelNode> CreateVoxelNode(const BoundingBox& bounds, double density) {
    auto v = std::make_unique<VoxelNode>();
    v->setBounds(bounds);
    v->setDensity(density);
    return v;
}

std::unique_ptr<PointCloudNode> CreatePointNode(const BoundingBox& bounds) {
    auto n = std::make_unique<PointCloudNode>();
    n->setBounds(bounds);
    return n;
}

void FinalizeCloud(PointCloud& cloud) { cloud.Finalize(); }

} // namespace pointcloud
} // namespace workstation
