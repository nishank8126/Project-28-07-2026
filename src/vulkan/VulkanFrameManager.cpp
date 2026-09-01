#include "workstation/vulkan/VulkanFrameManager.h"
#include <vector>

namespace workstation {
namespace vulkan {

VulkanFrameManager::~VulkanFrameManager() { Shutdown(); }

bool VulkanFrameManager::Initialize(VkDevice device, uint32_t maxFramesInFlight,
                                     uint32_t swapchainImageCount) {
    device_ = device;
    maxFramesInFlight_ = maxFramesInFlight;
    frames_.resize(maxFramesInFlight);
    return CreateSyncObjects();
}

void VulkanFrameManager::Shutdown() {
    for (auto& frame : frames_) {
        if (frame.imageAvailable) vkDestroySemaphore(device_, frame.imageAvailable, nullptr);
        if (frame.renderFinished) vkDestroySemaphore(device_, frame.renderFinished, nullptr);
        if (frame.inFlightFence) vkDestroyFence(device_, frame.inFlightFence, nullptr);
        frame = {};
    }
    device_ = VK_NULL_HANDLE;
}

bool VulkanFrameManager::CreateSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& frame : frames_) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS)
            return false;
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.renderFinished) != VK_SUCCESS)
            return false;
        if (vkCreateFence(device_, &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS)
            return false;
    }
    return true;
}

void VulkanFrameManager::BeginFrame() {
    auto& frame = frames_[currentFrame_];
    vkWaitForFences(device_, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &frame.inFlightFence);
    frame.frameInFlight = true;
}

void VulkanFrameManager::EndFrame() {
    frames_[currentFrame_].frameInFlight = false;
    currentFrame_ = (currentFrame_ + 1) % maxFramesInFlight_;
}

} // namespace vulkan
} // namespace workstation
