#pragma once
#include "workstation/gpu/PreparedGeometry.h"
#include "workstation/gpu/GPUBufferManager.h"
#include "workstation/gpu/GPUResourceManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <list>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>

namespace workstation {
namespace gpu {

enum class NodeStreamingState {
    Unloaded,
    Requested,
    Loading,
    CpuReady,
    GpuUploading,
    GpuResident,
    Released
};

struct StreamingRequest {
    uint64_t nodeKey = 0;
    float priority = 0.0f;
    uint64_t pointCount = 0;
    float screenSpaceError = 0.0f;
    NodeStreamingState state = NodeStreamingState::Unloaded;

    bool operator<(const StreamingRequest& o) const {
        return priority < o.priority;
    }
};

struct CPUCacheEntry {
    uint64_t nodeKey = 0;
    std::vector<float> positions;
    std::vector<float> colors;
    std::vector<float> intensities;
    std::vector<float> classifications;
    std::vector<float> normals;
    uint32_t pointCount = 0;
    uint64_t memorySize = 0;
    uint64_t lastUsedFrame = 0;
    bool pinned = false;
};

struct GPUResidentNode {
    uint64_t nodeKey = 0;
    uint64_t gpuMemory = 0;
    uint64_t lastUsedFrame = 0;
    bool resident = false;
};

struct StreamingDebugStats {
    uint32_t requestedNodes = 0;
    uint32_t loadingNodes = 0;
    uint32_t cpuReadyNodes = 0;
    uint32_t gpuUploadingNodes = 0;
    uint32_t gpuResidentNodes = 0;
    uint64_t cpuCacheUsed = 0;
    uint64_t cpuCacheLimit = 0;
    uint64_t gpuMemoryUsed = 0;
    uint64_t gpuMemoryLimit = 0;
    uint32_t cpuEvictions = 0;
    uint32_t gpuEvictions = 0;
    double decodeTimeMs = 0.0;
    double uploadTimeMs = 0.0;
    uint32_t queueSize = 0;
};

class PointStreamingManager {
public:
    ~PointStreamingManager();

    bool Initialize(GPUBufferManager* bufferManager,
                    uint64_t cpuCacheLimit = 20ULL * 1024 * 1024 * 1024,
                    uint64_t gpuMemoryLimit = 4ULL * 1024 * 1024 * 1024);
    void Shutdown();

    void SetActiveCloud(pointcloud::PointCloud* cloud);

    void RequestNode(uint64_t nodeKey, float priority,
                     uint64_t pointCount, float screenSpaceError);
    void CancelRequest(uint64_t nodeKey);
    void CancelAllRequests();

    void Update(uint64_t currentFrame);

    bool IsNodeResident(uint64_t nodeKey) const;
    bool IsNodeCpuReady(uint64_t nodeKey) const;
    bool IsNodeLoading(uint64_t nodeKey) const;

    PreparedGeometry* GetCpuGeometry(uint64_t nodeKey);
    GPUResidentNode* GetResidentNode(uint64_t nodeKey);

    const StreamingDebugStats& GetDebugStats() const { return debugStats_; }

    uint32_t GetPendingRequests() const { return static_cast<uint32_t>(requestQueue_.size()); }

private:
    GPUBufferManager* bufferManager_ = nullptr;
    pointcloud::PointCloud* activeCloud_ = nullptr;
    uint64_t currentFrame_ = 0;

    // Flat map: nodeKey -> PointCloudNode* for node-specific decoding
    std::unordered_map<uint64_t, pointcloud::PointCloudNode*> nodeKeyMap_;
    void BuildNodeKeyMap(pointcloud::PointCloud* cloud);

    std::priority_queue<StreamingRequest> requestQueue_;
    std::unordered_map<uint64_t, StreamingRequest> activeRequests_;
    mutable std::mutex requestMutex_;

    std::unordered_map<uint64_t, std::unique_ptr<CPUCacheEntry>> cpuCache_;
    uint64_t cpuCacheUsed_ = 0;
    uint64_t cpuCacheLimit_ = 20ULL * 1024 * 1024 * 1024;
    mutable std::mutex cpuCacheMutex_;

    // O(1) LRU: list (front=oldest, back=MRU) + map to iterator
    std::list<uint64_t> cpuLruList_;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> cpuLruMap_;

    std::unordered_map<uint64_t, GPUResidentNode> gpuResidentNodes_;
    uint64_t gpuMemoryUsed_ = 0;
    uint64_t gpuMemoryLimit_ = 4ULL * 1024 * 1024 * 1024;
    mutable std::mutex gpuResidencyMutex_;

    // O(1) LRU for GPU residency
    std::list<uint64_t> gpuLruList_;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> gpuLruMap_;

    std::atomic<bool> stopDecodeThread_{false};
    std::thread decodeThread_;
    std::condition_variable decodeCondition_;
    std::mutex decodeMutex_;
    std::vector<StreamingRequest> decodeQueue_;
    std::mutex decodeQueueMutex_;

    StreamingDebugStats debugStats_ = {};

    uint32_t uploadBatchSize_ = 500000;
    uint32_t uploadedThisFrame_ = 0;
    uint32_t cpuEvictions_ = 0;
    uint32_t gpuEvictions_ = 0;

    void DecodeThreadFunc();
    std::unique_ptr<CPUCacheEntry> DecodeNodeData(uint64_t nodeKey);
    void ProcessDecodeQueue();
    void UploadCpuReadyNodes();
    void EvictCpuCache();
    void EvictGpuResidency();
    void UpdateDebugStats();
};

} // namespace gpu
} // namespace workstation
