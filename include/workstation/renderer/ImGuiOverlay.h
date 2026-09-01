#pragma once
#include "workstation/renderer/RenderContext.h"
#include "workstation/vulkan/VulkanHeaders.h"
#include "workstation/vulkan/VulkanDevice.h"
#include "workstation/vulkan/VulkanDescriptorManager.h"

#include <memory>

namespace workstation {
namespace renderer {

struct VisibilityDebugStats;

class ImGuiOverlay {
public:
    ~ImGuiOverlay();

    bool Initialize(SDL_Window* window, VkInstance instance,
                    vulkan::VulkanDevice& device,
                    VkRenderPass renderPass, uint32_t imageCount);
    void Shutdown();

    void BeginFrame();
    void Render(VkCommandBuffer commandBuffer);
    void EndFrame();

    void RenderDebugPanel(RenderContext& context);
    void RenderLODPanel(RenderContext& context);
    void RenderGPUPanel(vulkan::VulkanAllocator& allocator);
    void RenderVisualizationPanel(RenderConfig& config);
    void RenderVisibilityPanel(const VisibilityDebugStats& stats, RenderConfig& config);
    void RenderStreamingPanel(RenderContext& context);

    bool IsInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool imguiPool_ = VK_NULL_HANDLE;
    SDL_Window* window_ = nullptr;

    void SetupStyle();
};

} // namespace renderer
} // namespace workstation
