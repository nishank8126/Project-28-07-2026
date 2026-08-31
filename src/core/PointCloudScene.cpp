#include "workstation/core/PointCloudScene.h"

namespace workstation { namespace core {

PointCloudScene& PointCloudScene::instance() {
    static PointCloudScene s;
    return s;
}

pointcloud::PointCloud* PointCloudScene::CreateCloud(const char* name) {
    auto c = std::make_unique<pointcloud::PointCloud>();
    c->SetName(name);
    pointcloud::PointCloud* raw = c.get();
    AddCloud(std::move(c));
    SetActiveCloud(raw);
    return raw;
}

void PointCloudScene::AddCloud(std::unique_ptr<pointcloud::PointCloud> cloud) {
    if (!cloud) return;
    if (active_ == nullptr) active_ = cloud.get();
    clouds_.push_back(std::move(cloud));
}

pointcloud::BoundingBox PointCloudScene::SceneBounds() const {
    pointcloud::BoundingBox b;
    bool any = false;
    for (const auto& c : clouds_) {
        if (!c->Root()) continue;
        const auto& n = c->Root()->bounds();
        if (!any) { b = n; any = true; }
        else {
            if (n.minX < b.minX) b.minX = n.minX;
            if (n.minY < b.minY) b.minY = n.minY;
            if (n.minZ < b.minZ) b.minZ = n.minZ;
            if (n.maxX > b.maxX) b.maxX = n.maxX;
            if (n.maxY > b.maxY) b.maxY = n.maxY;
            if (n.maxZ > b.maxZ) b.maxZ = n.maxZ;
        }
    }
    return b;
}

} // namespace core
} // namespace workstation
