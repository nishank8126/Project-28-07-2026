#pragma once
#include "workstation/renderer/PointCloudRenderAdapter.h"
#include "workstation/renderer/RenderContext.h"
#include "workstation/renderer/RenderQueue.h"
#include "workstation/renderer/ImGuiOverlay.h"
#include "workstation/renderer/VisualizationManager.h"
#include "workstation/renderer/DebugRenderer.h"
#include "workstation/renderer/BenchmarkTimer.h"
#include "workstation/renderer/CadRenderer.h"
#include "workstation/scene/SceneManager.h"
#include "workstation/tools/ToolManager.h"
#include "workstation/gpu/PointStreamingManager.h"
#include "workstation/spatial/SpatialTree.h"
#include "workstation/spatial/CoordinateNormalizationManager.h"
#include "workstation/surface/SurfaceRenderer.h"
#include "workstation/display/DisplayModeManager.h"

#include "workstation/vulkan/VulkanInstance.h"
#include "workstation/vulkan/VulkanDevice.h"
#include "workstation/vulkan/VulkanSwapchain.h"
#include "workstation/vulkan/VulkanFrameManager.h"
#include "workstation/vulkan/VulkanDescriptorManager.h"
#include "workstation/vulkan/VulkanPipelineManager.h"
#include "workstation/vulkan/VulkanShaderManager.h"
#include "workstation/vulkan/VulkanRenderPass.h"
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/pointcloud/SntFileReader.h"
#include "workstation/cad/AttachmentManager.h"

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

    // Re-uploads just the active cloud's Normals channel to the already
    // -prepared GPU geometry, for when normals are computed lazily after
    // the cloud is already loaded/rendering (see NormalEstimator.h). Does
    // nothing if there's no active cloud or its root has no Normals channel.
    void RefreshNormals();

    // Re-uploads the active cloud's Classification channel to the already
    // -prepared GPU geometry. Called after ClassificationTool's manual edits
    // or automated Classify* passes write new codes into the CPU-side
    // channel, since that write alone doesn't touch the GPU buffer built at
    // load time.
    void RefreshClassification();

    // Uploads .snt (or similar) 2-D/3-D vector CAD geometry as a persistent
    // line overlay drawn on top of the point cloud each frame. Pass an
    // empty SntEntities to clear the overlay.
    void SetVectorOverlay(const pointcloud::SntEntities& entities);

    // CAD attachment integration
    CadRenderer& GetCadRenderer() { return cadRenderer_; }
    const CadRenderer& GetCadRenderer() const { return cadRenderer_; }

    surface::SurfaceRenderer& GetSurfaceRenderer() { return surfaceRenderer_; }
    const surface::SurfaceRenderer& GetSurfaceRenderer() const { return surfaceRenderer_; }

    scene::SceneManager& GetSceneManager() { return sceneManager_; }
    const scene::SceneManager& GetSceneManager() const { return sceneManager_; }

    void LoadDxfAttachment(cad::DxfAttachment* attachment);
    void LoadDwgAttachment(cad::DwgAttachment* attachment);
    void LoadSntAttachment(cad::SntAttachment* attachment);
    void RemoveDxfAttachment(cad::DxfAttachment* attachment);
    void RemoveDwgAttachment(cad::DwgAttachment* attachment);
    void RemoveSntAttachment(cad::SntAttachment* attachment);
    void RemoveAllCadAttachments();
    void SetCadLayerVisibility(const std::string& layerName, bool visible);

    void LoadClassificationPTC(const std::string& filepath, std::string* error = nullptr);
    void SetCustomClassificationPalette(const display::ClassPalette& palette);
    void ClearCustomClassificationPalette();
    bool HasCustomPalette() const { return hasCustomPalette_; }
    const display::ClassPalette& GetCustomPalette() const { return customPalette_; }
    void UpdateClassificationVisibility(int classCode, bool visible);

    RenderContext& GetContext() { return context_; }
    PointCloudRenderAdapter& GetAdapter() { return adapter_; }
    gpu::GPUBufferManager& GetBufferManager() { return *bufferManager_; }
    const VisibilityDebugStats& GetVisibilityStats() const { return visDebugStats_; }
    spatial::CoordinateNormalizationManager& GetCoordNormalizer() { return coordNormalizer_; }

    bool IsInitialized() const { return initialized_; }

    void SetImGuiEnabled(bool enabled) { useImGui_ = enabled; }
    bool IsImGuiEnabled() const { return useImGui_; }

    // Benchmark
    void StartBenchmark(uint32_t frames = 300);
    bool IsBenchmarkRunning() const { return benchmark_.IsRunning(); }
    const BenchmarkTimer& GetBenchmarkTimer() const { return benchmark_; }

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
    CadRenderer cadRenderer_;
    surface::SurfaceRenderer surfaceRenderer_;
    scene::SceneManager sceneManager_;
    tools::ToolManager toolManager_;
    VisualizationManager visualizationManager_;
    DebugRenderer debugRenderer_;
    BenchmarkTimer benchmark_;

    spatial::SpatialTree spatialTree_;
    spatial::CoordinateNormalizationManager coordNormalizer_;
    std::vector<uint64_t> visibleNodeKeys_;
    std::vector<uint64_t> selectedNodeKeys_;
    VisibilityDebugStats visDebugStats_ = {};

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkPipeline pointPipeline_ = VK_NULL_HANDLE;
    VkPipeline debugPipeline_ = VK_NULL_HANDLE;
    VkPipeline linePipeline_ = VK_NULL_HANDLE;
    VkPipeline cadLinePipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pointPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout pointDescriptorLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool pointDescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> pointDescriptorSets_;

    surface::SurfaceInitParams surfaceInitParams_;

    vulkan::GPUBuffer classificationBuffer_;
    VkDescriptorSet classificationDescriptorSet_ = VK_NULL_HANDLE;
    bool hasCustomPalette_ = false;
    display::ClassPalette customPalette_;

    pointcloud::PointCloud* activeCloud_ = nullptr;

    vulkan::GPUBuffer overlayVertexBuffer_;
    uint32_t overlayVertexCount_ = 0;

    uint64_t lastFrameTime_ = 0;
    uint32_t frameNumber_ = 0;
    uint32_t cloudLoadedAtFrame_ = 0;
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
    bool CreateDebugPipeline();
    bool CreateLinePipeline();
    bool CreateCadLinePipeline();
    bool CreateDescriptorResources();
    void DrawVectorOverlay(VkCommandBuffer cmd);
    void DrawCadGeometry(VkCommandBuffer cmd);
    void DrawSurface(VkCommandBuffer cmd);

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
