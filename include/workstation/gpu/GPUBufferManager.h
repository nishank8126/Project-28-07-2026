#pragma once
#include "workstation/vulkan/VulkanHeaders.h"
#include "workstation/vulkan/VulkanAllocator.h"

#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace workstation {
namespace gpu {

enum class BufferType {
    PointPosition,
    PointColor,
    PointIntensity,
    PointClassification,
    PointNormal,
    Uniform,
    Storage,
    IndirectDraw,
    Staging
};

struct GPUBufferAllocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info = {};
    VkDeviceSize size = 0;
    BufferType type = BufferType::PointPosition;
    uint64_t lastUsedFrame = 0;
    bool isPersistent = true;
};

class GPUBufferManager {
public:
    bool Initialize(vulkan::VulkanAllocator& allocator, uint32_t maxFramesInFlight = 2);
    void Shutdown();

    GPUBufferAllocation* Allocate(BufferType type, VkDeviceSize size,
                                   bool hostVisible = false);
    void Free(GPUBufferAllocation* allocation);

    GPUBufferAllocation* AllocateStaging(VkDeviceSize size);
    void FreeStaging(GPUBufferAllocation* staging);

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer cmd);

    void UploadToGPU(GPUBufferAllocation* dst, const void* data,
                      VkDeviceSize size, VkDeviceSize offset = 0);

    void EvictUnused(uint64_t currentFrame, uint64_t maxAge = 120);

    struct Stats {
        uint64_t totalAllocated = 0;
        uint64_t totalUsed = 0;
        uint32_t allocationCount = 0;
        uint32_t stagingCount = 0;
        uint32_t pointBufferCount = 0;
        uint32_t uniformBufferCount = 0;
        uint32_t indirectBufferCount = 0;
    };
    Stats GetStats() const;

private:
    vulkan::VulkanAllocator* allocator_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    uint32_t maxFramesInFlight_ = 2;

    std::vector<std::unique_ptr<GPUBufferAllocation>> allocations_;
    std::vector<std::unique_ptr<GPUBufferAllocation>> stagingAllocations_;
    mutable std::mutex mutex_;
};

} // namespace gpu
} // namespace workstation
