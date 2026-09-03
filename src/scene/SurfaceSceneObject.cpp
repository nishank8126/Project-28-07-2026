#include "workstation/scene/SurfaceSceneObject.h"
#include "workstation/renderer/RenderQueue.h"

namespace workstation {
namespace scene {

SurfaceSceneObject::~SurfaceSceneObject() {
    Unload();
}

bool SurfaceSceneObject::Load(std::string* error) {
    if (!cloud_) {
        if (error) *error = "No point cloud set";
        return false;
    }
    loaded_ = true;
    return true;
}

void SurfaceSceneObject::Unload() {
    loaded_ = false;
    surfaceGenerated_ = false;
}

void SurfaceSceneObject::Update() {
    if (!loaded_ || !cloud_) return;
    if (node_) {
        auto* root = cloud_->Root();
        if (root) {
            const auto& b = root->bounds();
            node_->SetBounds({b.minX, b.minY, b.minZ, b.maxX, b.maxY, b.maxZ});
        }
    }
}

spatial::BoundingBox SurfaceSceneObject::GetBounds() const {
    if (!cloud_ || !cloud_->Root()) return {};
    const auto& b = cloud_->Root()->bounds();
    return {b.minX, b.minY, b.minZ, b.maxX, b.maxY, b.maxZ};
}

void SurfaceSceneObject::SubmitRenderCommands(renderer::RenderQueue& queue,
                                                void* linePipeline,
                                                void* pipelineLayout) {
    (void)queue;
    (void)linePipeline;
    (void)pipelineLayout;
}

void SurfaceSceneObject::GenerateSurface(const surface::SurfaceGenerationParams& params) {
    if (!cloud_ || !surfaceRenderer_ || surfaceGenerated_) return;

    surface::SurfaceGenerationParams genParams = params;
    genParams.normalNeighborCount = normalQuality_;

    if (cache_ && cloud_) {
        auto* cached = cache_->GetOrCreate(cloud_->Id(), *cloud_, genParams);
        // Surface the cache's generation metrics (no-op on a cache hit, which
        // is exactly what we want: stats describe the last real Generate()).
        surfaceRenderer_->SetLastGenerationStats(cache_->GetLastStats());
        if (cached && cached->isValid) {
            surfaceRenderer_->AddSurfaceMesh(cached->mesh);
            surfaceGenerated_ = true;
        }
    } else {
        surfaceRenderer_->GenerateSurfaceFromCloud(*cloud_, genParams);
        surfaceGenerated_ = true;
    }
}

} // namespace scene
} // namespace workstation
