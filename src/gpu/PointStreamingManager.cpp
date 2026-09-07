#include "workstation/gpu/PointStreamingManager.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/VoxelNode.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace workstation {
namespace gpu {

PointStreamingManager::~PointStreamingManager() { Shutdown(); }

bool PointStreamingManager::Initialize(GPUBufferManager* bufferManager,
                                        uint64_t cpuCacheLimit,
                                        uint64_t gpuMemoryLimit) {
    bufferManager_ = bufferManager;
    cpuCacheLimit_ = cpuCacheLimit;
    gpuMemoryLimit_ = gpuMemoryLimit;

    stopDecodeThread_ = false;
    decodeThread_ = std::thread(&PointStreamingManager::DecodeThreadFunc, this);

    return true;
}

void PointStreamingManager::Shutdown() {
    stopDecodeThread_ = true;
    decodeCondition_.notify_all();
    if (decodeThread_.joinable()) {
        decodeThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(cpuCacheMutex_);
        cpuCache_.clear();
        cpuCacheUsed_ = 0;
        nodeKeyMap_.clear();
        cpuLruList_.clear();
        cpuLruMap_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(gpuResidencyMutex_);
        gpuResidentNodes_.clear();
        gpuMemoryUsed_ = 0;
        gpuLruList_.clear();
        gpuLruMap_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        while (!requestQueue_.empty()) requestQueue_.pop();
        activeRequests_.clear();
    }

    bufferManager_ = nullptr;
    activeCloud_ = nullptr;
}

void PointStreamingManager::SetActiveCloud(pointcloud::PointCloud* cloud) {
    CancelAllRequests();
    activeCloud_ = cloud;
    BuildNodeKeyMap(cloud);
}

void PointStreamingManager::BuildNodeKeyMap(pointcloud::PointCloud* cloud) {
    std::lock_guard<std::mutex> lock(cpuCacheMutex_);
    nodeKeyMap_.clear();
    if (!cloud || !cloud->Root()) return;

    uint64_t nextKey = 0;
    std::function<void(pointcloud::PointCloudNode*)> walk = [&](pointcloud::PointCloudNode* n) {
        if (!n) return;
        nodeKeyMap_[nextKey++] = n;
        if (auto* v = dynamic_cast<pointcloud::VoxelNode*>(n)) {
            for (size_t i = 0; i < v->ChildCount(); ++i) {
                walk(v->Child(i));
            }
        }
    };
    walk(cloud->Root());

    fprintf(stderr, "[Streaming] Built nodeKey map: %zu nodes from cloud '%s'\n",
            nodeKeyMap_.size(), cloud->Name());
    fflush(stderr);
}

void PointStreamingManager::RequestNode(uint64_t nodeKey, float priority,
                                         uint64_t pointCount, float screenSpaceError) {
    std::lock_guard<std::mutex> lock(requestMutex_);

    auto it = activeRequests_.find(nodeKey);
    if (it != activeRequests_.end()) {
        if (it->second.state == NodeStreamingState::GpuResident ||
            it->second.state == NodeStreamingState::CpuReady) {
            return;
        }
        it->second.priority = priority;
        it->second.screenSpaceError = screenSpaceError;
        return;
    }

    StreamingRequest request;
    request.nodeKey = nodeKey;
    request.priority = priority;
    request.pointCount = pointCount;
    request.screenSpaceError = screenSpaceError;
    request.state = NodeStreamingState::Requested;

    activeRequests_[nodeKey] = request;
    requestQueue_.push(request);

    {
        std::lock_guard<std::mutex> lock(decodeQueueMutex_);
        decodeQueue_.push_back(request);
    }
    decodeCondition_.notify_one();
}

void PointStreamingManager::CancelRequest(uint64_t nodeKey) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    activeRequests_.erase(nodeKey);
}

void PointStreamingManager::CancelAllRequests() {
    std::lock_guard<std::mutex> lock(requestMutex_);
    while (!requestQueue_.empty()) requestQueue_.pop();
    activeRequests_.clear();

    std::lock_guard<std::mutex> decodeLock(decodeQueueMutex_);
    decodeQueue_.clear();
}

void PointStreamingManager::Update(uint64_t currentFrame) {
    currentFrame_ = currentFrame;
    uploadedThisFrame_ = 0;

    UploadCpuReadyNodes();

    EvictCpuCache();
    EvictGpuResidency();

    UpdateDebugStats();
}

