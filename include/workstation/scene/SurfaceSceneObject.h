#pragma once
#include "workstation/scene/SceneObject.h"
#include "workstation/surface/SurfaceRenderer.h"
#include "workstation/surface/SurfaceMeshCache.h"
#include "workstation/pointcloud/PointCloud.h"

#include <memory>
#include <string>

namespace workstation {
namespace renderer { class RenderQueue; }

namespace scene {

class SurfaceSceneObject : public SceneObject {
public:
    SurfaceSceneObject() = default;
    ~SurfaceSceneObject() override;

    ObjectType GetType() const override { return ObjectType::Mesh; }
    const char* GetTypeName() const override { return "Surface"; }

    bool Load(std::string* error = nullptr) override;
    void Unload() override;
    void Update() override;
    bool IsLoaded() const override { return loaded_; }

    spatial::BoundingBox GetBounds() const override;

    void SubmitRenderCommands(renderer::RenderQueue& queue,
                               void* linePipeline,
                               void* pipelineLayout) override;

    void SetPointCloud(pointcloud::PointCloud* cloud) { cloud_ = cloud; }
    pointcloud::PointCloud* GetPointCloud() const { return cloud_; }

    void SetSurfaceRenderer(surface::SurfaceRenderer* renderer) { surfaceRenderer_ = renderer; }
    surface::SurfaceRenderer* GetSurfaceRenderer() const { return surfaceRenderer_; }

    void SetCache(surface::SurfaceMeshCache* cache) { cache_ = cache; }
    surface::SurfaceMeshCache* GetCache() const { return cache_; }

    void GenerateSurface(const surface::SurfaceGenerationParams& params = {});
    bool IsSurfaceGenerated() const { return surfaceGenerated_; }

    void SetTriangleDensity(float density) { triangleDensity_ = density; }
    float GetTriangleDensity() const { return triangleDensity_; }

    void SetNormalQuality(int quality) { normalQuality_ = quality; }
    int GetNormalQuality() const { return normalQuality_; }

private:
    pointcloud::PointCloud* cloud_ = nullptr;
    surface::SurfaceRenderer* surfaceRenderer_ = nullptr;
    surface::SurfaceMeshCache* cache_ = nullptr;
    bool loaded_ = false;
    bool surfaceGenerated_ = false;
    float triangleDensity_ = 1.0f;
    int normalQuality_ = 10;
};

} // namespace scene
} // namespace workstation
