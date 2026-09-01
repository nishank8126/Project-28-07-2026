#pragma once
#include "workstation/renderer/PointCloudRenderAdapter.h"
#include "workstation/renderer/RenderContext.h"
#include "workstation/renderer/RenderQueue.h"
#include "workstation/renderer/ImGuiOverlay.h"
#include "workstation/renderer/VisualizationManager.h"
#include "workstation/renderer/DebugRenderer.h"
#include "workstation/tools/ToolManager.h"
#include "workstation/gpu/PointStreamingManager.h"
#include "workstation/spatial/SpatialTree.h"

#include "workstation/vulkan/VulkanInstance.h"
#include "workstation/vulkan/VulkanDevice.h"
#include "workstation/vulkan/VulkanSwapchain.h"
#include "workstation/vulkan/VulkanFrameManager.h"
#include "workstation/vulkan/VulkanDescriptorManager.h"
#include "workstation/vulkan/VulkanPipelineManager.h"
#include "workstation/vulkan/VulkanShaderManager.h"
#include "workstation/vulkan/VulkanRenderPass.h"

#include <memory>
#include <vector>

namespace workstation {
namespace renderer {

struct RendererConfig {
    std::string appName = "NakshaPointEngine";
    uint32_t initialWidth = 1920;
    uint32_t initialHeight = 1080;
    bool enableValidation = true;
    bool enableVSync = true;
    bool enableImGui = false;
    uint32_t maxFramesInFlight = 2;
    uint64_t gpuPointBudget = 50'000'000;
};

struct VisibilityDebugStats {
    uint32_t totalNodes = 0;
    uint32_t visibleNodes = 0;
    uint32_t culledNodes = 0;
    uint64_t visiblePoints = 0;
    uint32_t frustumTests = 0;
    uint32_t cacheHits = 0;
    double visibilityTimeMs = 0.0;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(const RendererConfig& config);

    // Initializes against an externally-owned native window (e.g. a Qt
    // widget's HWND) instead of creating our own SDL window. ImGui is
    // forced off in this mode since it depends on the SDL backend; the
    // embedding host is expected to supply its own UI (e.g. Qt panels).
    bool InitializeEmbedded(const RendererConfig& config, void* nativeWindowHandle);

    void Shutdown();

    void BeginFrame();
    void RenderFrame();
    void EndFrame();

    void OnResize(uint32_t width, uint32_t height);

    void SetPointCloud(pointcloud::PointCloud* cloud);

    RenderContext& GetContext() { return context_; }
    PointCloudRenderAdapter& GetAdapter() { return adapter_; }
    gpu::GPUBufferManager& GetBufferManager() { return *bufferManager_; }
    const VisibilityDebugStats& GetVisibilityStats() const { return visDebugStats_; }

    bool IsInitialized() const { return initialized_; }

    void SetImGuiEnabled(bool enabled) { useImGui_ = enabled; }
    bool IsImGuiEnabled() const { return useImGui_; }

private:
    bool initialized_ = false;
    bool embedded_ = false;
    RendererConfig config_;
    RenderContext context_;

    SDL_Window* window_ = nullptr;

    std::unique_ptr<vulkan::VulkanInstance> instance_;
    std::unique_ptr<vulkan::VulkanDevice> device_;
    std::unique_ptr<vulkan::VulkanSwapchain> swapchain_;
    std::unique_ptr<vulkan::VulkanFrameManager> frameManager_;
    std::unique_ptr<vulkan::VulkanDescriptorManager> descriptorManager_;
    std::unique_ptr<vulkan::VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<vulkan::VulkanShaderManager> shaderManager_;
    std::unique_ptr<vulkan::VulkanRenderPass> renderPass_;

    std::unique_ptr<gpu::GPUBufferManager> bufferManager_;
    std::unique_ptr<gpu::PointStreamingManager> streamingManager_;

    PointCloudRenderAdapter adapter_;
    tools::ToolManager toolManager_;
    VisualizationManager visualizationManager_;
    DebugRenderer debugRenderer_;

    spatial::SpatialTree spatialTree_;
    std::vector<uint64_t> visibleNodeKeys_;
    std::vector<uint64_t> selectedNodeKeys_;
    VisibilityDebugStats visDebugStats_ = {};

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkPipeline pointPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pointPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout pointDescriptorLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool pointDescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> pointDescriptorSets_;

    pointcloud::PointCloud* activeCloud_ = nullptr;

    uint64_t lastFrameTime_ = 0;
    uint32_t frameNumber_ = 0;
    bool frameStarted_ = false;
    bool useImGui_ = false;

    std::unique_ptr<ImGuiOverlay> imguiOverlay_;

    bool InitializeInternal(const RendererConfig& config, void* nativeWindowHandle);
    bool CreateSDLWindow();
    bool CreateSurface();
    bool CreateSurfaceFromNativeHandle(void* nativeWindowHandle);
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreatePointPipeline();
    bool CreateDescriptorResources();

    void UpdatePushConstants(VkCommandBuffer cmd);
    void BuildSpatialTreeFromCloud(pointcloud::PointCloud& cloud);
    void PerformVisibilityCulling();
    void PerformLODSelection();
    void ProcessStreamingRequests();
    void UpdateGPUResidency();
    void DrawVisibleNodes(VkCommandBuffer cmd);
    void DrawSelectedNodes(VkCommandBuffer cmd);
    void DrawResidentNodes(VkCommandBuffer cmd);

    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;
};

} // namespace renderer
} // namespace workstation
