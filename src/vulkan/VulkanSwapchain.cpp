#include "workstation/vulkan/VulkanSwapchain.h"
#include <algorithm>
#include <limits>

namespace workstation {
namespace vulkan {

VulkanSwapchain::~VulkanSwapchain() { Shutdown(); }

bool VulkanSwapchain::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                                  VkSurfaceKHR surface, uint32_t width, uint32_t height,
                                  const SwapchainSupportDetails& support,
                                  const QueueFamilyIndices& indices) {
    device_ = device;
    physicalDevice_ = physicalDevice;
    surface_ = surface;
    indices_ = indices;
    vkGetDeviceQueue(device_, static_cast<uint32_t>(indices_.presentFamily), 0, &presentQueue_);

    auto chooseFormat = [](const std::vector<VkSurfaceFormatKHR>& f) {
        for (auto& fmt : f) {
            if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
                fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return fmt;
        }
        return f[0];
    };

    auto choosePresentMode = [](const std::vector<VkPresentModeKHR>& m) {
        for (auto& mode : m) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    };

    auto chooseExtent = [&](const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) {
        if (caps.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
            return caps.currentExtent;
        }
        VkExtent2D ext = {w, h};
        ext.width = std::clamp(ext.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return ext;
    };

    auto surfaceFormat = chooseFormat(support.formats);
    auto presentMode = choosePresentMode(support.presentModes);
    extent_ = chooseExtent(support.capabilities, width, height);
    imageFormat_ = surfaceFormat.format;

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = imageFormat_;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {
        static_cast<uint32_t>(indices_.graphicsFamily),
        static_cast<uint32_t>(indices_.presentFamily)
    };

    if (indices_.graphicsFamily != indices_.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS)
        return false;

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    images_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images_.data());

    CreateImageViews();
    CreateDepthResources();
    return true;
}

void VulkanSwapchain::Shutdown() {
    CleanupSwapchain();
}

void VulkanSwapchain::Recreate(uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(device_);
    CleanupSwapchain();
}

void VulkanSwapchain::CleanupSwapchain() {
    if (depthImageView_) vkDestroyImageView(device_, depthImageView_, nullptr);
    if (depthImage_) vkDestroyImage(device_, depthImage_, nullptr);
    if (depthMemory_) vkFreeMemory(device_, depthMemory_, nullptr);
    depthImageView_ = VK_NULL_HANDLE;
    depthImage_ = VK_NULL_HANDLE;
    depthMemory_ = VK_NULL_HANDLE;

    for (auto iv : imageViews_) vkDestroyImageView(device_, iv, nullptr);
    imageViews_.clear();
    images_.clear();

    if (swapchain_) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanSwapchain::CreateImageViews() {
    imageViews_.resize(images_.size());
    for (size_t i = 0; i < images_.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = images_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = imageFormat_;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device_, &createInfo, nullptr, &imageViews_[i]);
    }
}

void VulkanSwapchain::CreateDepthResources() {
    depthFormat_ = ChooseDepthFormat(physicalDevice_);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent_.width, extent_.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    vkCreateImage(device_, &imageInfo, nullptr, &depthImage_);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, depthImage_, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }
    vkAllocateMemory(device_, &allocInfo, nullptr, &depthMemory_);
    vkBindImageMemory(device_, depthImage_, depthMemory_, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_);
}

VkResult VulkanSwapchain::AcquireNextImage(VkSemaphore signalSemaphore, uint32_t* imageIndex) {
    return vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, signalSemaphore,
                                  VK_NULL_HANDLE, imageIndex);
}

VkResult VulkanSwapchain::Present(VkSemaphore waitSemaphore, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    return vkQueuePresentKHR(presentQueue_, &presentInfo);
}

} // namespace vulkan
} // namespace workstation
