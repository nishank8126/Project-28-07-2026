#include "workstation/vulkan/VulkanDevice.h"
#include <set>
#include <vector>

namespace workstation {
namespace vulkan {

VulkanDevice::~VulkanDevice() { Shutdown(); }

bool VulkanDevice::Initialize(VkInstance instance, VkSurfaceKHR surface,
                               const VulkanDeviceConfig& config) {
    physicalDevice_.Choose(instance, surface);
    if (physicalDevice_.GetDevice() == VK_NULL_HANDLE) return false;

    auto indices = physicalDevice_.GetQueueFamilies();
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueFamilies = {
        static_cast<uint32_t>(indices.graphicsFamily),
        static_cast<uint32_t>(indices.presentFamily)
    };
    if (indices.computeFamily >= 0) uniqueFamilies.insert(indices.computeFamily);

    float queuePriority = 1.0f;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(queueInfo);
    }

    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (auto& ext : config.requiredExtensions) deviceExtensions.push_back(ext.c_str());

    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = config.enableAnisotropy;
    features.wideLines = VK_TRUE;
    deviceFeatures.features = features;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pEnabledFeatures = nullptr;

    if (vkCreateDevice(physicalDevice_.GetDevice(), &createInfo, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
    if (indices.computeFamily >= 0)
        vkGetDeviceQueue(device_, indices.computeFamily, 0, &computeQueue_);
    else
        computeQueue_ = graphicsQueue_;
    if (indices.transferFamily >= 0)
        vkGetDeviceQueue(device_, indices.transferFamily, 0, &transferQueue_);
    else
        transferQueue_ = graphicsQueue_;

    return true;
}

void VulkanDevice::Shutdown() {
    if (device_) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
}

} // namespace vulkan
} // namespace workstation
