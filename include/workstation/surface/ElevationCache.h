#pragma once
#include "workstation/surface/ElevationGrid.h"
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/surface/SurfaceGPUBuffer.h"
#include "workstation/vulkan/VulkanAllocator.h"

#include <memory>
#include <unordered_map>
#include <cstdint>

namespace workstation {
namespace surface {

struct CachedElevation {
    ElevationGrid grid;
    SurfaceMesh mesh;
    SurfaceGPUBuffer gpuBuffer;
    ElevationGridParams params;
    uint64_t lastUsedFrame = 0;
    bool isValid = false;
};

class ElevationCache {
public:
    ElevationCache() = default;
    ~ElevationCache() = default;

    void Initialize();

    void SetCurrentFrame(uint64_t frame) { currentFrame_ = frame; }

    // Get or generate the elevation grid for a given cloud + resolution + source.
    CachedElevation* GetOrCreate(uint32_t cloudID,
                                  const pointcloud::PointCloud& cloud,
                                  const ElevationGridParams& params);

    // Find an existing cached entry without generating.
    CachedElevation* Find(uint32_t cloudID, uint32_t resolution,
                          ElevationSourceMode sourceMode) const;

    // Find the best available LOD for the given screen fraction.
    // Resolutions: 256 (coarse), 512, 1024, 2048 (fine).
    // screenFraction: higher = closer to camera.
    CachedElevation* FindBestLOD(uint32_t cloudID, float screenFraction,
                                  ElevationSourceMode sourceMode) const;

    // Get all available resolutions for a cloud + source.
    std::vector<uint32_t> GetAvailableResolutions(
        uint32_t cloudID, ElevationSourceMode sourceMode) const;

    void Evict(uint64_t maxAge = 600);
    void Clear();
    void Remove(uint32_t cloudID);

    uint32_t GetCacheSize() const {
        return static_cast<uint32_t>(cache_.size());
    }

    const ElevationGridStats& GetLastStats() const { return lastStats_; }

private:
    struct CacheKey {
        uint32_t cloudID;
        uint32_t resolution;
        ElevationSourceMode sourceMode;
        bool operator==(const CacheKey& o) const {
            return cloudID == o.cloudID && resolution == o.resolution
                && sourceMode == o.sourceMode;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& k) const {
            size_t h1 = std::hash<uint32_t>()(k.cloudID);
            size_t h2 = std::hash<uint32_t>()(k.resolution);
            size_t h3 = std::hash<uint32_t>()(
                static_cast<uint32_t>(k.sourceMode));
            return h1 ^ (h2 * 0x9e3779b9 + h3 * 0x9e3779b9
                         + (h1 << 6) + (h1 >> 2));
        }
    };

    bool initialized_ = false;
    uint64_t currentFrame_ = 0;
    ElevationGridStats lastStats_;
    std::unordered_map<CacheKey, std::unique_ptr<CachedElevation>,
                       CacheKeyHash> cache_;
};

} // namespace surface
} // namespace workstation
