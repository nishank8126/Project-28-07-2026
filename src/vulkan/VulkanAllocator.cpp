#include "workstation/vulkan/VulkanAllocator.h"

namespace workstation {
namespace vulkan {

VulkanAllocator& VulkanAllocator::Get() {
    static VulkanAllocator instance;
    return instance;
}

void VulkanAllocator::Initialize(VkInstance instance, VkPhysicalDevice physicalDevice,
                                  VkDevice device, const VmaVulkanFunctions& vulkanFunctions) {
    device_ = device;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    allocatorInfo.instance = instance;

    vmaCreateAllocator(&allocatorInfo, &allocator_);
}

void VulkanAllocator::Shutdown() {
    if (allocator_) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
}

GPUBuffer VulkanAllocator::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                         VmaMemoryUsage memoryUsage,
                                         VmaAllocationCreateFlags flags) {
    GPUBuffer buffer{};
    buffer.size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.flags = flags;

    vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &buffer.buffer,
                    &buffer.allocation, &buffer.allocationInfo);
    buffer.mappedData = buffer.allocationInfo.pMappedData;

    stats_.totalAllocated += size;
    stats_.bufferCount++;
    return buffer;
}

void VulkanAllocator::DestroyBuffer(GPUBuffer& buffer) {
    if (buffer.IsValid()) {
        vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        stats_.totalAllocated -= buffer.size;
        stats_.bufferCount--;
        buffer = {};
    }
}

GPUImage VulkanAllocator::CreateImage(VkFormat format, VkExtent2D extent,
                                       VkImageUsageFlags usage, VmaMemoryUsage memoryUsage) {
    GPUImage image{};
    image.format = format;
    image.extent = extent;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    vmaCreateImage(allocator_, &imageInfo, &allocInfo, &image.image, &image.allocation, nullptr);
    stats_.imageCount++;
    return image;
}

void VulkanAllocator::DestroyImage(GPUImage& image) {
    if (image.image != VK_NULL_HANDLE) {
        if (image.imageView != VK_NULL_HANDLE)
            vkDestroyImageView(device_, image.imageView, nullptr);
        vmaDestroyImage(allocator_, image.image, image.allocation);
        stats_.imageCount--;
        image = {};
    }
}

VulkanAllocator::Stats VulkanAllocator::GetStats() const {
    Stats result = stats_;
    VmaTotalStatistics vmaStats;
    vmaCalculateStatistics(allocator_, &vmaStats);
    result.totalUsed = vmaStats.total.statistics.blockBytes;
    result.allocationCount = vmaStats.total.unusedRangeCount;
    return result;
}

} // namespace vulkan
} // namespace workstation
