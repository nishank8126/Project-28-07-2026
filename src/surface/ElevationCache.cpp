#include "workstation/surface/ElevationCache.h"
#include "workstation/surface/SurfaceLog.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"

namespace workstation {
namespace surface {

void ElevationCache::Initialize() {
    if (initialized_) return;
    initialized_ = true;
    SLOG_INFO("ElevationCache: initialized");
}

CachedElevation* ElevationCache::GetOrCreate(
    uint32_t cloudID,
    const pointcloud::PointCloud& cloud,
    const ElevationGridParams& params) {

    CacheKey key{cloudID, params.resolution, params.sourceMode};
    auto it = cache_.find(key);
    if (it != cache_.end() && it->second->isValid) {
        it->second->lastUsedFrame = currentFrame_;
        SLOG_INFO("ElevationCache: HIT cloud=%u res=%u source=%d (reuse)",
                  cloudID, params.resolution,
                  static_cast<int>(params.sourceMode));
        return it->second.get();
    }

    const char* srcName = params.sourceMode == ElevationSourceMode::GroundOnly  ? "DTM" :
                          params.sourceMode == ElevationSourceMode::HighestReturn ? "DSM" :
                          params.sourceMode == ElevationSourceMode::UserClass ? "USR" : "ALL";
    SLOG_INFO("ElevationCache: MISS cloud=%u res=%u source=%s - generating...",
              cloudID, params.resolution, srcName);

    auto entry = std::make_unique<CachedElevation>();
    entry->params = params;

    entry->grid.Generate(cloud, params);
    entry->mesh = entry->grid.CreateMesh();
    entry->isValid = !entry->mesh.IsEmpty();
    entry->lastUsedFrame = currentFrame_;

    lastStats_ = entry->grid.GetStats();

    if (entry->isValid) {
        entry->gpuBuffer.Initialize(vulkan::VulkanAllocator::Get());
        bool uploaded = entry->gpuBuffer.UploadMesh(entry->mesh);
        if (uploaded) {
            SLOG_INFO("ElevationCache: uploaded cloud=%u res=%u source=%s (%.2f MB)",
                      cloudID, params.resolution, srcName,
                      (entry->gpuBuffer.GetVertexBufferSize() +
                       entry->gpuBuffer.GetIndexBufferSize()) / (1024.0*1024.0));
        } else {
            SLOG_WARN("ElevationCache: GPU upload FAILED cloud=%u res=%u source=%s",
                      cloudID, params.resolution, srcName);
        }
    }

    auto* ptr = entry.get();
    cache_[key] = std::move(entry);
    return ptr;
}

CachedElevation* ElevationCache::Find(uint32_t cloudID, uint32_t resolution,
                                       ElevationSourceMode sourceMode) const {
    CacheKey key{cloudID, resolution, sourceMode};
    auto it = cache_.find(key);
    if (it != cache_.end() && it->second->isValid) {
        return it->second.get();
    }
    return nullptr;
}

CachedElevation* ElevationCache::FindBestLOD(
    uint32_t cloudID, float screenFraction,
    ElevationSourceMode sourceMode) const {

    // Resolutions from finest to coarsest
    static const uint32_t kResolutions[] = {2048, 1024, 512, 256};
    // Minimum screen fraction to use each resolution
    static const float kMinFractions[] = {0.1f, 0.02f, 0.005f, 0.001f};

    // Pick finest resolution that matches the screen fraction
    for (int i = 0; i < 4; ++i) {
        if (screenFraction >= kMinFractions[i]) {
            auto* e = Find(cloudID, kResolutions[i], sourceMode);
            if (e) return e;
        }
    }

    // Fallback: return coarsest available
    for (int i = 3; i >= 0; --i) {
        auto* e = Find(cloudID, kResolutions[i], sourceMode);
        if (e) return e;
    }
    return nullptr;
}

std::vector<uint32_t> ElevationCache::GetAvailableResolutions(
    uint32_t cloudID, ElevationSourceMode sourceMode) const {
    std::vector<uint32_t> result;
    static const uint32_t kAllRes[] = {256, 512, 1024, 2048};
    for (uint32_t r : kAllRes) {
        if (Find(cloudID, r, sourceMode)) {
            result.push_back(r);
        }
    }
    return result;
}

void ElevationCache::Evict(uint64_t maxAge) {
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (it->second->isValid &&
            (currentFrame_ - it->second->lastUsedFrame) > maxAge) {
            if (it->second->gpuBuffer.IsInitialized()) {
                it->second->gpuBuffer.Shutdown();
            }
            SLOG_INFO("ElevationCache: evicted cloud=%u res=%u source=%d",
                      it->first.cloudID, it->first.resolution,
                      static_cast<int>(it->first.sourceMode));
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void ElevationCache::Clear() {
    for (auto& [key, entry] : cache_) {
        if (entry->gpuBuffer.IsInitialized()) {
            entry->gpuBuffer.Shutdown();
        }
    }
    cache_.clear();
    SLOG_INFO("ElevationCache: cleared all entries");
}

void ElevationCache::Remove(uint32_t cloudID) {
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (it->first.cloudID == cloudID) {
            if (it->second->gpuBuffer.IsInitialized()) {
                it->second->gpuBuffer.Shutdown();
            }
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace surface
} // namespace workstation
