#include "workstation/pointcloud/PointStreamingManager.h"
#include "workstation/pointcloud/OctreeTile.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <thread>
#include <sys/stat.h>

namespace workstation { namespace pointcloud {

// ============================================================
// PointStreamingManager Implementation
// ============================================================

PointStreamingManager::PointStreamingManager()
    : gpuDevice_(VK_NULL_HANDLE),
      uploadQueue_(VK_NULL_HANDLE),
      gpuMemoryBudget_(4 * 1024 * 1024 * 1024) {
}

PointStreamingManager::~PointStreamingManager() {
    Shutdown();
}

bool PointStreamingManager::Initialize(VkDevice device, VkQueue uploadQueue,
                                       uint32_t maxGPUMemoryBytes) {
    if (!device || !uploadQueue) {
        fprintf(stderr, "[Streaming] Invalid device or queue\n");
        return false;
    }

    gpuDevice_ = device;
    uploadQueue_ = uploadQueue;
    gpuMemoryBudget_ = maxGPUMemoryBytes;

    running_ = true;
    workerThread_ = std::thread(&PointStreamingManager::StreamingWorkerThread, this);

    fprintf(stderr, "[Streaming] Manager initialized: %u MB GPU budget\n",
            maxGPUMemoryBytes / (1024 * 1024));
    return true;
}

void PointStreamingManager::Shutdown() {
    if (running_) {
        running_ = false;
        cvDataAvailable.notify_all();
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }
}

bool PointStreamingManager::Update(uint64_t frameNumber,
                                   const std::vector<StreamTileInfo>& visibleTiles,
                                   const std::vector<StreamTileInfo>& allTiles) {
    auto frameStart = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);

    tilesLoadedThisFrame_ = 0;
    tilesUnloadedThisFrame_ = 0;
    tilesEvictedThisFrame_ = 0;

    // 1. Mark visible tiles as used (update LRU tracking)
    for (const auto& tile : visibleTiles) {
        TileUsed(tile.nodeKey);
        tilesLoadedThisFrame_++;  // Count as "accessed this frame"
    }

    // 2. Update lastUsedFrame for all tiles
    double currentFrameTime = static_cast<double>(frameNumber);
    for (auto& [key, info] : tileDatabase_) {
        info.lastUsedFrame = currentFrameTime;
    }

    // 3. Process upload queue - move tiles from CPU uploading to GPU resident
    while (!uploadQueue_.empty()) {
        StreamTileInfo info = uploadQueue_.front();
        uploadQueue_.pop();

        // Find the tile in the database and update its state
        auto it = tileDatabase_.find(info.nodeKey);
        if (it != tileDatabase_.end()) {
            it->second.gpuState = GPUBufState::GPUResident;
            it->second.lastUsedFrame = currentFrameTime;

            // Add to resident LRU list (move to back = most recently used)
            residentTiles_.splice(residentTiles_.end(), residentTiles_, 
                                  std::find(residentTiles_.begin(), residentTiles_.end(), info));
            if (it != tileDatabase_.end()) {
                it->second = info;  // Update the database entry
            }
            tilesLoadedThisFrame_++;
        }
    }

    // 4. Evict tiles if we exceed GPU memory budget
    uint64_t currentGPUUsage = 0;
    for (const auto& [key, info] : tileDatabase_) {
        if (info.gpuState == GPUBufState::GPUResident) {
            currentGPUUsage += sizeof(OctreeTile) + (22 * info.pointCount);
        }
    }
    gpuMemoryUsed_ = currentGPUUsage;

    // Check if we need to evict
    while (currentGPUUsage > gpuMemoryBudget_ && !residentTiles_.empty()) {
        // Find the LRU tile (front of the list = oldest)
        StreamTileInfo& lruTile = residentTiles_.front();

        // Evict: move to evicted state, remove from resident list
        lruTile.gpuState = GPUBufState::Evicted;
        residentTiles_.pop_front();

        // Remove from database
        tileDatabase_.erase(lruTile.nodeKey);

        // Free GPU buffer if allocated
        // (In a full implementation, would call vkBufferMemoryBarrier + vkFreeMemory)

        currentGPUUsage -= sizeof(OctreeTile) + (22 * lruTile.pointCount);
        tilesEvictedThisFrame_++;
    }

