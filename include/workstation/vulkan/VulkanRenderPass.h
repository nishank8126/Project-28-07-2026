#pragma once
#include "workstation/vulkan/VulkanHeaders.h"

namespace workstation {
namespace vulkan {

struct RenderPassConfig {
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits colorSamples = VK_SAMPLE_COUNT_1_BIT;
    VkSampleCountFlagBits depthSamples = VK_SAMPLE_COUNT_1_BIT;
    bool loadColorClear = true;
    bool storeColor = true;
    bool loadDepthClear = true;
    bool storeDepth = false;
    VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
};

class VulkanRenderPass {
public:
    ~VulkanRenderPass();

    bool Initialize(VkDevice device, const RenderPassConfig& config);
    void Shutdown();

    VkRenderPass GetRenderPass() const { return renderPass_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace workstation
