#include "workstation/gpu/PointStreamingPipeline.h"
#include "workstation/pointcloud/PointAttributeMask.h"
#include "workstation/pointcloud/PointChannelManager.h"

#include <cstring>

namespace workstation {
namespace gpu {

PointStreamingPipeline::~PointStreamingPipeline() { Shutdown(); }

bool PointStreamingPipeline::Initialize(GPUResourceManager* resourceManager,
                                         uint32_t uploadBatchSize) {
    resourceManager_ = resourceManager;
    uploadBatchSize_ = uploadBatchSize;
    return true;
}

void PointStreamingPipeline::Shutdown() {
    CancelAllRequests();
    if (decodeThread_.joinable()) decodeThread_.join();
    resourceManager_ = nullptr;
}

void PointStreamingPipeline::SetActiveCloud(pointcloud::PointCloud* cloud) {
    CancelAllRequests();
    activeCloud_ = cloud;
}

void PointStreamingPipeline::RequestNodeUpload(uint64_t nodeKey, uint64_t priority,
                                                const renderer::LODManager& lodManager) {
    (void)lodManager;
    UploadRequest request;
    request.nodeKey = nodeKey;
    request.priority = priority;
    uploadQueue_.push(request);
}

void PointStreamingPipeline::Update(uint64_t currentFrame) {
    currentFrame_ = currentFrame;
    uploadedThisFrame_ = 0;

    while (!uploadQueue_.empty() && uploadedThisFrame_ < uploadBatchSize_) {
        auto request = uploadQueue_.top();
        uploadQueue_.pop();

        auto* existing = resourceManager_->GetNodeBuffer(request.nodeKey);
        if (existing && existing->isUploaded) continue;

        auto vertices = DecodeNodePoints(request.nodeKey);
        if (vertices.empty()) continue;

        GPUNodeBuffer* nodeBuf = nullptr;
        if (existing) {
            nodeBuf = existing;
        } else {
            nodeBuf = resourceManager_->AllocateNodeBuffer(request.nodeKey, vertices.size());
        }

        if (nodeBuf && nodeBuf->pointBuffer) {
            nodeBuf->pointBuffer->UploadPoints(vertices.data(), vertices.size());
            nodeBuf->isUploaded = true;
            nodeBuf->lastUsedFrame = currentFrame_;
            uploadedThisFrame_ += static_cast<uint32_t>(vertices.size());
        }
    }
}

void PointStreamingPipeline::CancelAllRequests() {
    while (!uploadQueue_.empty()) uploadQueue_.pop();
}

std::vector<PointVertex> PointStreamingPipeline::DecodeNodePoints(uint64_t nodeKey) {
    std::vector<PointVertex> result;
    if (!activeCloud_ || !activeCloud_->Root()) return result;

    auto* root = activeCloud_->Root();
    const auto& channels = root->channels();
    size_t count = channels.PointCount();
    if (count == 0) return result;

    result.resize(count);

    PointVertex v{};
    double xyz[3] = {};
    uint8_t rgb[3] = {};

    for (size_t i = 0; i < count; ++i) {
        std::memset(&v, 0, sizeof(PointVertex));

        if (channels.ReadXYZ(i, xyz)) {
            v.position[0] = static_cast<float>(xyz[0]);
            v.position[1] = static_cast<float>(xyz[1]);
            v.position[2] = static_cast<float>(xyz[2]);
        }

        if (channels.ReadRGB(i, rgb)) {
            v.color[0] = rgb[0] / 255.0f;
            v.color[1] = rgb[1] / 255.0f;
            v.color[2] = rgb[2] / 255.0f;
        } else {
            v.color[0] = 0.5f;
            v.color[1] = 0.5f;
            v.color[2] = 0.5f;
        }

        auto attrs = channels.Attributes();
        if (attrs.Has(pointcloud::PointAttribute::Intensity)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Intensity);
            if (ch && ch->Data() && i < ch->Count()) {
                v.intensity = reinterpret_cast<const float*>(ch->Data())[i];
            }
        } else {
            v.intensity = 0.5f;
        }

        if (attrs.Has(pointcloud::PointAttribute::Classification)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Classification);
            if (ch && ch->Data() && i < ch->Count()) {
                v.classification = static_cast<float>(ch->Data()[i]);
            }
        }

        if (attrs.Has(pointcloud::PointAttribute::Normals)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Normals);
            if (ch && ch->Data() && i < ch->Count()) {
                const float* n = reinterpret_cast<const float*>(ch->Data()) + i * 3;
                v.normal[0] = n[0]; v.normal[1] = n[1]; v.normal[2] = n[2];
            }
        } else {
            v.normal[0] = 0; v.normal[1] = 1; v.normal[2] = 0;
        }

        result[i] = v;
    }
    return result;
}

} // namespace gpu
} // namespace workstation
