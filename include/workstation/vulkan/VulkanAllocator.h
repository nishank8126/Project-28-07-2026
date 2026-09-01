#pragma once
#include "workstation/vulkan/VulkanHeaders.h"

namespace workstation {
namespace vulkan {

struct GPUBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo = {};
    VkDeviceSize size = 0;
    VkDeviceSize alignment = 0;
    void* mappedData = nullptr;

    bool IsValid() const { return buffer != VK_NULL_HANDLE; }
    void InvalidateMapped() {
        if (mappedData) vmaInvalidateAllocation(VulkanAllocator::Get(), allocation, 0, size);
    }
    void FlushMapped() {
        if (mappedData) vmaFlushAllocation(VulkanAllocator::Get(), allocation, 0, size);
    }
};

struct GPUImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
};

class VulkanAllocator {
public:
    static VulkanAllocator& Get();

    void Initialize(VkInstance instance, VkPhysicalDevice physicalDevice,
                    VkDevice device, const VmaVulkanFunctions& vulkanFunctions);
    void Shutdown();

    GPUBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaMemoryUsage memoryUsage,
                           VmaAllocationCreateFlags flags = 0);

    void DestroyBuffer(GPUBuffer& buffer);

    GPUImage CreateImage(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage,
                         VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY);

    void DestroyImage(GPUImage& image);

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    VmaAllocator GetAllocator() const { return allocator_; }

    struct Stats {
        uint64_t totalAllocated = 0;
        uint64_t totalUsed = 0;
        uint32_t allocationCount = 0;
        uint32_t bufferCount = 0;
        uint32_t imageCount = 0;
    };
    Stats GetStats() const;

private:
    VulkanAllocator() = default;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    Stats stats_ = {};
};

} // namespace vulkan
} // namespace workstation
