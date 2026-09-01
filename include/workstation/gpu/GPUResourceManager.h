#pragma once
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/gpu/GPUPointBuffer.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace workstation {
namespace gpu {

struct GPUNodeBuffer {
    uint64_t nodeKey = 0;
    std::unique_ptr<GPUPointBuffer> pointBuffer;
    uint64_t lastUsedFrame = 0;
    bool isUploaded = false;
};

class GPUResourceManager {
public:
    bool Initialize(vulkan::VulkanAllocator& allocator);
    void Shutdown();

    GPUNodeBuffer* AllocateNodeBuffer(uint64_t nodeKey, uint64_t maxPoints);
    GPUNodeBuffer* GetNodeBuffer(uint64_t nodeKey);
    void ReleaseNodeBuffer(uint64_t nodeKey);

    void EvictUnusedBuffers(uint64_t currentFrame, uint64_t maxAge = 60);

    uint32_t GetBufferCount() const { return static_cast<uint32_t>(nodeBuffers_.size()); }
    uint64_t GetTotalGPUPoints() const;
    uint64_t GetTotalGPUMemory() const;

    GPUPointBuffer* CreateTransientBuffer(uint64_t maxPoints);
    void DestroyTransientBuffer(GPUPointBuffer* buffer);

private:
    vulkan::VulkanAllocator* allocator_ = nullptr;
    std::unordered_map<uint64_t, std::unique_ptr<GPUNodeBuffer>> nodeBuffers_;
    std::vector<std::unique_ptr<GPUPointBuffer>> transientBuffers_;
};

} // namespace gpu
} // namespace workstation
