#include "workstation/renderer/Renderer.h"
#include "workstation/renderer/ImGuiOverlay.h"
#include "workstation/renderer/VisibilitySystem.h"
#include "workstation/renderer/VisibilityCache.h"

#include <chrono>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
static std::string GetExeShaderDir() {
    char buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return "shaders/";
    std::string exePath(buf, len);
    auto pos = exePath.find_last_of("\\/");
    if (pos == std::string::npos) return "shaders/";
    return exePath.substr(0, pos + 1) + "shaders/";
}
#endif

namespace workstation {
namespace renderer {

namespace {
// Must stay byte-for-byte in sync with the PushConstants block in
// shaders/point.vert and shaders/point.frag.
struct PointPushConstants {
    float viewProjection[16];   // 64 bytes
    float cameraPosition[4];    // 16 bytes
    float lightDirection[4];    // 16 bytes
    float pointScale;           //  4 bytes
    float pointSize;            //  4 bytes
    uint32_t visualizationMode; //  4 bytes
    float intensityMin;         //  4 bytes
    float intensityMax;         //  4 bytes
    float elevationMin;         //  4 bytes
    float elevationMax;         //  4 bytes
};                              // Total: 124 bytes
static_assert(sizeof(PointPushConstants) == 124,
    "PointPushConstants must be exactly 124 bytes to match GLSL shaders");
} // namespace

Renderer::~Renderer() { Shutdown(); }

bool Renderer::Initialize(const RendererConfig& config) {
    return InitializeInternal(config, nullptr);
}

bool Renderer::InitializeEmbedded(const RendererConfig& config, void* nativeWindowHandle) {
    RendererConfig embeddedConfig = config;
    embeddedConfig.enableImGui = false;
    return InitializeInternal(embeddedConfig, nativeWindowHandle);
}

bool Renderer::InitializeInternal(const RendererConfig& config, void* nativeWindowHandle) {
    config_ = config;
    embedded_ = nativeWindowHandle != nullptr;

    instance_ = std::make_unique<vulkan::VulkanInstance>();
    vulkan::VulkanInstanceConfig instanceConfig{};
    instanceConfig.enableValidation = config.enableValidation;
    instanceConfig.appName = config.appName;

    if (embedded_) {
        instanceConfig.requiredExtensions = {"VK_KHR_surface", "VK_KHR_win32_surface"};
    } else {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            fprintf(stderr, "Renderer::Initialize: SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
        for (const char* ext : instance_->GetRequiredSDLExtensions()) {
            instanceConfig.requiredExtensions.push_back(ext);
        }
    }

    if (!instance_->Initialize(instanceConfig)) {
        fprintf(stderr, "Renderer::Initialize: VulkanInstance::Initialize failed\n");
        return false;
    }

    if (embedded_) {
        if (!CreateSurfaceFromNativeHandle(nativeWindowHandle)) {
            fprintf(stderr, "Renderer::Initialize: CreateSurfaceFromNativeHandle failed\n");
            return false;
        }
    } else {
        if (!CreateSDLWindow()) {
            fprintf(stderr, "Renderer::Initialize: CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }
        if (!CreateSurface()) {
            fprintf(stderr, "Renderer::Initialize: CreateSurface failed: %s\n", SDL_GetError());
            return false;
        }
    }

    device_ = std::make_unique<vulkan::VulkanDevice>();
    if (!device_->Initialize(instance_->GetInstance(), surface_)) {
        fprintf(stderr, "Renderer::Initialize: VulkanDevice::Initialize failed\n");
        return false;
    }

    auto& physDev = device_->GetPhysicalDeviceInfo();
    auto support = physDev.GetSwapchainSupport(surface_);
    auto indices = device_->GetQueueFamilies();

    swapchain_ = std::make_unique<vulkan::VulkanSwapchain>();
    if (!swapchain_->Initialize(device_->GetDevice(), physDev.GetDevice(),
                                 surface_, config.initialWidth, config.initialHeight,
                                 support, indices)) {
        fprintf(stderr, "Renderer::Initialize: VulkanSwapchain::Initialize failed\n");
        return false;
    }

    frameManager_ = std::make_unique<vulkan::VulkanFrameManager>();
    if (!frameManager_->Initialize(device_->GetDevice(), config.maxFramesInFlight,
                                    swapchain_->GetImageCount())) {
        fprintf(stderr, "Renderer::Initialize: VulkanFrameManager::Initialize failed\n");
        return false;
    }

    VmaVulkanFunctions vmaFuncs{};
    vmaFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFuncs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vmaFuncs.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vmaFuncs.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    auto queueFamilies = device_->GetQueueFamilies();
    vulkan::VulkanAllocator::Get().Initialize(
        instance_->GetInstance(), physDev.GetDevice(),
        device_->GetDevice(), device_->GetGraphicsQueue(),
        queueFamilies.graphicsFamily, vmaFuncs);

    descriptorManager_ = std::make_unique<vulkan::VulkanDescriptorManager>();
    descriptorManager_->Initialize(device_->GetDevice());

    pipelineManager_ = std::make_unique<vulkan::VulkanPipelineManager>();
    pipelineManager_->Initialize(device_->GetDevice());

    shaderManager_ = std::make_unique<vulkan::VulkanShaderManager>();
    shaderManager_->Initialize(device_->GetDevice());

    renderPass_ = std::make_unique<vulkan::VulkanRenderPass>();
    vulkan::RenderPassConfig rpConfig{};
    rpConfig.colorFormat = swapchain_->GetImageFormat();
    rpConfig.depthFormat = vulkan::ChooseDepthFormat(physDev.GetDevice());
    rpConfig.loadColorClear = true;
    rpConfig.storeColor = true;
    rpConfig.depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    rpConfig.storeDepth = false;
    renderPass_->Initialize(device_->GetDevice(), rpConfig);

    bufferManager_ = std::make_unique<gpu::GPUBufferManager>();
    bufferManager_->Initialize(vulkan::VulkanAllocator::Get(), config.maxFramesInFlight);

    adapter_.Initialize();
    toolManager_.Initialize();
    visualizationManager_.Initialize();
    debugRenderer_.Initialize();

    CreateFramebuffers();

    uint32_t imageCount = swapchain_->GetImageCount();
    commandBuffers_.resize(imageCount);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamily;

    VkCommandPool tempPool;
    vkCreateCommandPool(device_->GetDevice(), &poolInfo, nullptr, &tempPool);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = tempPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = imageCount;
    vkAllocateCommandBuffers(device_->GetDevice(), &allocInfo, commandBuffers_.data());

    CreateDescriptorResources();
    if (!CreatePointPipeline()) {
        fprintf(stderr, "Renderer::Initialize: CreatePointPipeline failed\n");
        return false;
    }
    if (!CreateDebugPipeline()) {
        fprintf(stderr, "Renderer::Initialize: CreateDebugPipeline failed\n");
        return false;
    }

    context_.SetViewportSize(config.initialWidth, config.initialHeight);
    context_.GetVisibilitySystem().Initialize(&spatialTree_);
    context_.GetLODManager().GetLODCache().Initialize(16384);

    streamingManager_ = std::make_unique<gpu::PointStreamingManager>();
    streamingManager_->Initialize(bufferManager_.get(),
                                   20ULL * 1024 * 1024 * 1024,
                                   4ULL * 1024 * 1024 * 1024);

    if (config.enableImGui) {
        imguiOverlay_ = std::make_unique<ImGuiOverlay>();
        if (imguiOverlay_->Initialize(window_, instance_->GetInstance(), *device_,
                                       renderPass_->GetRenderPass(),
                                       swapchain_->GetImageCount())) {
            useImGui_ = true;
        }
    }

    initialized_ = true;
    lastFrameTime_ = SDL_GetPerformanceCounter();
    return true;
}

void Renderer::Shutdown() {
    if (!initialized_) return;

    device_->WaitIdle();

    for (auto fb : framebuffers_) {
        if (fb) vkDestroyFramebuffer(device_->GetDevice(), fb, nullptr);
    }
    framebuffers_.clear();

    if (pointPipeline_) pipelineManager_->DestroyPipeline(pointPipeline_);
    if (debugPipeline_) pipelineManager_->DestroyPipeline(debugPipeline_);
    if (pointPipelineLayout_) pipelineManager_->DestroyPipelineLayout(pointPipelineLayout_);
    if (pointDescriptorLayout_) descriptorManager_->DestroyLayout(pointDescriptorLayout_);
    if (pointDescriptorPool_) descriptorManager_->DestroyPool(pointDescriptorPool_);

    adapter_.ReleaseAll();

    if (streamingManager_) {
        streamingManager_->Shutdown();
        streamingManager_.reset();
    }

    bufferManager_->Shutdown();
    renderPass_->Shutdown();
    shaderManager_->Shutdown();
    pipelineManager_->Shutdown();
    descriptorManager_->Shutdown();

    if (imguiOverlay_) {
        imguiOverlay_->Shutdown();
        imguiOverlay_.reset();
    }

    vulkan::VulkanAllocator::Get().Shutdown();

    swapchain_->Shutdown();
    frameManager_->Shutdown();
    device_->Shutdown();

    if (surface_) {
        SDL_Vulkan_DestroySurface(instance_->GetInstance(), surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    instance_->Shutdown();
    initialized_ = false;
}

void Renderer::BeginFrame() {
    frameManager_->BeginFrame();
    auto& frame = frameManager_->GetCurrentFrame();

    VkResult result = swapchain_->AcquireNextImage(frame.imageAvailable, &frame.imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        OnResize(context_.GetViewportWidth(), context_.GetViewportHeight());
        frameManager_->EndFrame();
        return;
    }

    vkResetCommandBuffer(commandBuffers_[frame.imageIndex], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffers_[frame.imageIndex], &beginInfo);
    frameStarted_ = true;
}

void Renderer::RenderFrame() {
    if (!frameStarted_) return;

    auto& frame = frameManager_->GetCurrentFrame();
    VkCommandBuffer cmd = commandBuffers_[frame.imageIndex];

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_->GetRenderPass();
    renderPassInfo.framebuffer = framebuffers_[frame.imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain_->GetExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.05f, 0.05f, 0.08f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_->GetExtent().width);
    viewport.height = static_cast<float>(swapchain_->GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_->GetExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    context_.GetStats().drawCalls = 0;
    context_.GetStats().visiblePoints = 0;
    context_.GetStats().visibleNodes = 0;
    visDebugStats_ = {};

    if (activeCloud_) {
        auto& cfg = context_.GetConfig();
        bool useDebug = (cfg.visualizationMode == VisualizationMode::Debug);

        if (cfg.forceDrawAll || useDebug) {
            // Debug mode: bypass visibility/LOD, draw all prepared geometry directly
            VkPipeline activePipeline = useDebug ? debugPipeline_ : pointPipeline_;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
            UpdatePushConstants(cmd);

            for (auto& [key, geo] : adapter_.GetAllPreparedGeometries()) {
                if (!geo || geo->GetPointCount() == 0) continue;

                geo->BindPosition(cmd, 0);
                geo->BindColor(cmd, 1);
                geo->BindIntensity(cmd, 2);
                geo->BindClassification(cmd, 3);
                geo->BindNormal(cmd, 4);

                geo->Draw(cmd);
                context_.GetStats().drawCalls++;
            }
        } else {
            PerformVisibilityCulling();
            PerformLODSelection();
            ProcessStreamingRequests();
            UpdateGPUResidency();

            if (!selectedNodeKeys_.empty()) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointPipeline_);
                UpdatePushConstants(cmd);

                DrawResidentNodes(cmd);
            } else if (!visibleNodeKeys_.empty()) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointPipeline_);
                UpdatePushConstants(cmd);

                DrawVisibleNodes(cmd);
            } else {
                fprintf(stderr, "[Renderer] WARNING: No visible or selected nodes!\n");
            }
        }

        if (useImGui_ && imguiOverlay_) {
            imguiOverlay_->BeginFrame();
            imguiOverlay_->RenderDebugPanel(context_);
            imguiOverlay_->RenderGPUPanel(vulkan::VulkanAllocator::Get());
            imguiOverlay_->RenderStreamingPanel(context_);
            imguiOverlay_->RenderLODPanel(context_);
            imguiOverlay_->RenderVisualizationManagerPanel(visualizationManager_, context_);
            imguiOverlay_->RenderVisibilityPanel(visDebugStats_, context_.GetConfig());
            imguiOverlay_->RenderToolsPanel(toolManager_, context_);
            imguiOverlay_->RenderDebugOverlay(debugRenderer_, context_);

            // VisualizationManager UI changes take effect starting next frame's
            // push constants (matches this loop's existing single-frame latency).
            auto& cfg = context_.GetConfig();
            cfg.visualizationMode = visualizationManager_.GetMode();
            cfg.intensityMin = visualizationManager_.GetIntensityMin();
            cfg.intensityMax = visualizationManager_.GetIntensityMax();

            imguiOverlay_->Render(cmd);
        }
    }
}

void Renderer::EndFrame() {
    if (!frameStarted_) return;

    auto& frame = frameManager_->GetCurrentFrame();
    VkCommandBuffer cmd = commandBuffers_[frame.imageIndex];

    vkCmdEndRenderPass(cmd);

    if (useImGui_ && imguiOverlay_) {
        imguiOverlay_->EndFrame();
    }

    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinished;

    vkQueueSubmit(device_->GetGraphicsQueue(), 1, &submitInfo, frame.inFlightFence);

    VkResult result = swapchain_->Present(frame.renderFinished, frame.imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        OnResize(context_.GetViewportWidth(), context_.GetViewportHeight());
    }

    frameManager_->EndFrame();
    frameStarted_ = false;

    uint64_t now = SDL_GetPerformanceCounter();
    double delta = static_cast<double>(now - lastFrameTime_) / SDL_GetPerformanceFrequency();
    context_.UpdateFrameStats(delta * 1000.0);
    lastFrameTime_ = now;
    frameNumber_++;
}

void Renderer::SetPointCloud(pointcloud::PointCloud* cloud) {
    activeCloud_ = cloud;
    if (cloud) {
        auto* prepGeo = adapter_.PreparePointCloud(*cloud);
        auto* root = cloud->Root();
        if (root) {
            fprintf(stderr, "[Renderer] SetPointCloud: %llu points, cloud='%s'\n",
                    cloud->PointCount(), cloud->Name());

            context_.GetCamera().FocusOnBounds(root->bounds());

            BuildSpatialTreeFromCloud(*cloud);

            LODConfig lodCfg{};
            lodCfg.totalPointBudget = config_.gpuPointBudget;
            lodCfg.visiblePointBudget = config_.gpuPointBudget;
            context_.GetLODManager().SetConfig(lodCfg);

            PointBudgetConfig budgetCfg{};
            budgetCfg.gpuBudget = config_.gpuPointBudget;
            budgetCfg.enforceBudget = true;
            context_.GetPointBudget().SetConfig(budgetCfg);

            if (streamingManager_) {
                streamingManager_->SetActiveCloud(cloud);
            }
        }
    }
}

void Renderer::BuildSpatialTreeFromCloud(pointcloud::PointCloud& cloud) {
    spatialTree_.Clear();

    auto* root = cloud.Root();
    if (!root) return;

    spatialTree_.Insert(0, root->PointCount(), root->bounds());

    visDebugStats_.totalNodes = 1;
}

void Renderer::PerformVisibilityCulling() {
    auto t0 = std::chrono::high_resolution_clock::now();

    auto& visSystem = context_.GetVisibilitySystem();
    auto& visCache = context_.GetVisibilityCache();
    auto& cam = context_.GetCamera();

    auto result = visSystem.ComputeVisibility(
        cam,
        context_.GetViewportWidth(),
        context_.GetViewportHeight(),
        visCache,
        frameNumber_);

    visibleNodeKeys_ = std::move(result.visibleNodeKeys);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    visDebugStats_.totalNodes = result.nodesTested;
    visDebugStats_.visibleNodes = result.nodesPassed;
    visDebugStats_.culledNodes = result.nodesTested - result.nodesPassed;
    visDebugStats_.visiblePoints = result.totalVisiblePoints;
    visDebugStats_.frustumTests = result.nodesTested - result.nodesCached;
    visDebugStats_.cacheHits = result.nodesCached;
    visDebugStats_.visibilityTimeMs = elapsedMs;

    context_.GetStats().visibleNodes = result.nodesPassed;
    context_.GetStats().visiblePoints = result.totalVisiblePoints;
}

void Renderer::DrawVisibleNodes(VkCommandBuffer cmd) {
    for (uint64_t key : visibleNodeKeys_) {
        auto* geo = adapter_.GetPreparedGeometry(key);
        if (!geo || geo->GetPointCount() == 0) continue;

        geo->BindPosition(cmd, 0);
        geo->BindColor(cmd, 1);
        geo->BindIntensity(cmd, 2);
        geo->BindClassification(cmd, 3);
        geo->BindNormal(cmd, 4);

        geo->Draw(cmd);

        context_.GetStats().drawCalls++;
    }
}

void Renderer::PerformLODSelection() {
    auto t0 = std::chrono::high_resolution_clock::now();

    auto& lodManager = context_.GetLODManager();
    auto& visCache = context_.GetVisibilityCache();
    auto& pointBudget = context_.GetPointBudget();
    auto& cam = context_.GetCamera();
    auto& cfg = context_.GetConfig();
    auto& lodDebug = context_.GetLODDebugStats();

    lodManager.GetConfig().lodEnabled = cfg.lodEnabled;
    lodManager.GetConfig().forceLODLevel = cfg.forceLODLevel;
    lodManager.GetConfig().visiblePointBudget =
        static_cast<uint64_t>(cfg.pointBudgetMillions * 1'000'000.0);

    pointBudget.SetConfig({cfg.maxPointsPerFrame, 100'000'000, 1000, true});
    pointBudget.BeginFrame();

    auto result = lodManager.SelectNodes(
        visibleNodeKeys_,
        cam,
        context_.GetViewportWidth(),
        context_.GetViewportHeight(),
        pointBudget,
        activeCloud_,
        visCache,
        frameNumber_);

    selectedNodeKeys_ = std::move(result.selectedNodes);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    lodDebug.visibleNodes = static_cast<uint32_t>(visibleNodeKeys_.size());
    lodDebug.selectedNodes = result.selectedNodeCount;
    lodDebug.rejectedNodes = result.rejectedNodeCount;
    lodDebug.visiblePoints = context_.GetStats().visiblePoints;
    lodDebug.renderedPoints = result.selectedPointCount;
    lodDebug.pointBudget = cfg.maxPointsPerFrame;
    lodDebug.averageSSE = result.averageSSE;
    lodDebug.currentLOD = result.maxLOD;
    lodDebug.lodTimeMs = elapsedMs;
}

void Renderer::DrawSelectedNodes(VkCommandBuffer cmd) {
    for (uint64_t key : selectedNodeKeys_) {
        auto* geo = adapter_.GetPreparedGeometry(key);
        if (!geo || geo->GetPointCount() == 0) continue;

        geo->BindPosition(cmd, 0);
        geo->BindColor(cmd, 1);
        geo->BindIntensity(cmd, 2);
        geo->BindClassification(cmd, 3);
        geo->BindNormal(cmd, 4);

        geo->Draw(cmd);

        context_.GetStats().drawCalls++;
    }
}

void Renderer::ProcessStreamingRequests() {
    if (!streamingManager_) return;

    auto& lodManager = context_.GetLODManager();
    auto& selectedNodes = lodManager.GetSelectedNodes();
    auto& streamingStats = context_.GetStreamingDebugStats();

    for (const auto& node : selectedNodes) {
        // Nodes the adapter already uploaded directly (PreparePointCloud)
        // are drawable without the streaming pipeline; requesting them here
        // every frame forever (since they never become "resident" from the
        // streaming manager's point of view) wastes CPU/GPU work each frame.
        if (adapter_.GetPreparedGeometry(node.nodeKey) != nullptr) continue;

        if (!streamingManager_->IsNodeResident(node.nodeKey)) {
            float priority = static_cast<float>(node.screenSpaceError) * 1000.0f;
            streamingManager_->RequestNode(node.nodeKey, priority,
                                            node.pointCount,
                                            static_cast<float>(node.screenSpaceError));
        }
    }

    streamingManager_->Update(frameNumber_);
    streamingStats = streamingManager_->GetDebugStats();
}

void Renderer::UpdateGPUResidency() {
    if (!streamingManager_) return;

    auto& streamingStats = context_.GetStreamingDebugStats();
    streamingStats = streamingManager_->GetDebugStats();
}

void Renderer::DrawResidentNodes(VkCommandBuffer cmd) {
    for (uint64_t key : selectedNodeKeys_) {
        auto* geo = adapter_.GetPreparedGeometry(key);
        if (!geo || geo->GetPointCount() == 0) continue;

        // Geometry prepared by the adapter (via PreparePointCloud) is already
        // uploaded to GPU buffers and is always drawable. The streaming
        // manager's residency check applies only to nodes that depend on the
        // streaming pipeline for upload — adapter-prepared nodes do not.
        if (streamingManager_ &&
            !streamingManager_->IsNodeResident(key) &&
            adapter_.GetPreparedGeometry(key) == nullptr) {
            continue;
        }

        geo->BindPosition(cmd, 0);
        geo->BindColor(cmd, 1);
        geo->BindIntensity(cmd, 2);
        geo->BindClassification(cmd, 3);
        geo->BindNormal(cmd, 4);

        geo->Draw(cmd);

        context_.GetStats().drawCalls++;
    }
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    device_->WaitIdle();
    context_.SetViewportSize(width, height);

    auto& physDev = device_->GetPhysicalDeviceInfo();
    auto support = physDev.GetSwapchainSupport(surface_);

    swapchain_->Recreate(width, height);
    swapchain_->Initialize(device_->GetDevice(), physDev.GetDevice(),
                            surface_, width, height, support,
                            device_->GetQueueFamilies());

    for (auto fb : framebuffers_) {
        if (fb) vkDestroyFramebuffer(device_->GetDevice(), fb, nullptr);
    }
    CreateFramebuffers();
}

bool Renderer::CreateSDLWindow() {
    window_ = SDL_CreateWindow(
        config_.appName.c_str(),
        config_.initialWidth, config_.initialHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    return window_ != nullptr;
}

bool Renderer::CreateSurface() {
    return SDL_Vulkan_CreateSurface(window_, instance_->GetInstance(), nullptr, &surface_);
}

bool Renderer::CreateSurfaceFromNativeHandle(void* nativeWindowHandle) {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = static_cast<HWND>(nativeWindowHandle);
    createInfo.hinstance = GetModuleHandle(nullptr);
    return vkCreateWin32SurfaceKHR(instance_->GetInstance(), &createInfo, nullptr, &surface_) == VK_SUCCESS;
}

bool Renderer::CreateRenderPass() { return true; }

bool Renderer::CreateFramebuffers() {
    framebuffers_.resize(swapchain_->GetImageCount());
    for (uint32_t i = 0; i < swapchain_->GetImageCount(); ++i) {
        std::array<VkImageView, 2> attachments = {
            swapchain_->GetImageView(i),
            swapchain_->GetDepthImageView()
        };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass_->GetRenderPass();
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = swapchain_->GetExtent().width;
        fbInfo.height = swapchain_->GetExtent().height;
        fbInfo.layers = 1;

        vkCreateFramebuffer(device_->GetDevice(), &fbInfo, nullptr, &framebuffers_[i]);
    }
    return true;
}

bool Renderer::CreatePointPipeline() {
#ifdef _WIN32
    std::string shaderDir = GetExeShaderDir();
#else
    const char* basePath = SDL_GetBasePath();
    std::string shaderDir = basePath ? std::string(basePath) + "shaders/" : "shaders/";
#endif
    fprintf(stderr, "[Renderer] Shader dir: %s\n", shaderDir.c_str());
    fflush(stderr);

    auto shaders = shaderManager_->LoadSPIRVFiles({
        {shaderDir + "point.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {shaderDir + "point.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    for (auto& shader : shaders) {
        if (shader.module == VK_NULL_HANDLE) {
            fprintf(stderr, "Renderer::CreatePointPipeline: failed to load required shaders "
                            "(run wk_renderer.exe from the build-gui directory so shaders/*.spv resolve)\n");
            return false;
        }
    }

    VkDescriptorSetLayout layout = descriptorManager_->CreateLayout({});

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PointPushConstants);


    pointPipelineLayout_ = pipelineManager_->CreatePipelineLayout({layout}, {pushRange});
    pointDescriptorLayout_ = layout;

    vulkan::PipelineConfig pconfig;
    pconfig.SetDefaults();
    pconfig.colorFormat = swapchain_->GetImageFormat();
    pconfig.depthFormat = vulkan::ChooseDepthFormat(device_->GetPhysicalDeviceInfo().GetDevice());

    pointPipeline_ = pipelineManager_->CreateGraphicsPipeline(
        pointPipelineLayout_, shaders, pconfig, renderPass_->GetRenderPass());

    for (auto& shader : shaders) {
        shaderManager_->DestroyShaderModule(shader.module);
    }

    return pointPipeline_ != VK_NULL_HANDLE;
}

bool Renderer::CreateDebugPipeline() {
    // Debug pipeline uses the same shaders but with depth test OFF, cull mode NONE.
    // Activated when visualizationMode == Debug via push constants.
#ifdef _WIN32
    std::string shaderDir = GetExeShaderDir();
#else
    const char* basePath = SDL_GetBasePath();
    std::string shaderDir = basePath ? std::string(basePath) + "shaders/" : "shaders/";
#endif

    auto shaders = shaderManager_->LoadSPIRVFiles({
        {shaderDir + "point.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {shaderDir + "point.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    for (auto& shader : shaders) {
        if (shader.module == VK_NULL_HANDLE) {
            fprintf(stderr, "Renderer::CreateDebugPipeline: failed to load shaders\n");
            return false;
        }
    }

    // Reuse the same pipeline layout (same push constants)
    vulkan::PipelineConfig pconfig;
    pconfig.SetDefaults();
    pconfig.colorFormat = swapchain_->GetImageFormat();
    // Depth test OFF — points always pass
    pconfig.depthStencil.depthTestEnable = VK_FALSE;
    pconfig.depthStencil.depthWriteEnable = VK_FALSE;
    // No culling
    pconfig.rasterizer.cullMode = VK_CULL_MODE_NONE;
    // Wider points for visibility
    pconfig.rasterizer.lineWidth = 1.0f;

    debugPipeline_ = pipelineManager_->CreateGraphicsPipeline(
        pointPipelineLayout_, shaders, pconfig, renderPass_->GetRenderPass());

    for (auto& shader : shaders) {
        shaderManager_->DestroyShaderModule(shader.module);
    }

    fprintf(stderr, "[Renderer] Debug pipeline created: %s\n",
            debugPipeline_ != VK_NULL_HANDLE ? "OK" : "FAILED");
    fflush(stderr);
    return debugPipeline_ != VK_NULL_HANDLE;
}

bool Renderer::CreateDescriptorResources() {
    return true;
}

void Renderer::UpdatePushConstants(VkCommandBuffer cmd) {
    auto& cam = context_.GetCamera();
    auto& cfg = context_.GetConfig();

    PointPushConstants pc = {};

    // Matrix4d stores row-major (m_[row][col]). GLSL mat4 is column-major.
    // Copy column-by-column so the shader reads the correct orientation.
    const auto& vp = cam.GetViewProjectionMatrix();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            pc.viewProjection[c * 4 + r] = static_cast<float>(vp(r, c));
        }

    auto pos = cam.GetPosition();
    pc.cameraPosition[0] = static_cast<float>(pos.x);
    pc.cameraPosition[1] = static_cast<float>(pos.y);
    pc.cameraPosition[2] = static_cast<float>(pos.z);
    pc.lightDirection[0] = 0.3f;
    pc.lightDirection[1] = -0.7f;
    pc.lightDirection[2] = 0.5f;

    pc.pointScale = static_cast<float>(context_.GetViewportHeight()) * 0.5f;
    pc.pointSize = cfg.pointSize;
    pc.visualizationMode = static_cast<uint32_t>(cfg.visualizationMode);
    pc.intensityMin = cfg.intensityMin;
    pc.intensityMax = cfg.intensityMax;
    pc.elevationMin = cfg.elevationMin;
    pc.elevationMax = cfg.elevationMax;

    vkCmdPushConstants(cmd, pointPipelineLayout_,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(pc), &pc);
}

} // namespace renderer
} // namespace workstation
