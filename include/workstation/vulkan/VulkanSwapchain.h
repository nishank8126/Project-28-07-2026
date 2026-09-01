#pragma once
#include "workstation/vulkan/VulkanHeaders.h"
#include "workstation/vulkan/VulkanPhysicalDevice.h"

namespace workstation {
namespace vulkan {

class VulkanSwapchain {
public:
    ~VulkanSwapchain();

    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkSurfaceKHR surface, uint32_t width, uint32_t height,
                    const SwapchainSupportDetails& support,
                    const QueueFamilyIndices& indices);
    void Shutdown();
    void Recreate(uint32_t width, uint32_t height);

    VkResult AcquireNextImage(VkSemaphore signalSemaphore, uint32_t* imageIndex);
    VkResult Present(VkSemaphore waitSemaphore, uint32_t imageIndex);

    VkFormat GetImageFormat() const { return imageFormat_; }
    VkExtent2D GetExtent() const { return extent_; }
    VkImageView GetImageView(uint32_t index) const { return imageViews_[index]; }
    VkImageView GetDepthImageView() const { return depthImageView_; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(images_.size()); }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;

    VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_ = {0, 0};

    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;

    QueueFamilyIndices indices_ = {};

    void CreateSwapchain(uint32_t width, uint32_t height,
                         const SwapchainSupportDetails& support);
    void CreateImageViews();
    void CreateDepthResources();
    void CleanupSwapchain();
};

} // namespace vulkan
} // namespace workstation
