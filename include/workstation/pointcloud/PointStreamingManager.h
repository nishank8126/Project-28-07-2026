#pragma once
#include "workstation/pointcloud/OctreeTile.h"
#include <vulkan/vulkan.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>

namespace workstation { namespace pointcloud {

// ============================================================
// StreamTileInfo - Information about a tile to be loaded/unloaded
// ============================================================
struct StreamTileInfo {
    uint64_t nodeKey;           // Unique tile identifier
    BBox16 bounds;              // Bounding box for culling
    int32_t diskOffset;         // File offset (-1 if not on disk)
    uint32_t pointCount;        // Number of points in tile
    uint32_t lodLevel;          // Current LOD level
    GPUBufState gpuState;       // Current GPU state
    double lastUsedFrame;       // Frame number when last used
    bool bIsVisible;            // Is tile visible in current frame

    // Comparison for LRU ordering (lower lastUsedFrame = older = LRU candidate)
    bool operator<(const StreamTileInfo& other) const {
        return lastUsedFrame > other.lastUsedFrame;  // Reverse: higher = older
    }
};

// ============================================================
// StreamingConfig - Configuration for the streaming manager
// ============================================================
struct StreamingConfig {
    uint32_t maxGPUMemoryBytes = 4 * 1024 * 1024 * 1024;  // 4GB default
    uint32_t maxCPUMemoryBytes = 512 * 1024 * 1024;       // 512MB CPU cache
    uint32_t maxTilesResident = 500;                        // Max tiles in GPU
    uint32_t tileBudgetBytes = 256 * 1024 * 1024;         // 256MB per frame
    float tileVisibleThreshold = 0.1f;         // Min visibility fraction to keep tile
    uint32_t streamingThreadPriority = 5;                  // Thread priority
    bool enableBackgroundIO = true;                        // Async disk reads
    bool enableGPUPreload = true;                         // Preload visible tiles
};

// ============================================================
// PointStreamingManager - Manages out-of-core point cloud streaming
//
// Responsibilities:
// - Load visible tiles from disk on background thread
// - Unload invisible tiles
// - Manage GPU memory budget (LRU eviction)
// - Queue GPU uploads to render thread
// - Track tile usage and memory statistics
// ============================================================
class PointStreamingManager {
public:
    PointStreamingManager();
    ~PointStreamingManager();

    // Initialize streaming manager with Vulkan devices
    bool Initialize(VkDevice device, VkQueue uploadQueue,
                    uint32_t maxGPUMemoryBytes = 4 * 1024 * 1024 * 1024);

    // Shutdown - finish background thread
    void Shutdown();

    // Update - called each frame from render thread
    // Returns true if any tiles were loaded/unloaded/evicted
    bool Update(uint64_t frameNumber, const std::vector<StreamTileInfo>& visibleTiles,
                const std::vector<StreamTileInfo>& allTiles);

    // Get statistics
    struct Stats {
        uint32_t tilesLoadedThisFrame = 0;
        uint32_t tilesUnloadedThisFrame = 0;
        uint32_t tilesEvictedThisFrame = 0;
        uint32_t tilesResident = 0;
        uint32_t totalTilesInDatabase = 0;
        uint64_t gpuMemoryUsed = 0;
        uint64_t gpuMemoryBudget = 0;
        double streamingTimeMs = 0.0;
        uint32_t framesSinceLastLoad = 0;
    } GetStats() const;

    // Get currently resident tile keys
    std::vector<uint64_t> GetResidentTileKeys() const;

    // Get tile info by node key
    const StreamTileInfo* GetTileInfo(uint64_t nodeKey) const;

    // Mark tile as used (updates LRU tracking)
    void TileUsed(uint64_t nodeKey);

    // Force unload all tiles
    void UnloadAllTiles();

    // Load a specific tile by node key (synchronous, for fast startup)
    bool LoadTile(uint64_t nodeKey);

private:
    // Background worker thread function
    void StreamingWorkerThread();

    // Load a single tile from disk
    bool LoadTileFromDisk(StreamTileInfo& tileInfo);

    // Upload tile to GPU
    bool UploadTileToGPU(StreamTileInfo& tileInfo);

    // Evict least recently used tile(s) to stay within budget
    void EvictTiles(uint32_t bytesToFree);

    // Vulkan resources
    VkDevice gpuDevice_;
    VkQueue uploadQueue_;

    // Tile database - maps node key to tile info
    std::unordered_map<uint64_t, StreamTileInfo> tileDatabase_;

    // Currently resident tiles (LRU-ordered list)
    std::list<StreamTileInfo> residentTiles_;  // LRU: front = oldest, back = newest

    // Tiles waiting for upload to GPU
    std::queue<StreamTileInfo> uploadQueue_;

    // Synchronization
    std::mutex mutex_;
    std::condition_variable cvDataAvailable;
    std::conditionVariable cvUploadComplete;

    // Background thread
    std::thread workerThread_;
    std::atomic<bool> running_{false};

    // Statistics
    std::atomic<uint32_t> tilesLoadedThisFrame_{0};
    std::atomic<uint32_t> tilesUnloadedThisFrame_{0};
    std::atomic<uint32_t> tilesEvictedThisFrame_{0};
    std::atomic<uint64_t> gpuMemoryUsed_{0};
    std::atomic<uint64_t> gpuMemoryBudget_;
    std::atomic<double> streamingTimeMs_{0.0};
    std::atomic<uint32_t> framesSinceLastLoad_{0};

    // Configuration
    StreamingConfig config_;
};

// ============================================================
// PointStreamingManager - Inline methods
// ============================================================

inline const StreamTileInfo* PointStreamingManager::GetTileInfo(uint64_t nodeKey) const {
    auto it = tileDatabase_.find(nodeKey);
    if (it != tileDatabase_.end()) {
        return &it->second;
    }
    return nullptr;
}

inline std::vector<uint64_t> PointStreamingManager::GetResidentTileKeys() const {
    std::vector<uint64_t> keys;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& tile : residentTiles_) {
        keys.push_back(tile.nodeKey);
    }
    return keys;
}

inline void PointStreamingManager::TileUsed(uint64_t nodeKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Move tile to end of LRU list (most recently used)
    for (auto it = residentTiles_.begin(); it != residentTiles_.end(); ++it) {
        if (it->nodeKey == nodeKey) {
            residentTiles_.splice(residentTiles_.end(), residentTiles_, it);
            it->lastUsedFrame = static_cast<double>(framesSinceLastLoad_.load());
            break;
        }
    }
}

inline void PointStreamingManager::UnloadAllTiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    // Unload all resident tiles
    residentTiles_.clear();
    tileDatabase_.clear();
    gpuMemoryUsed_ = 0;
}

} // namespace workstation::pointcloud