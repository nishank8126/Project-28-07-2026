#pragma once
#include "workstation/pointcloud/PointStreamingManager.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>

namespace workstation { namespace pointcloud {

// ============================================================
// GPUResidencyManager - Manages GPU memory budget and LRU eviction
// ============================================================
class GPUResidencyManager {
public:
    GPUResidencyManager() = default;
    ~GPUResidencyManager() = default;

    // Initialize with Vulkan device and memory budget
    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkMemoryPropertyFlags memProperties,
                    uint64_t maxVRAMBytes);

    // Shutdown
    void Shutdown();

    // Register a tile's GPU memory usage
    void RegisterTile(uint64_t nodeKey, VkDeviceSize tileMemoryBytes);

    // Unregister a tile (evicted)
    void UnregisterTile(uint64_t nodeKey);

    // Update LRU tracking - mark tile as recently used
    void TileUsed(uint64_t nodeKey);

    // Check if tile is currently resident
    bool IsTileResident(uint64_t nodeKey) const;

    // Get memory usage stats
    struct Stats {
        uint64_t totalVRAMBudget;
        uint64_t vramUsedByTiles;
        uint64_t vramAvailable;
        uint32_t tilesResident;
        uint32_t tilesEvictedThisFrame;
    } GetStats() const;

    // Evict tiles to free memory (call after frame rendering)
    // Returns bytes freed
    uint64_t EvictTilesToBudget(uint64_t targetBytes);

    // Get list of resident tile keys (for potential manual management)
    std::vector<uint64_t> GetResidentTileKeys() const;

private:
    VkDevice gpuDevice_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkMemoryPropertyFlags memProperties_ = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    // VRAM budget
    uint64_t maxVRAMBytes_ = 4 * 1024 * 1024 * 1024;  // 4GB default

    // Track tile memory usage - nodeKey -> memory size
    std::unordered_map<uint64_t, VkDeviceSize> tileMemoryMap_;

    // LRU ordering - list of resident tile keys (front = oldest/LRU candidate)
    std::list<uint64_t> residentTilesLRU_;

    // Statistics
    std::atomic<uint64_t> vramUsedByTiles_{0};
    std::atomic<uint32_t> tilesResident_{0};
    std::atomic<uint32_t> tilesEvictedThisFrame_{0};
};

// ============================================================
// GPUResidencyManager Inline Methods
// ============================================================

inline bool GPUResidencyManager::IsTileResident(uint64_t nodeKey) const {
    return tileMemoryMap_.find(nodeKey) != tileMemoryMap_.end();
}

inline std::vector<uint64_t> GPUResidencyManager::GetResidentTileKeys() const {
    std::vector<uint64_t> keys;
    std::lock_guard<std::mutex> lock;  // Would need mutex, simplified
    for (const auto& key : residentTilesLRU_) {
        keys.push_back(key);
    }
    return keys;
}

inline void GPUResidencyManager::TileUsed(uint64_t nodeKey) {
    // Move tile to end of LRU list (most recently used)
    auto it = std::find(residentTilesLRU_.begin(), residentTilesLRU_.end(), nodeKey);
    if (it != residentTilesLRU_.end()) {
        residentTilesLRU_.splice(residentTilesLRU_.end(), residentTilesLRU_, it);
    }
}

// ============================================================
// GPUResidencyManager Implementation
// ============================================================

bool GPUResidencyManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                                       VkMemoryPropertyFlags memProperties,
                                       uint64_t maxVRAMBytes) {
    if (!device) {
        fprintf(stderr, "[GPU Residency] Invalid device\n");
        return false;
    }

    gpuDevice_ = device;
    physicalDevice_ = physicalDevice;
    memProperties_ = memProperties;
    maxVRAMBytes_ = maxVRAMBytes;

    fprintf(stderr, "[GPU Residency] Initialized: %u MB VRAM budget\n",
            maxVRAMBytes_ / (1024 * 1024));
    return true;
}

void GPUResidencyManager::Shutdown() {
    tileMemoryMap_.clear();
    residentTilesLRU_.clear();
}

void GPUResidencyManager::RegisterTile(uint64_t nodeKey, VkDeviceSize tileMemoryBytes) {
    std::lock_guard<std::mutex> lock(mutex_);  // Would need to add mutex member
    tileMemoryMap_[nodeKey] = tileMemoryBytes;

    // Add to LRU list if not already present
    if (std::find(residentTilesLRU_.begin(), residentTilesLRU_.end(), nodeKey) ==
        residentTilesLRU_.end()) {
        residentTilesLRU_.push_back(nodeKey);
        tilesResident_++;
    }

    // Update VRAM usage
    vramUsedByTiles_ += tileMemoryBytes;
}

void GPUResidencyManager::UnregisterTile(uint64_t nodeKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto memIt = tileMemoryMap_.find(nodeKey);
    if (memIt != tileMemoryMap_.end()) {
        vramUsedByTiles_ -= memIt->second;
        tileMemoryMap_.erase(memIt);
    }

    // Remove from LRU list
    residentTilesLRU_.remove(nodeKey);
    tilesResident_--;
}

void GPUResidencyManager::TileUsed(uint64_t nodeKey) {
    auto it = std::find(residentTilesLRU_.begin(), residentTilesLRU_.end(), nodeKey);
    if (it != residentTilesLRU_.end()) {
        // Move to back (most recently used)
        residentTilesLRU_.splice(residentTilesLRU_.end(), residentTilesLRU_, it);
    }
}

GPUResidencyManager::Stats GPUResidencyManager::GetStats() const {
    Stats stats;
    stats.totalVRAMBudget = maxVRAMBytes_;
    stats.vramUsedByTiles = vramUsedByTiles_.load();
    stats.vramAvailable = maxVRAMBytes_ - vramUsedByTiles_.load();
    stats.tilesResident = tilesResident_.load();
    stats.tilesEvictedThisFrame = tilesEvictedThisFrame_.load();
    return stats;
}

uint64_t GPUResidencyManager::EvictTilesToBudget(uint64_t targetBytes) {
    uint64_t bytesFreed = 0;
    uint64_t target = targetBytes ? targetBytes : maxVRAMBytes_;

    // Evict from front of LRU list (oldest tiles first)
    while (vramUsedByTiles_.load() > target && !residentTilesLRU_.empty()) {
        uint64_t nodeKey = residentTilesLRU_.front();
        residentTilesLRU_.pop_front();

        auto memIt = tileMemoryMap_.find(nodeKey);
        if (memIt != tileMemoryMap_.end()) {
            vramUsedByTiles_ -= memIt->second;
            tileMemoryMap_.erase(memIt);
            tilesEvictedThisFrame_++;
            bytesFreed += memIt->second;
        }
    }

    // Update resident count
    tilesResident_ = static_cast<uint32_t>(residentTilesLRU_.size());

    return bytesFreed;
}

} // namespace workstation::pointcloud