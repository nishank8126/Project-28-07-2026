#pragma once
#include "workstation/vulkan/VulkanHeaders.h"

namespace workstation {
namespace vulkan {

struct VulkanFrameSync {
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    uint32_t imageIndex = 0;
    bool frameInFlight = false;
};

class VulkanFrameManager {
public:
    bool Initialize(VkDevice device, uint32_t maxFramesInFlight = 2,
                    uint32_t swapchainImageCount = 0);
    void Shutdown();

    VulkanFrameSync& GetCurrentFrame() { return frames_[currentFrame_]; }
    uint32_t GetCurrentFrameIndex() const { return currentFrame_; }
    uint32_t GetMaxFramesInFlight() const { return maxFramesInFlight_; }

    void BeginFrame();
    void EndFrame();

    void SetCommandBuffer(uint32_t frameIndex, VkCommandBuffer buffer) {
        frames_[frameIndex].commandBuffer = buffer;
    }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t maxFramesInFlight_ = 2;
    uint32_t currentFrame_ = 0;
    std::vector<VulkanFrameSync> frames_;

    bool CreateSyncObjects();
};

} // namespace vulkan
} // namespace workstation
