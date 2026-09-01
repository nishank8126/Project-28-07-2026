#include "workstation/gpu/GPUResourceManager.h"

namespace workstation {
namespace gpu {

bool GPUResourceManager::Initialize(vulkan::VulkanAllocator& allocator) {
    allocator_ = &allocator;
    return true;
}

void GPUResourceManager::Shutdown() {
    nodeBuffers_.clear();
    transientBuffers_.clear();
    allocator_ = nullptr;
}

GPUNodeBuffer* GPUResourceManager::AllocateNodeBuffer(uint64_t nodeKey, uint64_t maxPoints) {
    auto buffer = std::make_unique<GPUPointBuffer>();
    if (!buffer->Initialize(*allocator_, maxPoints)) return nullptr;

    GPUNodeBuffer nodeBuffer{};
    nodeBuffer.nodeKey = nodeKey;
    nodeBuffer.pointBuffer = std::move(buffer);

    auto* ptr = nodeBuffer.pointBuffer.get();
    nodeBuffers_[nodeKey] = std::make_unique<GPUNodeBuffer>(std::move(nodeBuffer));
    return nodeBuffers_[nodeKey].get();
}

GPUNodeBuffer* GPUResourceManager::GetNodeBuffer(uint64_t nodeKey) {
    auto it = nodeBuffers_.find(nodeKey);
    if (it != nodeBuffers_.end()) return it->second.get();
    return nullptr;
}

void GPUResourceManager::ReleaseNodeBuffer(uint64_t nodeKey) {
    nodeBuffers_.erase(nodeKey);
}

void GPUResourceManager::EvictUnusedBuffers(uint64_t currentFrame, uint64_t maxAge) {
    auto it = nodeBuffers_.begin();
    while (it != nodeBuffers_.end()) {
        if (currentFrame - it->second->lastUsedFrame > maxAge) {
            it = nodeBuffers_.erase(it);
        } else {
            ++it;
        }
    }
}

uint64_t GPUResourceManager::GetTotalGPUPoints() const {
    uint64_t total = 0;
    for (auto& [key, buf] : nodeBuffers_) {
        if (buf->pointBuffer) total += buf->pointBuffer->GetPointCount();
    }
    return total;
}

uint64_t GPUResourceManager::GetTotalGPUMemory() const {
    uint64_t total = 0;
    for (auto& [key, buf] : nodeBuffers_) {
        if (buf->pointBuffer) total += buf->pointBuffer->GetAllocatedSize();
    }
    return total;
}

GPUPointBuffer* GPUResourceManager::CreateTransientBuffer(uint64_t maxPoints) {
    auto buffer = std::make_unique<GPUPointBuffer>();
    if (!buffer->Initialize(*allocator_, maxPoints, PointBufferUsage::Streaming))
        return nullptr;

    GPUPointBuffer* ptr = buffer.get();
    transientBuffers_.push_back(std::move(buffer));
    return ptr;
}

void GPUResourceManager::DestroyTransientBuffer(GPUPointBuffer* buffer) {
    auto it = std::find_if(transientBuffers_.begin(), transientBuffers_.end(),
                           [buffer](const std::unique_ptr<GPUPointBuffer>& b) {
                               return b.get() == buffer;
                           });
    if (it != transientBuffers_.end()) {
        transientBuffers_.erase(it);
    }
}

} // namespace gpu
} // namespace workstation
