#pragma once
#include "workstation/scene/SceneObject.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/renderer/PointCloudRenderAdapter.h"

#include <memory>
#include <string>

namespace workstation {
namespace renderer { class RenderQueue; }

namespace scene {

class PointCloudSceneObject : public SceneObject {
public:
    PointCloudSceneObject() = default;
    ~PointCloudSceneObject() override;

    ObjectType GetType() const override { return ObjectType::PointCloud; }
    const char* GetTypeName() const override { return "PointCloud"; }

    bool Load(std::string* error = nullptr) override;
    void Unload() override;
    void Update() override;
    bool IsLoaded() const override { return loaded_; }

    spatial::BoundingBox GetBounds() const override;

    void SubmitRenderCommands(renderer::RenderQueue& queue,
                               void* linePipeline,
                               void* pipelineLayout) override;

    void SetPointCloud(std::unique_ptr<pointcloud::PointCloud> cloud);
    pointcloud::PointCloud* GetPointCloud() const { return cloud_.get(); }

    void SetRenderAdapter(renderer::PointCloudRenderAdapter* adapter) { adapter_ = adapter; }
    renderer::PointCloudRenderAdapter* GetRenderAdapter() const { return adapter_; }

    void SetPointCount(uint64_t count) { pointCount_ = count; }
    uint64_t GetPointCount() const { return pointCount_; }

private:
    std::unique_ptr<pointcloud::PointCloud> cloud_;
    renderer::PointCloudRenderAdapter* adapter_ = nullptr;
    uint64_t pointCount_ = 0;
    bool loaded_ = false;
};

} // namespace scene
} // namespace workstation
