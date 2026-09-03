#pragma once
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/surface/SurfaceMeshGenerator.h"
#include "workstation/surface/SurfaceGPUBuffer.h"
#include "workstation/pointcloud/PointCloud.h"

#include <memory>
#include <unordered_map>
#include <cstdint>

namespace workstation {
namespace surface {

struct CachedSurface {
    SurfaceMesh mesh;
    SurfaceGPUBuffer gpuBuffer;
    uint64_t lastUsedFrame = 0;
    bool isValid = false;
};

class SurfaceMeshCache {
public:
    SurfaceMeshCache() = default;
    ~SurfaceMeshCache() = default;

    void Initialize(vulkan::VulkanAllocator* allocator) { allocator_ = allocator; }

    // Frame counter used for LRU eviction; the Renderer calls this once per
    // frame so cached surfaces do not get rebuilt while the camera moves.
    void SetCurrentFrame(uint64_t frame) { currentFrame_ = frame; }

    CachedSurface* GetOrCreate(uint32_t cloudID,
                                pointcloud::PointCloud& cloud,
                                const SurfaceGenerationParams& params = {});

    CachedSurface* Find(uint32_t cloudID) const;
    void Evict(uint64_t maxAge = 300);
    void Clear();
    void Remove(uint32_t cloudID);

    uint32_t GetCacheSize() const { return static_cast<uint32_t>(cache_.size()); }
    size_t GetGPUMemoryUsage() const;

    // Phase 12 metrics from the most recent generation performed by this
    // cache (unchanged on cache hits, so it always describes the last real
    // Generate() run, not a reuse).
    const SurfaceGenerationStats& GetLastStats() const { return lastStats_; }

private:
    vulkan::VulkanAllocator* allocator_ = nullptr;
    uint64_t currentFrame_ = 0;
    SurfaceGenerationStats lastStats_;
    std::unordered_map<uint32_t, std::unique_ptr<CachedSurface>> cache_;
    SurfaceMeshGenerator generator_;
};

} // namespace surface
} // namespace workstation
