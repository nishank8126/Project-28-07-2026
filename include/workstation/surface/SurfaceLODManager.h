#pragma once
#include "workstation/spatial/BoundingBox.h"
#include "workstation/spatial/Frustum.h"
#include "workstation/renderer/Camera.h"
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/surface/SurfaceMeshGenerator.h"

#include <vector>
#include <cstdint>

namespace workstation {
namespace surface {

struct SurfaceLODLevel {
    uint32_t maxPoints = 0;
    double maxEdgeLength = 0.0;
    float minScreenFraction = 0.0f;
    SurfaceMesh mesh;
    bool generated = false;
};

struct SurfaceLODConfig {
    uint32_t lodCount = 3;
    // DelaunayTriangulator::Triangulate is O(n^2) (see the comment on
    // SurfaceGenerationParams::maxPoints) - 200,000 points here previously
    // meant LOD0 alone took on the order of a minute, appearing to hang the
    // app. Match the same conservative cap used everywhere else that feeds
    // the triangulator.
    uint32_t baseMaxPoints = 15000;
    float lodBias = 1.0f;
    float minScreenFraction = 0.005f;
    bool frustumCulling = true;
};

class SurfaceLODManager {
public:
    SurfaceLODManager() = default;

    void SetConfig(const SurfaceLODConfig& config) { config_ = config; }
    const SurfaceLODConfig& GetConfig() const { return config_; }

    void GenerateLODs(const pointcloud::PointCloud& cloud,
                       const SurfaceGenerationParams& params);

    void Invalidate() { levels_.clear(); needsRebuild_ = true; }

    bool IsEmpty() const { return levels_.empty(); }
    uint32_t GetLODCount() const { return static_cast<uint32_t>(levels_.size()); }

    const SurfaceMesh* GetMesh(uint32_t lod) const;

    uint32_t SelectLOD(const renderer::Camera& camera) const;

    bool IsVisible(const renderer::Camera& camera) const;

    const spatial::BoundingBox& GetBounds() const { return bounds_; }

private:
    void ComputeBounds();

    SurfaceLODConfig config_;
    std::vector<SurfaceLODLevel> levels_;
    spatial::BoundingBox bounds_ = {};
    bool needsRebuild_ = true;
};

} // namespace surface
} // namespace workstation
