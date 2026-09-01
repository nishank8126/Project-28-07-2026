#include "workstation/renderer/VulkanStateCache.h"

namespace workstation {
namespace renderer {

void VulkanStateCache::Initialize() {
    Reset();
}

void VulkanStateCache::Reset() {
    lastPipeline_ = VK_NULL_HANDLE;
    lastLayout_ = VK_NULL_HANDLE;
    for (auto& ds : lastDescriptorSet_) ds = VK_NULL_HANDLE;
    for (auto& vb : lastVertexBuffer_) vb = VK_NULL_HANDLE;
    for (auto& vo : lastVertexOffset_) vo = 0;
    lastRenderPass_ = VK_NULL_HANDLE;
    lastFramebuffer_ = VK_NULL_HANDLE;
    currentPipeline_ = VK_NULL_HANDLE;
    currentLayout_ = VK_NULL_HANDLE;
    for (auto& ds : currentDescriptorSet_) ds = VK_NULL_HANDLE;
    for (auto& vb : currentVertexBuffer_) vb = VK_NULL_HANDLE;
    for (auto& vo : currentVertexOffset_) vo = 0;
    currentRenderPass_ = VK_NULL_HANDLE;
    currentFramebuffer_ = VK_NULL_HANDLE;
}

void VulkanStateCache::SetCurrentPipeline(VkPipeline pipeline) {
    currentPipeline_ = pipeline;
}

void VulkanStateCache::SetCurrentPipelineLayout(VkPipelineLayout layout) {
    currentLayout_ = layout;
}

void VulkanStateCache::SetCurrentDescriptorSet(VkDescriptorSet set, uint32_t index) {
    if (index < 8) currentDescriptorSet_[index] = set;
}

void VulkanStateCache::SetCurrentVertexBuffer(VkBuffer buffer, uint32_t binding) {
    if (binding < 4) currentVertexBuffer_[binding] = buffer;
}

void VulkanStateCache::SetCurrentVertexOffset(VkDeviceSize offset, uint32_t binding) {
    if (binding < 4) currentVertexOffset_[binding] = offset;
}

void VulkanStateCache::SetCurrentRenderPass(VkRenderPass renderPass) {
    currentRenderPass_ = renderPass;
}

void VulkanStateCache::SetCurrentFramebuffer(VkFramebuffer framebuffer) {
    currentFramebuffer_ = framebuffer;
}

void VulkanStateCache::ApplyPipeline(VkCommandBuffer cmd) {
    if (currentPipeline_ != lastPipeline_ && currentPipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline_);
        lastPipeline_ = currentPipeline_;
        stats_.pipelineChanges++;
        stats_.totalVulkanCommands++;
    }
}

void VulkanStateCache::ApplyPipelineLayout(VkCommandBuffer cmd) {
    (void)cmd;
    lastLayout_ = currentLayout_;
}

void VulkanStateCache::ApplyDescriptorSet(VkCommandBuffer cmd,
                                            VkPipelineBindPoint bindPoint,
                                            uint32_t firstSet, uint32_t count) {
    bool changed = false;
    for (uint32_t i = firstSet; i < firstSet + count && i < 8; ++i) {
        if (currentDescriptorSet_[i] != lastDescriptorSet_[i]) {
            changed = true;
            break;
        }
    }
    if (changed && currentLayout_ != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, bindPoint, currentLayout_, firstSet, count,
                                 &currentDescriptorSet_[firstSet], 0, nullptr);
        for (uint32_t i = firstSet; i < firstSet + count && i < 8; ++i) {
            lastDescriptorSet_[i] = currentDescriptorSet_[i];
        }
        stats_.descriptorChanges++;
        stats_.totalVulkanCommands++;
    }
}

void VulkanStateCache::ApplyVertexBuffer(VkCommandBuffer cmd, uint32_t binding) {
    if (binding >= 4) return;
    if (currentVertexBuffer_[binding] != lastVertexBuffer_[binding] &&
        currentVertexBuffer_[binding] != VK_NULL_HANDLE) {
        vkCmdBindVertexBuffers(cmd, binding, 1, &currentVertexBuffer_[binding],
                                &currentVertexOffset_[binding]);
        lastVertexBuffer_[binding] = currentVertexBuffer_[binding];
        lastVertexOffset_[binding] = currentVertexOffset_[binding];
        stats_.bufferChanges++;
        stats_.totalVulkanCommands++;
    }
}

void VulkanStateCache::ApplyRenderPass(VkCommandBuffer cmd) {
    (void)cmd;
    if (currentRenderPass_ != lastRenderPass_) {
        lastRenderPass_ = currentRenderPass_;
        stats_.renderPassChanges++;
    }
}

void VulkanStateCache::ApplyFramebuffer(VkCommandBuffer cmd) {
    (void)cmd;
    lastFramebuffer_ = currentFramebuffer_;
}

void VulkanStateCache::Commit() {
}

void VulkanStateCache::InvalidateAll() {
    Reset();
}

} // namespace renderer
} // namespace workstation
