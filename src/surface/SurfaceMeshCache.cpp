#include "workstation/surface/SurfaceMeshCache.h"
#include "workstation/surface/SurfaceLog.h"

namespace workstation {
namespace surface {

CachedSurface* SurfaceMeshCache::GetOrCreate(uint32_t cloudID,
                                               pointcloud::PointCloud& cloud,
                                               const SurfaceGenerationParams& params) {
    auto it = cache_.find(cloudID);
    if (it != cache_.end() && it->second->isValid) {
        it->second->lastUsedFrame = currentFrame_;
        SLOG_INFO("Cache: HIT cloud=%u (reusing mesh, no rebuild)", cloudID);
        return it->second.get();
    }

    SLOG_INFO("Cache: MISS cloud=%u - generating surface mesh (cloud: %zu points)...",
              cloudID, cloud.PointCount());
    auto entry = std::make_unique<CachedSurface>();
    entry->mesh = generator_.Generate(cloud, params);
    entry->mesh.ComputeEdges();
    entry->isValid = !entry->mesh.IsEmpty();
    entry->lastUsedFrame = currentFrame_;
    lastStats_ = generator_.GetLastStats();

    const auto& st = lastStats_;
    SLOG_INFO("Cache: generated cloud=%u -> %zu verts / %zu tris / %zu edges "
              "(%.1f ms total: %.1f triangulate + %.1f normals + %.1f colors)",
              cloudID, st.vertexCount, st.triangleCount,
              static_cast<size_t>(entry->mesh.EdgeCount()) / 2,
              st.generationTimeMs, st.triangulationTimeMs,
              st.normalTimeMs, st.colorTimeMs);

    if (entry->isValid && allocator_) {
        entry->gpuBuffer.Initialize(*allocator_);
        entry->gpuBuffer.UploadMesh(entry->mesh);
        SLOG_INFO("Cache: uploaded cloud=%u to GPU (%.2f MB)",
                  cloudID, entry->gpuBuffer.GetVertexBufferSize() / (1024.0 * 1024.0));
    } else if (entry->isValid) {
        SLOG_WARN("Cache: no allocator - cloud=%u mesh stays CPU-only", cloudID);
    }

    uint32_t id = cloudID;
    auto* ptr = entry.get();
    cache_[id] = std::move(entry);
    return ptr;
}

CachedSurface* SurfaceMeshCache::Find(uint32_t cloudID) const {
    auto it = cache_.find(cloudID);
    if (it != cache_.end() && it->second->isValid) {
        return it->second.get();
    }
    return nullptr;
}

void SurfaceMeshCache::Evict(uint64_t maxAge) {
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (it->second->isValid &&
            (currentFrame_ - it->second->lastUsedFrame) > maxAge) {
            if (it->second->gpuBuffer.IsInitialized()) {
                it->second->gpuBuffer.Shutdown();
            }
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void SurfaceMeshCache::Clear() {
    for (auto& [id, entry] : cache_) {
        if (entry->gpuBuffer.IsInitialized()) {
            entry->gpuBuffer.Shutdown();
        }
    }
    cache_.clear();
}

void SurfaceMeshCache::Remove(uint32_t cloudID) {
    auto it = cache_.find(cloudID);
    if (it != cache_.end()) {
        if (it->second->gpuBuffer.IsInitialized()) {
            it->second->gpuBuffer.Shutdown();
        }
        cache_.erase(it);
    }
}

size_t SurfaceMeshCache::GetGPUMemoryUsage() const {
    size_t total = 0;
    for (const auto& [id, entry] : cache_) {
        if (entry->isValid) {
            total += entry->gpuBuffer.GetVertexBufferSize();
            total += entry->gpuBuffer.GetIndexBufferSize();
        }
    }
    return total;
}

} // namespace surface
} // namespace workstation
