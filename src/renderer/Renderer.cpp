#include "workstation/renderer/Renderer.h"
#include "workstation/renderer/ImGuiOverlay.h"
#include "workstation/renderer/VisibilitySystem.h"
#include "workstation/renderer/VisibilityCache.h"

#include <chrono>

namespace workstation {
namespace renderer {

Renderer::~Renderer() { Shutdown(); }

bool Renderer::Initialize(const RendererConfig& config) {
    config_ = config;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

    instance_ = std::make_unique<vulkan::VulkanInstance>();
    vulkan::VulkanInstanceConfig instanceConfig{};
    instanceConfig.enableValidation = config.enableValidation;
    instanceConfig.appName = config.appName;
    instanceConfig.requiredExtensions = instance_->GetRequiredSDLExtensions();
    if (!instance_->Initialize(instanceConfig)) return false;

    if (!CreateWindow()) return false;
    if (!CreateSurface()) return false;

    device_ = std::make_unique<vulkan::VulkanDevice>();
    if (!device_->Initialize(instance_->GetInstance(), surface_)) return false;

    auto& physDev = device_->GetPhysicalDeviceInfo();
    auto support = physDev.GetSwapchainSupport(surface_);
    auto indices = device_->GetQueueFamilies();

    swapchain_ = std::make_unique<vulkan::VulkanSwapchain>();
    if (!swapchain_->Initialize(device_->GetDevice(), physDev.GetDevice(),
                                 surface_, config.initialWidth, config.initialHeight,
                                 support, indices)) return false;

    frameManager_ = std::make_unique<vulkan::VulkanFrameManager>();
    if (!frameManager_->Initialize(device_->GetDevice(), config.maxFramesInFlight,
                                    swapchain_->GetImageCount())) return false;

    VmaVulkanFunctions vmaFuncs{};
    vmaFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFuncs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vmaFuncs.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vmaFuncs.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkan::VulkanAllocator::Get().Initialize(
        instance_->GetInstance(), physDev.GetDevice(),
        device_->GetDevice(), vmaFuncs);

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
    CreatePointPipeline();

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
        }

        if (useImGui_ && imguiOverlay_) {
            imguiOverlay_->BeginFrame();
            imguiOverlay_->RenderDebugPanel(context_);
            imguiOverlay_->RenderVisibilityPanel(visDebugStats_, context_.GetConfig());
            imguiOverlay_->RenderLODPanel(context_);
            imguiOverlay_->RenderStreamingPanel(context_);
            imguiOverlay_->RenderVisualizationPanel(context_.GetConfig());
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
        adapter_.PreparePointCloud(*cloud);
        auto* root = cloud->Root();
        if (root) {
            context_.GetCamera().FocusOnBounds(root->bounds());
            BuildSpatialTreeFromCloud(*cloud);

            LODConfig lodCfg{};
            lodCfg.totalPointBudget = config_.gpuPointBudget;
            lodCfg.visiblePointBudget = config_.gpuPointBudget;
            context_.GetLODManager().SetConfig(lodCfg);

            ViewportPointBudget::PointBudgetConfig budgetCfg{};
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
    spatialTree_ = spatial::SpatialTree();

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

    pointBudget.SetConfig({cfg.maxPointsPerFrame, 100'000'000, 1000, cfg.lodEnabled});

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
    if (!streamingManager_) {
        DrawSelectedNodes(cmd);
        return;
    }

    for (uint64_t key : selectedNodeKeys_) {
        if (!streamingManager_->IsNodeResident(key)) continue;

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

bool Renderer::CreateWindow() {
    window_ = SDL_CreateWindow(
        config_.appName.c_str(),
        config_.initialWidth, config_.initialHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    return window_ != nullptr;
}

bool Renderer::CreateSurface() {
    return SDL_Vulkan_CreateSurface(window_, instance_->GetInstance(), &surface_);
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
    auto shaders = shaderManager_->LoadSPIRVFiles({
        {"shaders/point.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/point.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
    });

    VkDescriptorSetLayout layout = descriptorManager_->CreateLayout({});

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = 128;

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

bool Renderer::CreateDescriptorResources() {
    return true;
}

void Renderer::UpdatePushConstants(VkCommandBuffer cmd) {
    auto& cam = context_.GetCamera();
    auto& cfg = context_.GetConfig();

    struct PushConstants {
        float viewProjection[16];
        float view[16];
        float projection[16];
        float cameraPosition[4];
        float cameraDirection[4];
        float lightDirection[4];
        float pointScale;
        float pointSize;
        uint32_t visualizationMode;
        float intensityMin;
        float intensityMax;
        float elevationMin;
        float elevationMax;
        uint32_t padding0;
        uint32_t padding1;
    } pc = {};

    const auto& vp = cam.GetViewProjectionMatrix();
    const auto& v = cam.GetViewMatrix();
    const auto& p = cam.GetProjectionMatrix();
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            pc.viewProjection[r * 4 + c] = static_cast<float>(vp(r, c));
            pc.view[r * 4 + c] = static_cast<float>(v(r, c));
            pc.projection[r * 4 + c] = static_cast<float>(p(r, c));
        }

    auto pos = cam.GetPosition();
    auto fwd = cam.GetForward();
    pc.cameraPosition[0] = static_cast<float>(pos.x);
    pc.cameraPosition[1] = static_cast<float>(pos.y);
    pc.cameraPosition[2] = static_cast<float>(pos.z);
    pc.cameraDirection[0] = static_cast<float>(fwd.x);
    pc.cameraDirection[1] = static_cast<float>(fwd.y);
    pc.cameraDirection[2] = static_cast<float>(fwd.z);
    pc.lightDirection[0] = 0.3f;
    pc.lightDirection[1] = -0.7f;
    pc.lightDirection[2] = 0.5f;

    pc.pointScale = static_cast<float>(context_.GetViewportHeight()) * 0.5f;
    pc.pointSize = cfg.pointSize;
    pc.visualizationMode = static_cast<uint32_t>(cfg.visualizationMode);
    pc.intensityMin = cfg.intensityMin;
    pc.intensityMax = cfg.intensityMax;
    pc.elevationMin = -50.0f;
    pc.elevationMax = 50.0f;

    vkCmdPushConstants(cmd, pointPipelineLayout_,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(pc), &pc);
}

} // namespace renderer
} // namespace workstation
