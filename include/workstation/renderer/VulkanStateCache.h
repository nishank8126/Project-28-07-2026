#pragma once
#include "workstation/vulkan/VulkanHeaders.h"

#include <cstdint>

namespace workstation {
namespace renderer {

class VulkanStateCache {
public:
    void Initialize();

    void SetCurrentPipeline(VkPipeline pipeline);
    void SetCurrentPipelineLayout(VkPipelineLayout layout);
    void SetCurrentDescriptorSet(VkDescriptorSet set, uint32_t setIndex = 0);
    void SetCurrentVertexBuffer(VkBuffer buffer, uint32_t binding = 0);
    void SetCurrentVertexOffset(VkDeviceSize offset, uint32_t binding = 0);
    void SetCurrentRenderPass(VkRenderPass renderPass);
    void SetCurrentFramebuffer(VkFramebuffer framebuffer);

    bool PipelineChanged() const { return currentPipeline_ != lastPipeline_; }
    bool PipelineLayoutChanged() const { return currentLayout_ != lastLayout_; }
    bool DescriptorSetChanged(uint32_t index) const {
        return currentDescriptorSet_[index] != lastDescriptorSet_[index];
    }
    bool VertexBufferChanged(uint32_t binding = 0) const {
        return currentVertexBuffer_[binding] != lastVertexBuffer_[binding];
    }
    bool RenderPassChanged() const { return currentRenderPass_ != lastRenderPass_; }
    bool FramebufferChanged() const { return currentFramebuffer_ != lastFramebuffer_; }

    void ApplyPipeline(VkCommandBuffer cmd);
    void ApplyPipelineLayout(VkCommandBuffer cmd);
    void ApplyDescriptorSet(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint,
                             uint32_t firstSet = 0, uint32_t count = 1);
    void ApplyVertexBuffer(VkCommandBuffer cmd, uint32_t binding = 0);
    void ApplyRenderPass(VkCommandBuffer cmd);
    void ApplyFramebuffer(VkCommandBuffer cmd);

    void Commit(); // Call after frame to swap last = current
    void Reset();  // Call at frame start

    void InvalidateAll();

    struct Stats {
        uint32_t pipelineChanges = 0;
        uint32_t descriptorChanges = 0;
        uint32_t bufferChanges = 0;
        uint32_t renderPassChanges = 0;
        uint32_t totalVulkanCommands = 0;
    };
    Stats GetStats() const { return stats_; }
    void ResetStats() { stats_ = {}; }

private:
    VkPipeline currentPipeline_ = VK_NULL_HANDLE;
    VkPipeline lastPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout currentLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout lastLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet currentDescriptorSet_[8] = {};
    VkDescriptorSet lastDescriptorSet_[8] = {};
    VkBuffer currentVertexBuffer_[4] = {};
    VkBuffer lastVertexBuffer_[4] = {};
    VkDeviceSize currentVertexOffset_[4] = {};
    VkRenderPass currentRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass lastRenderPass_ = VK_NULL_HANDLE;
    VkFramebuffer currentFramebuffer_ = VK_NULL_HANDLE;
    VkFramebuffer lastFramebuffer_ = VK_NULL_HANDLE;

    Stats stats_ = {};
};

} // namespace renderer
} // namespace workstation
