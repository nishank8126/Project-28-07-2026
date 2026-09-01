#pragma once
#include "workstation/renderer/RenderContext.h"
#include "workstation/vulkan/VulkanHeaders.h"
#include "workstation/vulkan/VulkanDevice.h"
#include "workstation/vulkan/VulkanDescriptorManager.h"

#include <memory>

namespace workstation {

namespace tools {
class ToolManager;
}

namespace renderer {

struct VisibilityDebugStats;
class DebugRenderer;
class VisualizationManager;

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
    void RenderVisibilityPanel(const VisibilityDebugStats& stats, RenderConfig& config);
    void RenderStreamingPanel(RenderContext& context);
    void RenderToolsPanel(tools::ToolManager& toolManager, RenderContext& context);
    void RenderDebugOverlay(DebugRenderer& debugRenderer, RenderContext& context);
    void RenderVisualizationManagerPanel(VisualizationManager& vizManager, RenderContext& context);

    bool IsInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
    VkDevice device_ = VK_NULL_HANDLE;
    SDL_Window* window_ = nullptr;

    void SetupStyle();
};

} // namespace renderer
} // namespace workstation