void PointStreamingManager::DecodeThreadFunc() {
    while (!stopDecodeThread_.load()) {
        std::vector<StreamingRequest> batch;

        {
            std::unique_lock<std::mutex> lock(decodeMutex_);
            decodeCondition_.wait_for(lock, std::chrono::milliseconds(10),
                                      [this] { return !decodeQueue_.empty() || stopDecodeThread_.load(); });

            if (stopDecodeThread_.load() && decodeQueue_.empty()) break;

            std::lock_guard<std::mutex> queueLock(decodeQueueMutex_);
            size_t count = std::min(decodeQueue_.size(), static_cast<size_t>(100));
            batch.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                batch.push_back(decodeQueue_[i]);
            }
            decodeQueue_.erase(decodeQueue_.begin(),
                               decodeQueue_.begin() + count);
        }

        for (auto& request : batch) {
            if (stopDecodeThread_.load()) break;

            auto entry = DecodeNodeData(request.nodeKey);
            if (!entry) continue;

            {
                std::lock_guard<std::mutex> lock(requestMutex_);
                auto it = activeRequests_.find(request.nodeKey);
                if (it != activeRequests_.end()) {
                    it->second.state = NodeStreamingState::CpuReady;
                }
            }

            {
                std::lock_guard<std::mutex> lock(cpuCacheMutex_);
                cpuCache_[request.nodeKey] = std::move(entry);
                cpuCacheUsed_ += cpuCache_[request.nodeKey]->memorySize;
                // O(1) LRU: insert at back (most recently used)
                auto it = cpuLruMap_.find(request.nodeKey);
                if (it != cpuLruMap_.end()) {
                    cpuLruList_.erase(it->second);
                }
                cpuLruList_.push_back(request.nodeKey);
                cpuLruMap_[request.nodeKey] = std::prev(cpuLruList_.end());
            }
        }
    }
}

std::unique_ptr<CPUCacheEntry> PointStreamingManager::DecodeNodeData(uint64_t nodeKey) {
    // Look up the actual node by nodeKey instead of always decoding root
    pointcloud::PointCloudNode* node = nullptr;
    {
        std::lock_guard<std::mutex> lock(cpuCacheMutex_);
        auto it = nodeKeyMap_.find(nodeKey);
        if (it != nodeKeyMap_.end()) {
            node = it->second;
        }
    }

    // Fall back to root if key not found (for single-node clouds with key=0)
    if (!node) {
        if (!activeCloud_ || !activeCloud_->Root()) return nullptr;
        node = activeCloud_->Root();
    }

    const auto& channels = node->channels();
    size_t count = channels.PointCount();
    if (count == 0) return nullptr;

    auto entry = std::make_unique<CPUCacheEntry>();
    entry->nodeKey = nodeKey;
    entry->pointCount = static_cast<uint32_t>(count);

    fprintf(stderr, "[Tile Decode] nodeKey=%llu expected=%zu decoded=%zu bounds=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
            (unsigned long long)nodeKey,
            channels.PointCount(), channels.PointCount(),
            node->bounds().minX, node->bounds().minY, node->bounds().minZ,
            node->bounds().maxX, node->bounds().maxY, node->bounds().maxZ);
    fflush(stderr);

    entry->positions.resize(count * 3);
    entry->colors.resize(count * 3, 0.5f);
    entry->intensities.resize(count, 0.5f);
    entry->classifications.resize(count, 0.0f);
    entry->normals.resize(count * 3);

    double xyz[3] = {};
    uint8_t rgb[3] = {};

    for (size_t i = 0; i < count; ++i) {
        if (channels.ReadXYZ(i, xyz)) {
            entry->positions[i * 3 + 0] = static_cast<float>(xyz[0]);
            entry->positions[i * 3 + 1] = static_cast<float>(xyz[1]);
            entry->positions[i * 3 + 2] = static_cast<float>(xyz[2]);
        }

        if (channels.ReadRGB(i, rgb)) {
            entry->colors[i * 3 + 0] = rgb[0] / 255.0f;
            entry->colors[i * 3 + 1] = rgb[1] / 255.0f;
            entry->colors[i * 3 + 2] = rgb[2] / 255.0f;
        }

        auto attrs = channels.Attributes();
        if (attrs.Has(pointcloud::PointAttribute::Intensity)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Intensity);
            if (ch && ch->Data() && i < ch->Count()) {
                entry->intensities[i] = reinterpret_cast<const float*>(ch->Data())[i];
            }
        }

        if (attrs.Has(pointcloud::PointAttribute::Classification)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Classification);
            if (ch && ch->Data() && i < ch->Count()) {
                entry->classifications[i] = static_cast<float>(ch->Data()[i]);
            }
        }

        if (attrs.Has(pointcloud::PointAttribute::Normals)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Normals);
            if (ch && ch->Data() && i < ch->Count()) {
                const float* n = reinterpret_cast<const float*>(ch->Data()) + i * 3;
                entry->normals[i * 3 + 0] = n[0];
                entry->normals[i * 3 + 1] = n[1];
                entry->normals[i * 3 + 2] = n[2];
            } else {
                entry->normals[i * 3 + 0] = 0;
                entry->normals[i * 3 + 1] = 1;
                entry->normals[i * 3 + 2] = 0;
            }
        } else {
            entry->normals[i * 3 + 0] = 0;
            entry->normals[i * 3 + 1] = 1;
            entry->normals[i * 3 + 2] = 0;
        }
    }

    entry->memorySize = (count * 3 + count * 3 + count + count + count * 3) * sizeof(float);

    return entry;
}