    // 5. Queue GPU uploads for tiles that are visible but not yet resident
    for (const auto& tileInfo : visibleTiles) {
        auto it = tileDatabase_.find(tileInfo.nodeKey);
        if (it == tileDatabase_.end()) {
            // New tile - needs to be loaded
            tilesLoadedThisFrame_++;
            // Queue for loading (will be picked up by background thread)
            StreamTileInfo newTile = tileInfo;
            newTile.gpuState = GPUBufState::CPUUploading;
            uploadQueue_.push(newTile);
        } else if (it->second.gpuState == GPUBufState::NotLoaded ||
                   it->second.gpuState == GPUBufState::Evicted) {
            // Tile exists but not on GPU - queue upload
            tilesLoadedThisFrame_++;
            StreamTileInfo& dbTile = it->second;
            dbTile.gpuState = GPUBufState::CPUUploading;
            dbTile.lastUsedFrame = currentFrameTime;
            residentTiles_.splice(residentTiles_.end(), residentTiles_,
                                std::find(residentTiles_.begin(), residentTiles_.end(), dbTile));
            uploadQueue_.push(dbTile);
        }
    }

    // 6. Unload invisible tiles (those not in visible set and not recently used)
    for (auto it = residentTiles_.begin(); it != residentTiles_.end(); ) {
        bool isVisible = false;
        for (const auto& vTile : visibleTiles) {
            if (it->nodeKey == vTile.nodeKey) {
                isVisible = true;
                break;
            }
        }

        if (!isVisible) {
            // Check if tile was used recently (within last few frames)
            bool recentlyUsed = (frameNumber - it->lastUsedFrame) < 3;
            if (!recentlyUsed) {
                // Unload this tile
                it->gpuState = GPUBufState::Evicted;
                it = residentTiles_.erase(it);
                tilesUnloadedThisFrame_++;
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    auto frameEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = frameEnd - frameStart;
    streamingTimeMs_ = elapsed.count();

    framesSinceLastLoad_++;
    return true;
}

std::vector<uint64_t> PointStreamingManager::GetResidentTileKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint64_t> keys;
    for (const auto& tile : residentTiles_) {
        keys.push_back(tile.nodeKey);
    }
    return keys;
}

const StreamTileInfo* PointStreamingManager::GetTileInfo(uint64_t nodeKey) const {
    auto it = tileDatabase_.find(nodeKey);
    if (it != tileDatabase_.end()) {
        return &it->second;
    }
    return nullptr;
}

void PointStreamingManager::TileUsed(uint64_t nodeKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = residentTiles_.begin(); it != residentTiles_.end(); ++it) {
        if (it->nodeKey == nodeKey) {
            residentTiles_.splice(residentTiles_.end(), residentTiles_, it);
            it->lastUsedFrame = static_cast<double>(framesSinceLastLoad_.load());
            break;
        }
    }
}

void PointStreamingManager::UnloadAllTiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    residentTiles_.clear();
    tileDatabase_.clear();
    gpuMemoryUsed_ = 0;
    uploadQueue_ = std::queue<StreamTileInfo>();
}

uint32_t PointStreamingManager::GetStats::GettilesLoadedThisFrame() const {
    return tilesLoadedThisFrame.load();
}

uint32_t PointStreamingManager::GetStats::GettilesUnloadedThisFrame() const {
    return tilesUnloadedThisFrame.load();
}

uint32_t PointStreamingManager::GetStats::GettilesEvictedThisFrame() const {
    return tilesEvictedThisFrame.load();
}

uint32_t PointStreamingManager::GetStats::GettilesResident() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(residentTiles_.size());
}

uint64_t PointStreamingManager::GetStats::GetgpuMemoryUsed() const {
    return gpuMemoryUsed_.load();
}

uint64_t PointStreamingManager::GetStats::GetgpuMemoryBudget() const {
    return gpuMemoryBudget_.load();
}

