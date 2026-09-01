#include "workstation/vulkan/VulkanAllocator.h"

namespace workstation {
namespace vulkan {

VulkanAllocator& VulkanAllocator::Get() {
    static VulkanAllocator instance;
    return instance;
}

void VulkanAllocator::Initialize(VkInstance instance, VkPhysicalDevice physicalDevice,
                                  VkDevice device, VkQueue graphicsQueue,
                                  uint32_t graphicsFamilyIndex,
                                  const VmaVulkanFunctions& vulkanFunctions) {
    device_ = device;
    graphicsQueue_ = graphicsQueue;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamilyIndex;
    vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_);

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
    if (commandPool_) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    graphicsQueue_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
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

void VulkanAllocator::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer cmd = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);

    EndSingleTimeCommands(cmd);
}

VulkanAllocator::Stats VulkanAllocator::GetStats() const {
    Stats result = stats_;
    VmaTotalStatistics vmaStats;
    vmaCalculateStatistics(allocator_, &vmaStats);
    result.totalUsed = vmaStats.total.statistics.blockBytes;
    result.allocationCount = vmaStats.total.unusedRangeCount;
    return result;
}

VkCommandBuffer VulkanAllocator::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device_, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

void VulkanAllocator::EndSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
}

} // namespace vulkan
} // namespace workstation
