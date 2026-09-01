#pragma once
#include "workstation/gpu/GPUPointBuffer.h"
#include "workstation/gpu/GPUResourceManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointStorage.h"
#include "workstation/renderer/LODManager.h"

#include <thread>
#include <atomic>
#include <queue>
#include <vector>
#include <functional>

namespace workstation {
namespace gpu {

struct UploadRequest {
    uint64_t nodeKey = 0;
    std::vector<PointVertex> vertices;
    uint64_t priority = 0;
    bool operator<(const UploadRequest& o) const { return priority < o.priority; }
};

enum class StreamingState {
    Idle,
    Loading,
    Uploading,
    Complete
};

class PointStreamingPipeline {
public:
    ~PointStreamingPipeline();

    bool Initialize(GPUResourceManager* resourceManager, uint32_t uploadBatchSize = 100000);
    void Shutdown();

    void SetActiveCloud(pointcloud::PointCloud* cloud);

    void RequestNodeUpload(uint64_t nodeKey, uint64_t priority,
                           const renderer::LODManager& lodManager);

    void Update(uint64_t currentFrame);

    void CancelAllRequests();

    StreamingState GetState() const { return state_.load(); }
    uint32_t GetPendingUploads() const { return static_cast<uint32_t>(uploadQueue_.size()); }
    uint32_t GetUploadedThisFrame() const { return uploadedThisFrame_; }

private:
    GPUResourceManager* resourceManager_ = nullptr;
    pointcloud::PointCloud* activeCloud_ = nullptr;
    uint32_t uploadBatchSize_ = 100000;

    std::priority_queue<UploadRequest> uploadQueue_;
    std::atomic<StreamingState> state_{StreamingState::Idle};
    std::atomic<bool> stopThread_{false};
    std::thread decodeThread_;
    uint32_t uploadedThisFrame_ = 0;
    uint64_t currentFrame_ = 0;

    void DecodeThreadFunc();
    std::vector<PointVertex> DecodeNodePoints(uint64_t nodeKey);
    void UploadBatch();
};

} // namespace gpu
} // namespace workstation