double PointStreamingManager::GetStats::GetstreamingTimeMs() const {
    return streamingTimeMs_.load();
}

uint32_t PointStreamingManager::GetStats::GetframesSinceLastLoad() const {
    return framesSinceLastLoad.load();
}

// ============================================================
// Background Worker Thread
// ============================================================
void PointStreamingManager::StreamingWorkerThread() {
    fprintf(stderr, "[Streaming] Worker thread started\n");

    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cvDataAvailable.wait(lock, [this] { return !uploadQueue_.empty() || !running_; });

        if (!running_) break;

        // Process upload queue items
        while (!uploadQueue_.empty() && running_) {
            StreamTileInfo tileInfo = uploadQueue_.front();
            uploadQueue_.pop();
            lock.unlock();

            // Load tile from disk
            bool loaded = LoadTileFromDisk(tileInfo);

            // Upload to GPU
            if (loaded) {
                UploadTileToGPU(tileInfo);
            }

            lock.lock();
            tilesLoadedThisFrame_++;
        }

        cvUploadComplete.notify_all();
        lock.unlock();

        // Small yield to prevent busy-waiting
        std::this_thread::yield();
    }

    fprintf(stderr, "[Streaming] Worker thread terminated\n");
}

bool PointStreamingManager::LoadTileFromDisk(StreamTileInfo& tileInfo) {
    // In a full implementation, this would:
    // 1. Open the LAS/LAZ file
    // 2. Seek to the tile's disk offset
    // 3. Read the OctreeTile header + point data
    // 4. Decompress if LAZ
    // 5. Decode point attributes (XYZ, RGB, Intensity, Classification, ReturnNum)

    // For now, simulate the operation
    if (tileInfo.diskOffset < 0) {
        // No disk data - this is a newly created tile
        // Initialize with default values
        tileInfo.pointCount = 0;
        tileInfo.bounds = BBox16{0, 0, 0, 0, 0, 0};
        tileInfo.gpuState = GPUBufState::GPUResident;  // Mark as loaded (simulated)
        return true;
    }

    // Simulate file I/O
    std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Simulate disk read latency

    // Read tile from disk (placeholder - real implementation would read binary tile)
    // OctreeTile tile;
    // std::ifstream file(tileFilePath, std::ios::binary);
    // file.seekg(tileInfo.diskOffset);
    // file.read(reinterpret_cast<char*>(&tile), sizeof(OctreeTile) + 22 * tile.pointCount);

    // For now, mark as loaded and return
    tileInfo.gpuState = GPUBufState::GPUResident;
    return true;
}

bool PointStreamingManager::UploadTileToGPU(StreamTileInfo& tileInfo) {
    // In a full implementation, this would:
    // 1. Create a VkBuffer for the point data
    // 2. Use vmaMapMemory to map the buffer
    // 3. Copy point data from CPU buffer to mapped memory
    // 4. vmaUnmapMemory
    // 5. Insert memory barrier for GPU access

    // For now, simulate the upload
    std::this_thread::sleep_for(std::chrono::milliseconds(5));  // Simulate GPU upload time

    // Update tile state
    tileInfo.gpuState = GPUBufState::GPUResident;

    return true;
}

// ============================================================
// Statistics accessors
// ============================================================

PointStreamingManager::Stats PointStreamingManager::GetStats() const {
    Stats stats;
    stats.tilesLoadedThisFrame = tilesLoadedThisFrame.load();
    stats.tilesUnloadedThisFrame = tilesUnloadedThisFrame.load();
    stats.tilesEvictedThisFrame = tilesEvictedThisFrame.load();
    stats.tilesResident = GetStats().tilesResident;  // Will use lock internally
    stats.gpuMemoryUsed = gpuMemoryUsed_.load();
    stats.gpuMemoryBudget = gpuMemoryBudget_.load();
    stats.streamingTimeMs = streamingTimeMs_.load();
    stats.framesSinceLastLoad = framesSinceLastLoad.load();
    return stats;
}

} // namespace workstation::pointcloud