#include "workstation/scene/PointCloudSceneObject.h"
#include "workstation/renderer/CadRenderer.h"
#include <cstdio>

namespace workstation {
namespace scene {

PointCloudSceneObject::~PointCloudSceneObject() { Unload(); }

bool PointCloudSceneObject::Load(std::string* error) {
    if (!cloud_) {
        if (error) *error = "No point cloud set";
        return false;
    }
    pointCount_ = cloud_->PointCount();
    loaded_ = true;
    return true;
}

void PointCloudSceneObject::Unload() {
    if (node_) node_->SetVisible(false);
    loaded_ = false;
    pointCount_ = 0;
}

void PointCloudSceneObject::Update() {
    if (!loaded_ || !cloud_) return;
    pointCount_ = cloud_->PointCount();
    if (cloud_->Root()) {
        auto rootBounds = cloud_->Root()->bounds();
        spatial::BoundingBox b;
        b.minX = rootBounds.minX;
        b.minY = rootBounds.minY;
        b.minZ = rootBounds.minZ;
        b.maxX = rootBounds.maxX;
        b.maxY = rootBounds.maxY;
        b.maxZ = rootBounds.maxZ;
        if (node_) node_->SetBounds(b);
    }
}

spatial::BoundingBox PointCloudSceneObject::GetBounds() const {
    if (cloud_ && cloud_->Root()) {
        auto rootBounds = cloud_->Root()->bounds();
        spatial::BoundingBox b;
        b.minX = rootBounds.minX;
        b.minY = rootBounds.minY;
        b.minZ = rootBounds.minZ;
        b.maxX = rootBounds.maxX;
        b.maxY = rootBounds.maxY;
        b.maxZ = rootBounds.maxZ;
        return b;
    }
    return {};
}

void PointCloudSceneObject::SubmitRenderCommands(renderer::RenderQueue& queue,
                                                   void* linePipeline,
                                                   void* pipelineLayout) {
    (void)queue;
    (void)linePipeline;
    (void)pipelineLayout;
}

void PointCloudSceneObject::SetPointCloud(std::unique_ptr<pointcloud::PointCloud> cloud) {
    cloud_ = std::move(cloud);
    loaded_ = cloud_ != nullptr;
    if (cloud_) pointCount_ = cloud_->PointCount();
}

} // namespace scene
} // namespace workstation