void PointStreamingManager::ProcessDecodeQueue() {
}

void PointStreamingManager::UploadCpuReadyNodes() {
    std::lock_guard<std::mutex> lock(requestMutex_);

    for (auto& [nodeKey, request] : activeRequests_) {
        if (request.state != NodeStreamingState::CpuReady) continue;
        if (uploadedThisFrame_ >= uploadBatchSize_) break;

        std::unique_ptr<CPUCacheEntry> cpuEntry;
        {
            std::lock_guard<std::mutex> cacheLock(cpuCacheMutex_);
            auto it = cpuCache_.find(nodeKey);
            if (it == cpuCache_.end()) continue;
            cpuEntry = std::move(it->second);
            cpuCacheUsed_ -= cpuEntry->memorySize;
            cpuCache_.erase(it);
            // O(1) LRU: remove from CPU LRU list
            auto lruIt = cpuLruMap_.find(nodeKey);
            if (lruIt != cpuLruMap_.end()) {
                cpuLruList_.erase(lruIt->second);
                cpuLruMap_.erase(lruIt);
            }
        }

        if (!cpuEntry) continue;

        request.state = NodeStreamingState::GpuUploading;

        bool uploaded = false;
        {
            std::lock_guard<std::mutex> gpuLock(gpuResidencyMutex_);

            if (gpuMemoryUsed_ + cpuEntry->memorySize > gpuMemoryLimit_) {
                EvictGpuResidency();
            }

            if (gpuMemoryUsed_ + cpuEntry->memorySize <= gpuMemoryLimit_) {
                GPUResidentNode resident;
                resident.nodeKey = nodeKey;
                resident.gpuMemory = cpuEntry->memorySize;
                resident.lastUsedFrame = currentFrame_;
                resident.resident = true;

                gpuResidentNodes_[nodeKey] = resident;
                gpuMemoryUsed_ += cpuEntry->memorySize;
                // O(1) LRU: insert at back (most recently used)
                auto lruIt = gpuLruMap_.find(nodeKey);
                if (lruIt != gpuLruMap_.end()) {
                    gpuLruList_.erase(lruIt->second);
                }
                gpuLruList_.push_back(nodeKey);
                gpuLruMap_[nodeKey] = std::prev(gpuLruList_.end());
                uploaded = true;
            }
        }

        if (uploaded) {
            request.state = NodeStreamingState::GpuResident;
            uploadedThisFrame_ += cpuEntry->pointCount;
        } else {
            request.state = NodeStreamingState::CpuReady;
            {
                std::lock_guard<std::mutex> cacheLock(cpuCacheMutex_);
                cpuCacheUsed_ += cpuEntry->memorySize;
                cpuCache_[nodeKey] = std::move(cpuEntry);
                // O(1) LRU: re-insert at back (most recently used)
                auto lruIt = cpuLruMap_.find(nodeKey);
                if (lruIt != cpuLruMap_.end()) {
                    cpuLruList_.erase(lruIt->second);
                }
                cpuLruList_.push_back(nodeKey);
                cpuLruMap_[nodeKey] = std::prev(cpuLruList_.end());
            }
        }
    }
}

void PointStreamingManager::EvictCpuCache() {
    std::lock_guard<std::mutex> lock(cpuCacheMutex_);

    while (cpuCacheUsed_ > cpuCacheLimit_ && !cpuLruList_.empty()) {
        // O(1): take from front of LRU list (oldest)
        uint64_t oldestKey = cpuLruList_.front();
        cpuLruList_.pop_front();

        auto mapIt = cpuLruMap_.find(oldestKey);
        if (mapIt != cpuLruMap_.end()) {
            cpuLruMap_.erase(mapIt);
        }

        auto cacheIt = cpuCache_.find(oldestKey);
        if (cacheIt != cpuCache_.end()) {
            cpuCacheUsed_ -= cacheIt->second->memorySize;
            cpuCache_.erase(cacheIt);
            cpuEvictions_++;
        }
    }
}

