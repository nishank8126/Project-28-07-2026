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
    Staging,
    Visibility,       // NEW: per-node visibility info
    LODSelected       // NEW: LOD selection result
};

struct GPUBufferAllocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info = {};
    VkDeviceSize size = 0;
    BufferType type = BufferType::PointPosition;
    uint64_t lastUsedFrame = 0;
    bool isPersistent = true;

    bool IsValid() const { return buffer != VK_NULL_HANDLE; }
};

struct VisibilityInfo {
    uint64_t nodeKey = 0;       // Octree node key
    uint32_t visible : 1;       // 1 = visible, 0 = culled
    uint32_t lodLevel : 3;      // 0-3 LOD level
    uint32_t drawCount : 28;    // Number of instances/draws
};

struct IndirectDrawCommand {
    uint32_t vertexCount = 0;       // Vertex count per instance
    uint32_t instanceCount = 0;     // Number of instances
    uint32_t firstVertex = 0;       // First vertex index
    uint32_t firstInstance = 0;     // First instance index
};

struct DrawIndirectCommand {  // Vulkan struct equivalent
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

namespace {
    // Internal visibility buffer - one per application, accessed via GPUBufferManager
    VisibilityInfo internalVisibilityBuffer[256];
    uint32_t internalVisibilityCount = 0;
}  // anonymous namespace

class GPUBufferManager {
public:
    bool Initialize(vulkan::VulkanAllocator& allocator, uint32_t maxFramesInFlight = 2);
    void Shutdown();

    VmaAllocator GetAllocator() const { return allocator_->GetAllocator(); }

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