#include "workstation/surface/SurfaceLODManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

namespace workstation {
namespace surface {

void SurfaceLODManager::GenerateLODs(const pointcloud::PointCloud& cloudIn,
                                       const SurfaceGenerationParams& params) {
    levels_.clear();
    needsRebuild_ = false;

    if (!cloudIn.Root()) return;

    auto* xyzChannel = cloudIn.Root()->channels().GetChannel(pointcloud::ChannelId::XYZ);
    if (!xyzChannel || xyzChannel->Count() == 0) return;

    const uint32_t totalPoints = static_cast<uint32_t>(xyzChannel->Count());

    auto& cloud = const_cast<pointcloud::PointCloud&>(cloudIn);

    levels_.resize(config_.lodCount);
    SurfaceMeshGenerator gen;

    for (uint32_t i = 0; i < config_.lodCount; ++i) {
        auto& level = levels_[i];
        float fraction = 1.0f / static_cast<float>(1u << i);
        level.maxPoints = static_cast<uint32_t>(config_.baseMaxPoints * fraction);
        if (level.maxPoints < 1000) level.maxPoints = 1000;
        if (level.maxPoints > totalPoints) level.maxPoints = totalPoints;
        // Hard safety cap regardless of config: DelaunayTriangulator is O(n^2)
        // and anything beyond ~20k points risks the same multi-minute hang
        // this LOD system previously reintroduced at its old 200k default.
        if (level.maxPoints > 20000) level.maxPoints = 20000;
        level.maxEdgeLength = params.maxEdgeLength * (1u << i);
        level.minScreenFraction = config_.minScreenFraction * static_cast<float>(1u << i);

        SurfaceGenerationParams lp = params;
        lp.maxPoints = level.maxPoints;
        lp.maxEdgeLength = level.maxEdgeLength;

        level.mesh = gen.Generate(cloud, lp);
        level.generated = !level.mesh.IsEmpty();

        fprintf(stderr, "[SurfaceLOD] LOD %u: maxPoints=%u edgeLen=%.1f mesh=%s verts=%u tris=%u\n",
                i, level.maxPoints, level.maxEdgeLength,
                level.generated ? "OK" : "EMPTY",
                level.mesh.VertexCount(), level.mesh.TriangleCount());
    }

    if (!levels_.empty() && levels_[0].generated) {
        bounds_ = levels_[0].mesh.GetBounds();
    } else {
        ComputeBounds();
    }

    fprintf(stderr, "[SurfaceLOD] Generated %u LOD levels, bounds: min(%.2f,%.2f,%.2f) max(%.2f,%.2f,%.2f)\n",
            static_cast<uint32_t>(levels_.size()),
            bounds_.minX, bounds_.minY, bounds_.minZ,
            bounds_.maxX, bounds_.maxY, bounds_.maxZ);
    fflush(stderr);
}

const SurfaceMesh* SurfaceLODManager::GetMesh(uint32_t lod) const {
    if (lod >= levels_.size()) return nullptr;
    if (!levels_[lod].generated) return nullptr;
    return &levels_[lod].mesh;
}

uint32_t SurfaceLODManager::SelectLOD(const renderer::Camera& camera) const {
    if (levels_.empty()) return 0;

    auto camPos = camera.GetPosition();
    double dx = (bounds_.maxX + bounds_.minX) * 0.5 - camPos.x;
    double dy = (bounds_.maxY + bounds_.minY) * 0.5 - camPos.y;
    double dz = (bounds_.maxZ + bounds_.minZ) * 0.5 - camPos.z;
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    double diag = std::sqrt(
        (bounds_.maxX - bounds_.minX) * (bounds_.maxX - bounds_.minX) +
        (bounds_.maxY - bounds_.minY) * (bounds_.maxY - bounds_.minY) +
        (bounds_.maxZ - bounds_.minZ) * (bounds_.maxZ - bounds_.minZ));

    if (diag <= 0.0 || dist <= 0.0) return 0;

    double fovRad = camera.GetFOV() * 3.14159265358979 / 180.0;
    double screenFraction = (diag / dist) / (2.0 * std::tan(fovRad * 0.5));

    uint32_t selected = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(levels_.size()); ++i) {
        if (screenFraction >= levels_[i].minScreenFraction) {
            selected = i;
        }
    }

    return selected;
}

bool SurfaceLODManager::IsVisible(const renderer::Camera& camera) const {
    if (levels_.empty()) return false;

    const auto& planes = camera.GetFrustumPlanes();
    return planes.TestAABB(bounds_);
}

void SurfaceLODManager::ComputeBounds() {
    bounds_ = {};
    bool found = false;
    for (const auto& level : levels_) {
        if (!level.generated) continue;
        const auto& b = level.mesh.GetBounds();
        if (!found) {
            bounds_ = b;
            found = true;
        } else {
            bounds_.minX = std::min(bounds_.minX, b.minX);
            bounds_.minY = std::min(bounds_.minY, b.minY);
            bounds_.minZ = std::min(bounds_.minZ, b.minZ);
            bounds_.maxX = std::max(bounds_.maxX, b.maxX);
            bounds_.maxY = std::max(bounds_.maxY, b.maxY);
            bounds_.maxZ = std::max(bounds_.maxZ, b.maxZ);
        }
    }
}

} // namespace surface
} // namespace workstation