void PointStreamingManager::EvictGpuResidency() {
    std::lock_guard<std::mutex> lock(gpuResidencyMutex_);

    while (gpuMemoryUsed_ > gpuMemoryLimit_ && !gpuLruList_.empty()) {
        // O(1): take from front of LRU list (oldest)
        uint64_t oldestKey = gpuLruList_.front();
        gpuLruList_.pop_front();

        auto mapIt = gpuLruMap_.find(oldestKey);
        if (mapIt != gpuLruMap_.end()) {
            gpuLruMap_.erase(mapIt);
        }

        auto nodeIt = gpuResidentNodes_.find(oldestKey);
        if (nodeIt != gpuResidentNodes_.end()) {
            gpuMemoryUsed_ -= nodeIt->second.gpuMemory;
            gpuResidentNodes_.erase(nodeIt);
            gpuEvictions_++;

            {
                std::lock_guard<std::mutex> reqLock(requestMutex_);
                auto it = activeRequests_.find(oldestKey);
                if (it != activeRequests_.end()) {
                    it->second.state = NodeStreamingState::Released;
                }
            }
        }
    }
}

void PointStreamingManager::UpdateDebugStats() {
    debugStats_ = {};

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        for (auto& [key, request] : activeRequests_) {
            switch (request.state) {
                case NodeStreamingState::Requested:
                    debugStats_.requestedNodes++;
                    break;
                case NodeStreamingState::Loading:
                    debugStats_.loadingNodes++;
                    break;
                case NodeStreamingState::CpuReady:
                    debugStats_.cpuReadyNodes++;
                    break;
                case NodeStreamingState::GpuUploading:
                    debugStats_.gpuUploadingNodes++;
                    break;
                case NodeStreamingState::GpuResident:
                    debugStats_.gpuResidentNodes++;
                    break;
                default:
                    break;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(cpuCacheMutex_);
        debugStats_.cpuCacheUsed = cpuCacheUsed_;
    }

    debugStats_.cpuCacheLimit = cpuCacheLimit_;

    {
        std::lock_guard<std::mutex> lock(gpuResidencyMutex_);
        debugStats_.gpuMemoryUsed = gpuMemoryUsed_;
    }

    debugStats_.gpuMemoryLimit = gpuMemoryLimit_;
    debugStats_.cpuEvictions = cpuEvictions_;
    debugStats_.gpuEvictions = gpuEvictions_;

    {
        std::lock_guard<std::mutex> lock(decodeQueueMutex_);
        debugStats_.queueSize = static_cast<uint32_t>(decodeQueue_.size());
    }
}

bool PointStreamingManager::IsNodeResident(uint64_t nodeKey) const {
    std::lock_guard<std::mutex> lock(gpuResidencyMutex_);
    auto it = gpuResidentNodes_.find(nodeKey);
    return it != gpuResidentNodes_.end() && it->second.resident;
}

bool PointStreamingManager::IsNodeCpuReady(uint64_t nodeKey) const {
    std::lock_guard<std::mutex> lock(cpuCacheMutex_);
    return cpuCache_.find(nodeKey) != cpuCache_.end();
}

bool PointStreamingManager::IsNodeLoading(uint64_t nodeKey) const {
    std::lock_guard<std::mutex> lock(requestMutex_);
    auto it = activeRequests_.find(nodeKey);
    if (it == activeRequests_.end()) return false;
    return it->second.state == NodeStreamingState::Loading ||
           it->second.state == NodeStreamingState::Requested;
}

PreparedGeometry* PointStreamingManager::GetCpuGeometry(uint64_t nodeKey) {
    return nullptr;
}

GPUResidentNode* PointStreamingManager::GetResidentNode(uint64_t nodeKey) {
    std::lock_guard<std::mutex> lock(gpuResidencyMutex_);
    auto it = gpuResidentNodes_.find(nodeKey);
    if (it != gpuResidentNodes_.end() && it->second.resident) {
        it->second.lastUsedFrame = currentFrame_;
        // O(1) LRU: splice to back (most recently used)
        auto lruIt = gpuLruMap_.find(nodeKey);
        if (lruIt != gpuLruMap_.end()) {
            gpuLruList_.splice(gpuLruList_.end(), gpuLruList_, lruIt->second);
        }
        return &it->second;
    }
    return nullptr;
}

} // namespace gpu
} // namespace workstation
