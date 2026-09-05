#include "workstation/renderer/Renderer.h"
#include "workstation/renderer/ImGuiOverlay.h"
#include "workstation/renderer/VisibilitySystem.h"
#include "workstation/renderer/VisibilityCache.h"
#include "workstation/surface/SurfaceLog.h"

#include <chrono>
#include <cstdio>
#include <cstring>

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
    float depthMin;             //  4 bytes
    float depthMax;             //  4 bytes
    float surfaceAmbient;       //  4 bytes
    float surfaceDiffuse;       //  4 bytes
    float surfaceSpecular;      //  4 bytes
    float surfaceShininess;     //  4 bytes
    float edlStrength;          //  4 bytes
    uint32_t hasCustomPalette;  //  4 bytes (1 = use SSBO classification colors)
};                              // Total: 156 bytes
static_assert(sizeof(PointPushConstants) == 156,
    "PointPushConstants must be exactly 156 bytes to match GLSL shaders");
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
    if (auto* classificationTool = toolManager_.GetClassificationTool()) {
        classificationTool->SetRefreshCallback([this]() { RefreshClassification(); });
    }
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
    if (!CreateLinePipeline()) {
        fprintf(stderr, "Renderer::Initialize: CreateLinePipeline failed\n");
        return false;
    }
    if (!CreateCadLinePipeline()) {
        fprintf(stderr, "Renderer::Initialize: CreateCadLinePipeline failed\n");
        return false;
    }

    cadRenderer_.Initialize(&vulkan::VulkanAllocator::Get());
    cadRenderer_.SetCoordinateNormalizer(&coordNormalizer_);

    surfaceInitParams_.device = device_.get();
    surfaceInitParams_.allocator = &vulkan::VulkanAllocator::Get();
    surfaceInitParams_.pipelineManager = pipelineManager_.get();
    surfaceInitParams_.shaderManager = shaderManager_.get();
    surfaceInitParams_.descriptorManager = descriptorManager_.get();
    surfaceInitParams_.swapchain = swapchain_.get();
    surfaceInitParams_.renderPass = renderPass_.get();
    surfaceRenderer_.Initialize(surfaceInitParams_);

    sceneManager_.Initialize();
    sceneManager_.SetCadRenderer(&cadRenderer_);

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
    if (linePipeline_) pipelineManager_->DestroyPipeline(linePipeline_);
    if (pointPipelineLayout_) pipelineManager_->DestroyPipelineLayout(pointPipelineLayout_);
    if (overlayVertexBuffer_.IsValid()) {
        vulkan::VulkanAllocator::Get().DestroyBuffer(overlayVertexBuffer_);
    }
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
    surfaceRenderer_.Shutdown();
    sceneManager_.Shutdown();
    cadRenderer_.Shutdown();
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
    benchmark_.BeginFrame();

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

    auto frameStart = std::chrono::high_resolution_clock::now();

    context_.GetStats().drawCalls = 0;
    context_.GetStats().visiblePoints = 0;
    context_.GetStats().visibleNodes = 0;
    context_.GetStats().gpuComputeTimeMs = 0.0;
    context_.GetStats().gpuRenderTimeMs = 0.0;
    context_.GetStats().culledNodes = 0;
    context_.GetStats().indirectDraws = 0;
    visDebugStats_ = {};

    // Surface/Wireframe/ShadedSurface modes replace the raw point rendering
    // entirely - only Points and Hybrid still want the point pipeline drawn.
    // This was never gated before: the point cloud (potentially millions of
    // points) was always drawn regardless of display mode, so it visually
    // swamped a much sparser generated mesh (capped at 15,000 points) sitting
    // underneath it, making the surface effectively invisible even though it
    // really was being generated and rendered.
    bool wantPointDraw = true;
    if (surfaceRenderer_.IsVisible() && surfaceRenderer_.GetMeshCount() > 0) {
        auto surfMode = surfaceRenderer_.GetMode();
        wantPointDraw = (surfMode == surface::SurfaceMode::Points ||
                          surfMode == surface::SurfaceMode::Hybrid);
    }

    if (activeCloud_ && wantPointDraw) {
        auto& cfg = context_.GetConfig();
        bool useDebug = (cfg.visualizationMode == VisualizationMode::Debug);

        if (cfg.forceDrawAll || useDebug) {
            // Debug mode: bypass visibility/LOD, draw all prepared geometry directly
            VkPipeline activePipeline = useDebug ? debugPipeline_ : pointPipeline_;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
            if (classificationDescriptorSet_ != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pointPipelineLayout_, 0, 1, &classificationDescriptorSet_, 0, nullptr);
            }
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

            // GPU-accelerated culling and LOD selection
            DispatchCullingComputeShader();

            if (!selectedNodeKeys_.empty()) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointPipeline_);
                if (classificationDescriptorSet_ != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pointPipelineLayout_, 0, 1, &classificationDescriptorSet_, 0, nullptr);
                }
                UpdatePushConstants(cmd);

                DrawResidentNodes(cmd);
            } else if (!visibleNodeKeys_.empty()) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointPipeline_);
                if (classificationDescriptorSet_ != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pointPipelineLayout_, 0, 1, &classificationDescriptorSet_, 0, nullptr);
                }
                UpdatePushConstants(cmd);

                DrawVisibleNodes(cmd);
            } else {
                // A loaded, non-empty cloud with zero nodes surviving culling
                // is a culling/framing bug, not a legitimate "nothing to draw"
                // state - the camera may have ended up positioned such that
                // the frustum test misses the single all-encompassing spatial
                // node. Rather than leave the viewport blank, fall back to the
                // same direct draw the debug/forceDrawAll path already uses so
                // the loaded data stays visible regardless of that bug.
                {
                    auto& diagCam = context_.GetCamera();
                    auto eye = diagCam.GetPosition();
                    auto tgt = diagCam.GetTarget();
                    fprintf(stderr, "[Renderer] WARNING: No visible or selected nodes! "
                                     "eye=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) "
                                     "nodesTested=%u nodesPassed=%u - falling back to direct draw.\n",
                                     eye.x, eye.y, eye.z, tgt.x, tgt.y, tgt.z,
                                     visDebugStats_.totalNodes, visDebugStats_.visibleNodes);
                }
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointPipeline_);
                if (classificationDescriptorSet_ != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pointPipelineLayout_, 0, 1, &classificationDescriptorSet_, 0, nullptr);
                }
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
            }
        }
    }

    DrawVectorOverlay(cmd);
    DrawCadGeometry(cmd);
    DrawSurface(cmd);

    if (sceneManager_.GetObjectCount() > 0) {
        sceneManager_.Update();
        sceneManager_.SubmitRenderCommands(context_.GetRenderQueue(), cadLinePipeline_, pointPipelineLayout_);
    }

    if (useImGui_ && imguiOverlay_) {
        imguiOverlay_->BeginFrame();
            imguiOverlay_->RenderDebugPanel(context_);
            imguiOverlay_->RenderGPUPanel(vulkan::VulkanAllocator::Get());
            imguiOverlay_->RenderGPURendererPanel(context_.GetStats());
            imguiOverlay_->RenderStreamingPanel(context_);
            imguiOverlay_->RenderLODPanel(context_);
            imguiOverlay_->RenderVisualizationManagerPanel(visualizationManager_, context_);
            imguiOverlay_->RenderVisibilityPanel(visDebugStats_, context_.GetConfig());
            imguiOverlay_->RenderSurfacePanel(surfaceRenderer_, context_);
            imguiOverlay_->RenderToolsPanel(toolManager_, context_);
            imguiOverlay_->RenderDebugOverlay(debugRenderer_, context_);

            // VisualizationManager UI changes take effect starting next frame's
            // push constants (matches this loop's existing single-frame latency).
            auto& cfg = context_.GetConfig();
            cfg.visualizationMode = visualizationManager_.GetMode();
            cfg.intensityMin = visualizationManager_.GetIntensityMin();
            cfg.intensityMax = visualizationManager_.GetIntensityMax();
            cfg.depthMin = visualizationManager_.GetDepthShadingMin();
            cfg.depthMax = visualizationManager_.GetDepthShadingMax();
            cfg.surfaceAmbient = visualizationManager_.GetSurfaceAmbient();
            cfg.surfaceDiffuse = visualizationManager_.GetSurfaceDiffuse();
            cfg.surfaceSpecular = visualizationManager_.GetSurfaceSpecular();
            cfg.surfaceShininess = visualizationManager_.GetSurfaceShininess();
            cfg.edlStrength = visualizationManager_.GetEDLStrength();

            imguiOverlay_->Render(cmd);
    }
}

void Renderer::EndFrame() {
    if (!frameStarted_) return;

    auto& frame = frameManager_->GetCurrentFrame();
    VkCommandBuffer cmd = commandBuffers_[frame.imageIndex];

    vkCmdEndRenderPass(cmd);

    // ONE-TIME DEBUG PIXEL CAPTURE: dumps the actual rendered BGRA bytes at
    // the viewport center to stderr once, ~1s after a cloud is loaded, so we
    // can see the true post-blend framebuffer content directly instead of
    // relying on a screenshot description. Remove once the black-render bug
    // is diagnosed.
    static bool debugPixelCaptured = false;
    bool captureThisFrame = false;
    vulkan::GPUBuffer debugStagingBuffer;
    VkExtent2D debugExtent{};
    // Wait 90 frames (~1.5s) after the cloud is actually set before sampling,
    // so geometry upload/visibility/LOD have had time to settle - the first
    // capture attempt fired on the very same frame SetPointCloud() was
    // called and only ever saw the raw clear color (nothing drawn yet).
    if (!debugPixelCaptured && activeCloud_ && frameNumber_ > cloudLoadedAtFrame_ + 90) {
        captureThisFrame = true;
        debugPixelCaptured = true;

        VkExtent2D ext = swapchain_->GetExtent();
        debugExtent = ext;
        VkDeviceSize bufSize = static_cast<VkDeviceSize>(ext.width) * ext.height * 4;
        debugStagingBuffer = vulkan::VulkanAllocator::Get().CreateBuffer(
            bufSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

        VkImage img = swapchain_->GetImage(frame.imageIndex);
        VkImageMemoryBarrier toSrc{};
        toSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toSrc.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSrc.image = img;
        toSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toSrc.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toSrc);

        // Copy the ENTIRE frame this time (not just the center) so we can't
        // miss the shape regardless of where on screen it actually sits.
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {ext.width, ext.height, 1};
        vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                debugStagingBuffer.buffer, 1, &region);

        VkImageMemoryBarrier backToPresent = toSrc;
        backToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        backToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        backToPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        backToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &backToPresent);
    }

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

    if (captureThisFrame) {
        vkDeviceWaitIdle(device_->GetDevice());
        if (debugStagingBuffer.mappedData) {
            auto* px = static_cast<uint8_t*>(debugStagingBuffer.mappedData);
            fprintf(stderr, "[Renderer] DEBUG PIXEL CAPTURE: full frame %ux%u, format=%d, 12x12 grid sample (R,G,B,A):\n",
                    debugExtent.width, debugExtent.height, static_cast<int>(swapchain_->GetImageFormat()));
            constexpr int kGrid = 12;
            for (int gy = 0; gy < kGrid; ++gy) {
                uint32_t y = (debugExtent.height * (gy * 2 + 1)) / (kGrid * 2);
                fprintf(stderr, "  y=%4u: ", y);
                for (int gx = 0; gx < kGrid; ++gx) {
                    uint32_t x = (debugExtent.width * (gx * 2 + 1)) / (kGrid * 2);
                    uint8_t* p = px + (static_cast<size_t>(y) * debugExtent.width + x) * 4;
                    // Swapchain format is B8G8R8A8 - print as R,G,B,A for readability.
                    fprintf(stderr, "(%3u,%3u,%3u,%3u) ", p[2], p[1], p[0], p[3]);
                }
                fprintf(stderr, "\n");
            }
        } else {
            fprintf(stderr, "[Renderer] DEBUG PIXEL CAPTURE: staging buffer not mapped!\n");
        }
        vulkan::VulkanAllocator::Get().DestroyBuffer(debugStagingBuffer);
    }

    VkResult result = swapchain_->Present(frame.renderFinished, frame.imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        OnResize(context_.GetViewportWidth(), context_.GetViewportHeight());
    }

    frameManager_->EndFrame();
    frameStarted_ = false;

    benchmark_.EndFrame(context_.GetStats().visiblePoints);
    if (!benchmark_.IsRunning() && benchmark_.GetResult().totalFrames > 0) {
        benchmark_.PrintSummary();
    }

    uint64_t now = SDL_GetPerformanceCounter();
    double delta = static_cast<double>(now - lastFrameTime_) / SDL_GetPerformanceFrequency();
    context_.UpdateFrameStats(delta * 1000.0);
    lastFrameTime_ = now;
    frameNumber_++;
}

void Renderer::SetPointCloud(pointcloud::PointCloud* cloud) {
    activeCloud_ = cloud;
    cloudLoadedAtFrame_ = frameNumber_;
    if (auto* classificationTool = toolManager_.GetClassificationTool()) {
        classificationTool->SetTargetCloud(cloud);
    }
    if (cloud) {
        auto* prepGeo = adapter_.PreparePointCloud(*cloud);
        auto* root = cloud->Root();
        if (root) {
            fprintf(stderr, "[Renderer] SetPointCloud: %llu points, cloud='%s'\n",
                    cloud->PointCount(), cloud->Name());

            // Coordinate normalization now happens at load time: LoadLasFile
            // (see LasFileReader.cpp) establishes coordNormalizer_'s shared
            // origin on the first load and reuses it on every load after -
            // including one already set by an SNT attachment - so recomputing
            // it here from this cloud's own (already-recentered) local bounds
            // would just clobber whatever origin the data was placed against.

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

void Renderer::RefreshNormals() {
    if (!activeCloud_) return;
    auto* root = activeCloud_->Root();
    if (!root) return;

    auto* ch = root->channels().GetChannel(pointcloud::ChannelId::Normals);
    if (!ch || !ch->Data()) return;

    auto* geo = adapter_.GetPreparedGeometry(0);
    if (!geo) return;

    geo->UploadNormal(reinterpret_cast<const float*>(ch->Data()),
                       static_cast<uint32_t>(ch->Count()));
}

void Renderer::RefreshClassification() {
    if (!activeCloud_) return;
    auto* root = activeCloud_->Root();
    if (!root) return;

    auto* ch = root->channels().GetChannel(pointcloud::ChannelId::Classification);
    if (!ch || !ch->Data()) return;

    size_t count = ch->Count();
    // GPU classification attribute is float (see point.vert's inClassification
    // and PointCloudRenderAdapter's initial upload) - the CPU channel itself
    // stays 1 byte/point, so convert on the way up same as the initial build.
    std::vector<float> classifications(count);
    const uint8_t* data = ch->Data();
    for (size_t i = 0; i < count; ++i) {
        classifications[i] = static_cast<float>(data[i]);
    }

    auto* geo = adapter_.GetPreparedGeometry(0);
    if (!geo) return;

    geo->UploadClassification(classifications.data(), static_cast<uint32_t>(count));
    geo->IncrementRevision();
}

void Renderer::SetVectorOverlay(const pointcloud::SntEntities& entities) {
    if (overlayVertexBuffer_.IsValid()) {
        vulkan::VulkanAllocator::Get().DestroyBuffer(overlayVertexBuffer_);
    }
    overlayVertexCount_ = 0;
    if (entities.polylines.empty()) return;

    // Recenter around this geometry's own bbox midpoint (matching
    // LasFileReader's recentering approach) so line vertices stay within
    // float32 precision range. There's no shared origin with a separately
    // -loaded point cloud's own per-file recentering -- exact co-
    // registration with a specific LAZ tile isn't guaranteed.
    double originX = (entities.bounds.minX + entities.bounds.maxX) * 0.5;
    double originY = (entities.bounds.minY + entities.bounds.maxY) * 0.5;
    double originZ = (entities.bounds.minZ + entities.bounds.maxZ) * 0.5;

    std::vector<float> lineVerts;
    for (const auto& poly : entities.polylines) {
        size_t n = poly.VertexCount();
        for (size_t i = 0; i + 1 < n; ++i) {
            for (int e = 0; e < 2; ++e) {
                size_t vi = i + e;
                lineVerts.push_back(static_cast<float>(poly.points[vi * 3 + 0] - originX));
                lineVerts.push_back(static_cast<float>(poly.points[vi * 3 + 1] - originY));
                lineVerts.push_back(static_cast<float>(poly.points[vi * 3 + 2] - originZ));
            }
        }
    }
    if (lineVerts.empty()) return;

    VkDeviceSize size = static_cast<VkDeviceSize>(lineVerts.size()) * sizeof(float);
    overlayVertexBuffer_ = vulkan::VulkanAllocator::Get().CreateBuffer(
        size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    if (!overlayVertexBuffer_.IsValid()) {
        fprintf(stderr, "[Renderer] SetVectorOverlay: buffer allocation failed\n");
        return;
    }
    if (overlayVertexBuffer_.mappedData) {
        memcpy(overlayVertexBuffer_.mappedData, lineVerts.data(), static_cast<size_t>(size));
        overlayVertexBuffer_.FlushMapped();
    }
    overlayVertexCount_ = static_cast<uint32_t>(lineVerts.size() / 3);

    if (!activeCloud_) {
        // Only take over camera framing when nothing else is already
        // loaded/focused.
        spatial::BoundingBox recentered;
        recentered.minX = entities.bounds.minX - originX;
        recentered.maxX = entities.bounds.maxX - originX;
        recentered.minY = entities.bounds.minY - originY;
        recentered.maxY = entities.bounds.maxY - originY;
        recentered.minZ = entities.bounds.minZ - originZ;
        recentered.maxZ = entities.bounds.maxZ - originZ;
        context_.GetCamera().FocusOnBounds(recentered);
    }

    fprintf(stderr, "[Renderer] SetVectorOverlay: %zu polylines, %u line vertices uploaded\n",
            entities.polylines.size(), overlayVertexCount_);
}

void Renderer::DrawVectorOverlay(VkCommandBuffer cmd) {
    if (overlayVertexCount_ == 0 || !overlayVertexBuffer_.IsValid()) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline_);
    UpdatePushConstants(cmd);

    VkBuffer bufs[] = {overlayVertexBuffer_.buffer};
    VkDeviceSize offs[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, bufs, offs);
    vkCmdDraw(cmd, overlayVertexCount_, 1, 0, 0);
}

void Renderer::DrawCadGeometry(VkCommandBuffer cmd) {
    if (!cadRenderer_.HasGeometry()) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cadLinePipeline_);
    UpdatePushConstants(cmd);

    if (cadRenderer_.GetLineVertexCount() > 0) {
        VkBuffer bufs[] = {cadRenderer_.GetLineVertexBuffer().buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, bufs, offs);
        vkCmdDraw(cmd, cadRenderer_.GetLineVertexCount(), 1, 0, 0);
    }

    if (cadRenderer_.GetPointVertexCount() > 0) {
        VkBuffer bufs[] = {cadRenderer_.GetPointVertexBuffer().buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, bufs, offs);
        vkCmdDraw(cmd, cadRenderer_.GetPointVertexCount(), 1, 0, 0);
    }
}

void Renderer::DrawSurface(VkCommandBuffer cmd) {
    if (!surfaceRenderer_.IsVisible() || surfaceRenderer_.GetMeshCount() == 0) return;

    auto& cam = context_.GetCamera();

    // Preserve the user's display mode / shading selection that was stored on
    // the renderer (see ViewportWindow::SetSurfaceMode / SetSurfaceShading),
    // only re-syncing the per-frame lighting values from the config.
    auto params = surfaceRenderer_.GetParams();

    auto& cfg = context_.GetConfig();
    params.ambient = cfg.surfaceAmbient;
    params.diffuse = cfg.surfaceDiffuse;
    params.specular = cfg.surfaceSpecular;
    params.shininess = cfg.surfaceShininess;
    params.depthMin = cfg.depthMin;
    params.depthMax = cfg.depthMax;
    params.edlStrength = cfg.edlStrength;
    params.lightDirX = cfg.surfaceLightDirX;
    params.lightDirY = cfg.surfaceLightDirY;
    params.lightDirZ = cfg.surfaceLightDirZ;
    params.elevationMin = cfg.elevationMin;
    params.elevationMax = cfg.elevationMax;

    surfaceRenderer_.Render(cmd, cam, params);
}

void Renderer::LoadClassificationPTC(const std::string& filepath, std::string* error) {
    display::ClassPalette palette;
    if (!display::PtcFileReader::load(filepath, palette, error)) {
        fprintf(stderr, "[Renderer] LoadClassificationPTC: FAILED - %s\n",
                error ? error->c_str() : "unknown");
        return;
    }
    SetCustomClassificationPalette(palette);
    fprintf(stderr, "[Renderer] LoadClassificationPTC: loaded %zu classes from %s\n",
            palette.size(), filepath.c_str());
}

void Renderer::SetCustomClassificationPalette(const display::ClassPalette& palette) {
    customPalette_ = palette;
    hasCustomPalette_ = true;

    fprintf(stderr, "[Renderer] SetCustomClassificationPalette: buffer valid=%d, mapped=%p\n",
            classificationBuffer_.IsValid(), classificationBuffer_.mappedData);

    if (!classificationBuffer_.IsValid() || !classificationBuffer_.mappedData) return;

    constexpr uint32_t kMaxClasses = 256;
    constexpr VkDeviceSize bufSize = kMaxClasses * sizeof(float) * 4;
    float colors[kMaxClasses * 4];
    memset(colors, 0, sizeof(colors));

    for (uint32_t i = 0; i < kMaxClasses; ++i) {
        colors[i * 4 + 3] = 1.0f;
    }

    for (const auto& [code, entry] : palette) {
        if (code < 0 || code >= static_cast<int>(kMaxClasses)) continue;
        colors[code * 4 + 0] = entry.normalizedR();
        colors[code * 4 + 1] = entry.normalizedG();
        colors[code * 4 + 2] = entry.normalizedB();
        colors[code * 4 + 3] = entry.visible ? 1.0f : 0.0f;
        fprintf(stderr, "  class %d: %s -> rgb=(%.3f,%.3f,%.3f) visible=%d\n",
                code, entry.description.c_str(),
                entry.normalizedR(), entry.normalizedG(), entry.normalizedB(), entry.visible);
    }

    memcpy(classificationBuffer_.mappedData, colors, bufSize);
    classificationBuffer_.FlushMapped();
    // Verify readback of first few entries
    {
        float* rb = static_cast<float*>(classificationBuffer_.mappedData);
        fprintf(stderr, "[Renderer] PTC SSBO verify: c0=(%.2f,%.2f,%.2f,%.2f) c1=(%.2f,%.2f,%.2f,%.2f) c2=(%.2f,%.2f,%.2f,%.2f) c14=(%.2f,%.2f,%.2f,%.2f) hasCustom=%d set=%p\n",
            rb[0],rb[1],rb[2],rb[3], rb[4],rb[5],rb[6],rb[7], rb[8],rb[9],rb[10],rb[11], rb[56],rb[57],rb[58],rb[59], hasCustomPalette_, (void*)classificationDescriptorSet_);
        SLOG_INFO("PTC SSBO verify: c0=(%.2f,%.2f,%.2f,%.2f) c1=(%.2f,%.2f,%.2f,%.2f) c2=(%.2f,%.2f,%.2f,%.2f) c14=(%.2f,%.2f,%.2f,%.2f) hasCustom=%d set=%p",
            rb[0],rb[1],rb[2],rb[3], rb[4],rb[5],rb[6],rb[7], rb[8],rb[9],rb[10],rb[11], rb[56],rb[57],rb[58],rb[59], hasCustomPalette_, (void*)classificationDescriptorSet_);
        fflush(stderr);
    }
    fprintf(stderr, "[Renderer] Custom palette: %zu classes, descriptor set valid=%d\n",
            palette.size(), classificationDescriptorSet_ != VK_NULL_HANDLE);
    SLOG_INFO("Custom palette: %zu classes, descriptor set valid=%d", palette.size(), classificationDescriptorSet_ != VK_NULL_HANDLE);
}

void Renderer::ClearCustomClassificationPalette() {
    hasCustomPalette_ = false;
    customPalette_.clear();

    // Reset to default ASPRS palette.
    if (!classificationBuffer_.IsValid() || !classificationBuffer_.mappedData) return;

    constexpr uint32_t kMaxClasses = 256;
    constexpr VkDeviceSize bufSize = kMaxClasses * sizeof(float) * 4;
    float defaults[kMaxClasses * 4];
    memset(defaults, 0, sizeof(defaults));
    for (uint32_t i = 0; i < kMaxClasses; ++i) {
        defaults[i * 4 + 3] = 1.0f;
    }
    auto setCls = [&](int cls, float r, float g, float b) {
        defaults[cls * 4 + 0] = r;
        defaults[cls * 4 + 1] = g;
        defaults[cls * 4 + 2] = b;
    };
    setCls(0,  0.50f, 0.50f, 0.50f); setCls(1,  0.00f, 1.00f, 0.00f);
    setCls(2,  0.00f, 0.78f, 0.00f); setCls(3,  0.00f, 0.60f, 0.00f);
    setCls(4,  0.00f, 0.42f, 0.00f); setCls(5,  0.00f, 0.25f, 0.00f);
    setCls(6,  1.00f, 0.00f, 0.00f); setCls(7,  1.00f, 0.50f, 0.00f);
    setCls(8,  1.00f, 1.00f, 0.00f); setCls(9,  0.50f, 0.00f, 0.50f);
    setCls(10, 0.75f, 0.75f, 0.75f); setCls(11, 0.80f, 0.80f, 0.00f);
    setCls(12, 0.60f, 0.60f, 0.00f); setCls(13, 0.40f, 0.40f, 0.00f);
    setCls(14, 0.20f, 0.20f, 0.00f); setCls(15, 0.60f, 0.30f, 0.00f);
    setCls(16, 0.00f, 0.00f, 1.00f); setCls(17, 0.50f, 0.50f, 1.00f);

    memcpy(classificationBuffer_.mappedData, defaults, bufSize);
    classificationBuffer_.FlushMapped();
}

void Renderer::UpdateClassificationVisibility(int classCode, bool visible) {
    if (classCode < 0 || classCode >= 256) return;

    auto it = customPalette_.find(classCode);
    if (it != customPalette_.end()) {
        it->second.visible = visible;
    }

    if (!classificationBuffer_.IsValid() || !classificationBuffer_.mappedData) return;

    // Update just the alpha of the affected class.
    float* colors = static_cast<float*>(classificationBuffer_.mappedData);
    colors[classCode * 4 + 3] = visible ? 1.0f : 0.0f;
    classificationBuffer_.FlushMapped();
}

void Renderer::LoadDxfAttachment(cad::DxfAttachment* attachment) {
    cadRenderer_.LoadDxfAttachment(attachment);
}

void Renderer::LoadDwgAttachment(cad::DwgAttachment* attachment) {
    cadRenderer_.LoadDwgAttachment(attachment);
}

void Renderer::LoadSntAttachment(cad::SntAttachment* attachment) {
    cadRenderer_.LoadSntAttachment(attachment);
}

void Renderer::RemoveDxfAttachment(cad::DxfAttachment* attachment) {
    cadRenderer_.RemoveAttachment(attachment);
}

void Renderer::RemoveDwgAttachment(cad::DwgAttachment* attachment) {
    cadRenderer_.RemoveAttachment(attachment);
}

void Renderer::RemoveSntAttachment(cad::SntAttachment* attachment) {
    cadRenderer_.RemoveAttachment(attachment);
}

void Renderer::RemoveAllCadAttachments() {
    cadRenderer_.RemoveAllAttachments();
}

void Renderer::SetCadLayerVisibility(const std::string& layerName, bool visible) {
    cadRenderer_.SetLayerVisibility(layerName, visible);
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

void Renderer::DispatchCullingComputeShader() {
    // GPU-accelerated frustum culling using compute shader
    auto& visCache = context_.GetVisibilityCache();
    auto& cam = context_.GetCamera();
    uint32_t nodeCount = static_cast<uint32_t>(visibleNodeKeys_.size());

    if (nodeCount == 0) return;

    // Allocate visibility buffer from GPU buffer manager
    VkDeviceSize bufferSize = nodeCount * sizeof(gpu::VisibilityInfo);
    workstation::gpu::GPUBufferAllocation* visBufferAlloc = bufferManager_->Allocate(
        gpu::BufferType::Visibility, bufferSize, true);
    if (!visBufferAlloc || !visBufferAlloc->IsValid()) {
        fprintf(stderr, "[Renderer] Failed to allocate visibility buffer\n");
        return;
    }

    // Map and fill the visibility buffer
    gpu::VisibilityInfo* visData = nullptr;
    VkResult res = vmaMapMemory(
        vulkan::VulkanAllocator::Get().GetAllocator(),
        visBufferAlloc->allocation,
        reinterpret_cast<void**>(&visData));
    if (res != VK_SUCCESS) {
        fprintf(stderr, "[Renderer] Failed to map visibility buffer\n");
        return;
    }

    // Fill visibility info for each node
    uint32_t visibleCount = 0;
    for (uint32_t i = 0; i < nodeCount; i++) {
        uint64_t nodeKey = visibleNodeKeys_[i];

        // Look up cached visibility result
        auto* cached = visCache.Get(nodeKey);
        if (cached && cached->lastTestedFrame == frameNumber_) {
            // Use cached result
            visData[i].nodeKey = cached->nodeKey;
            visData[i].visible = cached->isVisible ? 1 : 0;
            visData[i].lodLevel = 0;  // LOD level not cached, default to 0
            visData[i].drawCount = 0; // drawCount not cached, default to 0
            if (cached->isVisible) {
                visibleCount++;
            }
            continue;
        }

        // Test node against frustum
        bool visible = false;
        // TODO: Test node bounds against frustum planes

        visData[i].nodeKey = nodeKey;
        visData[i].visible = visible ? 1 : 0;
        visData[i].lodLevel = 0;  // Will be set by LOD selection later
        visData[i].drawCount = 0;  // Will be set after LOD selection

        if (visible) visibleCount++;
    }

    vkUnmapMemory(
        vulkan::VulkanAllocator::Get().GetDevice(),
        visBufferAlloc->allocation);

    // Bind the compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline_);

    // Dispatch compute shader - 256 work items per wave
    uint32_t groupCount = (nodeCount + 255) / 256;
    vkCmdDispatch(cmd, groupCount, 1, 1);

    // Insert a memory barrier to ensure compute results are available
    // for the graphics queue to read the indirect command buffer
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    // Record indirect draw commands - compute shader writes directly to
    // the indirect command buffer, no CPU readback needed
    RecordIndirectDrawCommands(nodeCount);

    // Optional debug readback: map visibility buffer and count visible nodes
    if (context_.GetConfig().enableDebugReadback) {
        gpu::VisibilityInfo* visDataMapped = nullptr;
        VkResult res = vmaMapMemory(
            vulkan::VulkanAllocator::Get().GetAllocator(),
            visBufferAlloc->allocation,
            reinterpret_cast<void**>(&visDataMapped));
        if (res == VK_SUCCESS) {
            uint32_t debugVisibleCount = 0;
            for (uint32_t i = 0; i < nodeCount; i++) {
                if (visDataMapped[i].visible) {
                    debugVisibleCount++;
                }
            }
            context_.GetStats().visibleNodes = debugVisibleCount;
            vkUnmapMemory(
                vulkan::VulkanAllocator::Get().GetDevice(),
                visBufferAlloc->allocation);
            fprintf(stderr, "[Renderer] Debug readback: %u visible nodes out of %u total\n",
                    debugVisibleCount, nodeCount);
        }
    }
}

void Renderer::RecordIndirectDrawCommands(uint32_t nodeCount,
    // Allocate indirect command buffer
    VkDeviceSize indirectBufSize = nodeCount * sizeof(gpu::IndirectDrawCommand);
    workstation::gpu::GPUBufferAllocation* indirectAlloc = bufferManager_->Allocate(
        gpu::BufferType::IndirectDraw, indirectBufSize, true);
    if (!indirectAlloc || !indirectAlloc->IsValid()) {
        fprintf(stderr, "[Renderer] Failed to allocate indirect command buffer\n");
        return;
    }

    // Map and fill indirect commands
    gpu::IndirectDrawCommand* indirectData = nullptr;
    VkResult res = vmaMapMemory(
        vulkan::VulkanAllocator::Get().GetAllocator(),
        indirectAlloc->allocation,
        reinterpret_cast<void**>(&indirectData));
    if (res != VK_SUCCESS) {
        fprintf(stderr, "[Renderer] Failed to map indirect command buffer\n");
        return;
    }

    // Initialize indirect draw commands with defaults
    // (Compute shader will overwrite these with actual values)
    for (uint32_t i = 0; i < nodeCount; i++) {
        // Default: draw 1 instance per visible node
        // Compute shader should update with actual point counts
        indirectData[i].vertexCount = 0;  // Not used for point lists
        indirectData[i].instanceCount = 1;
        indirectData[i].firstVertex = 0;
        indirectData[i].firstInstance = 0;
    }

    vmaUnmapMemory(
        vulkan::VulkanAllocator::Get().GetAllocator(),
        indirectAlloc->allocation);

    // Record the indirect command buffer for submission via vkCmdDrawIndirect
    // No CPU readback of visibility results - compute shader writes directly
    context_.GetStats().indirectDrawCount = nodeCount;
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

void Renderer::StartBenchmark(uint32_t frames) {
    benchmark_.BeginRun(frames);
    fprintf(stderr, "[Renderer] Benchmark started: %u frames\n", frames);
    fflush(stderr);
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

    // Descriptor layout: binding 0 = classification color storage buffer (256 RGBA vec4s).
    // The shader reads from it only when visualizationMode == 15 (ClassificationPalette)
    // or when hasCustomPalette is set via push constants.
    VkDescriptorSetLayout layout = descriptorManager_->CreateLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
    });

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

    // Classification color storage buffer: 256 entries × 16 bytes (vec4 RGBA).
    // Initialized to the default ASPRS palette; overridden by LoadClassificationPTC.
    // Must use VMA_MEMORY_USAGE_AUTO + HOST_VISIBLE|HOST_COHERENT like
    // PreparedGeometry so the SSBO is readable by the GPU as a storage buffer.
    {
        constexpr uint32_t kMaxClasses = 256;
        constexpr VkDeviceSize bufSize = kMaxClasses * sizeof(float) * 4;
        // Match PreparedGeometry's VMA pattern for host-visible + GPU-readable.
        VmaAllocator vma = vulkan::VulkanAllocator::Get().GetAllocator();
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufSize;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        vmaCreateBuffer(vma, &bufInfo, &allocInfo,
                        &classificationBuffer_.buffer, &classificationBuffer_.allocation, &classificationBuffer_.allocationInfo);
        classificationBuffer_.size = bufSize;
        classificationBuffer_.mappedData = classificationBuffer_.allocationInfo.pMappedData;
        fprintf(stderr, "[Renderer] Classification SSBO: buffer=%p mapped=%p size=%llu valid=%d\n",
                classificationBuffer_.buffer, classificationBuffer_.mappedData,
                (unsigned long long)bufSize, classificationBuffer_.IsValid());
        fflush(stderr);

        if (classificationBuffer_.IsValid() && classificationBuffer_.mappedData) {
            // Fill with default ASPRS palette (from InitializeClassificationPalette).
            float defaults[kMaxClasses * 4] = {};
            auto setCls = [&](int cls, float r, float g, float b) {
                defaults[cls * 4 + 0] = r;
                defaults[cls * 4 + 1] = g;
                defaults[cls * 4 + 2] = b;
                defaults[cls * 4 + 3] = 1.0f;
            };
            setCls(0,  0.50f, 0.50f, 0.50f); setCls(1,  0.00f, 1.00f, 0.00f);
            setCls(2,  0.00f, 0.78f, 0.00f); setCls(3,  0.00f, 0.60f, 0.00f);
            setCls(4,  0.00f, 0.42f, 0.00f); setCls(5,  0.00f, 0.25f, 0.00f);
            setCls(6,  1.00f, 0.00f, 0.00f); setCls(7,  1.00f, 0.50f, 0.00f);
            setCls(8,  1.00f, 1.00f, 0.00f); setCls(9,  0.50f, 0.00f, 0.50f);
            setCls(10, 0.75f, 0.75f, 0.75f); setCls(11, 0.80f, 0.80f, 0.00f);
            setCls(12, 0.60f, 0.60f, 0.00f); setCls(13, 0.40f, 0.40f, 0.00f);
            setCls(14, 0.20f, 0.20f, 0.00f); setCls(15, 0.60f, 0.30f, 0.00f);
            setCls(16, 0.00f, 0.00f, 1.00f); setCls(17, 0.50f, 0.50f, 1.00f);
            memcpy(classificationBuffer_.mappedData, defaults, bufSize);
            classificationBuffer_.FlushMapped();
        }

        classificationDescriptorSet_ = descriptorManager_->AllocateSet(
            pointDescriptorPool_, pointDescriptorLayout_);
        descriptorManager_->UpdateBuffer(
            classificationDescriptorSet_, 0,
            classificationBuffer_.buffer, classificationBuffer_.size,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
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

bool Renderer::CreateLinePipeline() {
    // Renders vector/CAD overlay geometry (e.g. decoded .snt shapes) as flat
    // amber line segments on top of the point cloud.
#ifdef _WIN32
    std::string shaderDir = GetExeShaderDir();
#else
    const char* basePath = SDL_GetBasePath();
    std::string shaderDir = basePath ? std::string(basePath) + "shaders/" : "shaders/";
#endif

    auto shaders = shaderManager_->LoadSPIRVFiles({
        {shaderDir + "line.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {shaderDir + "line.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    for (auto& shader : shaders) {
        if (shader.module == VK_NULL_HANDLE) {
            fprintf(stderr, "Renderer::CreateLinePipeline: failed to load shaders\n");
            return false;
        }
    }

    vulkan::PipelineConfig pconfig;
    pconfig.SetDefaults();
    pconfig.colorFormat = swapchain_->GetImageFormat();

    // Single position-only vertex stream, replacing the 5-binding point
    // layout SetDefaults() configured.
    pconfig.vertexBindings = {
        {0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX},
    };
    pconfig.vertexAttributes = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
    };
    pconfig.vertexInput.vertexBindingDescriptionCount =
        static_cast<uint32_t>(pconfig.vertexBindings.size());
    pconfig.vertexInput.pVertexBindingDescriptions = pconfig.vertexBindings.data();
    pconfig.vertexInput.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(pconfig.vertexAttributes.size());
    pconfig.vertexInput.pVertexAttributeDescriptions = pconfig.vertexAttributes.data();

    pconfig.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    pconfig.depthStencil.depthTestEnable = VK_FALSE;
    pconfig.depthStencil.depthWriteEnable = VK_FALSE;
    pconfig.rasterizer.cullMode = VK_CULL_MODE_NONE;
    pconfig.rasterizer.lineWidth = 1.0f;

    // Reuses pointPipelineLayout_ -- line.vert declares the identical
    // push-constant block (only viewProjection is read).
    linePipeline_ = pipelineManager_->CreateGraphicsPipeline(
        pointPipelineLayout_, shaders, pconfig, renderPass_->GetRenderPass());

    for (auto& shader : shaders) {
        shaderManager_->DestroyShaderModule(shader.module);
    }

    fprintf(stderr, "[Renderer] Line pipeline created: %s\n",
            linePipeline_ != VK_NULL_HANDLE ? "OK" : "FAILED");
    fflush(stderr);
    return linePipeline_ != VK_NULL_HANDLE;
}

bool Renderer::CreateCadLinePipeline() {
#ifdef _WIN32
    std::string shaderDir = GetExeShaderDir();
#else
    const char* basePath = SDL_GetBasePath();
    std::string shaderDir = basePath ? std::string(basePath) + "shaders/" : "shaders/";
#endif

    auto shaders = shaderManager_->LoadSPIRVFiles({
        {shaderDir + "cad_line.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {shaderDir + "cad_line.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    for (auto& shader : shaders) {
        if (shader.module == VK_NULL_HANDLE) {
            fprintf(stderr, "Renderer::CreateCadLinePipeline: failed to load shaders\n");
            return false;
        }
    }

    vulkan::PipelineConfig pconfig;
    pconfig.SetDefaults();
    pconfig.colorFormat = swapchain_->GetImageFormat();

    pconfig.vertexBindings = {
        {0, sizeof(float) * 6, VK_VERTEX_INPUT_RATE_VERTEX},
    };
    pconfig.vertexAttributes = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3},
    };
    pconfig.vertexInput.vertexBindingDescriptionCount =
        static_cast<uint32_t>(pconfig.vertexBindings.size());
    pconfig.vertexInput.pVertexBindingDescriptions = pconfig.vertexBindings.data();
    pconfig.vertexInput.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(pconfig.vertexAttributes.size());
    pconfig.vertexInput.pVertexAttributeDescriptions = pconfig.vertexAttributes.data();

    pconfig.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    pconfig.depthStencil.depthTestEnable = VK_TRUE;
    pconfig.depthStencil.depthWriteEnable = VK_FALSE;
    pconfig.rasterizer.cullMode = VK_CULL_MODE_NONE;
    pconfig.rasterizer.lineWidth = 1.0f;

    cadLinePipeline_ = pipelineManager_->CreateGraphicsPipeline(
        pointPipelineLayout_, shaders, pconfig, renderPass_->GetRenderPass());

    for (auto& shader : shaders) {
        shaderManager_->DestroyShaderModule(shader.module);
    }

    fprintf(stderr, "[Renderer] CAD line pipeline created: %s\n",
            cadLinePipeline_ != VK_NULL_HANDLE ? "OK" : "FAILED");
    fflush(stderr);
    return cadLinePipeline_ != VK_NULL_HANDLE;
}

bool Renderer::CreateDescriptorResources() {
    // Create a descriptor pool for the point pipeline's classification SSBO.
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;
    pointDescriptorPool_ = descriptorManager_->CreatePool({poolSize}, 1);
    if (pointDescriptorPool_ == VK_NULL_HANDLE) {
        fprintf(stderr, "[Renderer] Failed to create point descriptor pool\n");
        return false;
    }
    fprintf(stderr, "[Renderer] Point descriptor pool created OK\n");
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
    // Predominantly overhead (Z is up in this Z-up/LiDAR data convention),
    // with a slight lateral tilt so vertical surfaces (walls, edges) still
    // pick up visible contrast in NormalShading mode.
    pc.lightDirection[0] = 0.35f;
    pc.lightDirection[1] = 0.35f;
    pc.lightDirection[2] = 0.87f;

    pc.pointScale = static_cast<float>(context_.GetViewportHeight()) * 0.5f;
    pc.pointSize = cfg.pointSize;
    pc.visualizationMode = static_cast<uint32_t>(cfg.visualizationMode);
    pc.intensityMin = cfg.intensityMin;
    pc.intensityMax = cfg.intensityMax;
    pc.elevationMin = cfg.elevationMin;
    pc.elevationMax = cfg.elevationMax;
    // New depth/surface/EDL parameters
    pc.depthMin = cfg.depthMin;
    pc.depthMax = cfg.depthMax;
    pc.surfaceAmbient = cfg.surfaceAmbient;
    pc.surfaceDiffuse = cfg.surfaceDiffuse;
    pc.surfaceSpecular = cfg.surfaceSpecular;
    pc.surfaceShininess = cfg.surfaceShininess;
    pc.edlStrength = cfg.edlStrength;
    pc.hasCustomPalette = hasCustomPalette_ ? 1u : 0u;

    static int logCounter = 0;
    if (logCounter++ < 5 || (logCounter % 120 == 0)) {
        fprintf(stderr, "[Renderer] PushConstants: vizMode=%u hasCustomPalette=%u\n",
                pc.visualizationMode, pc.hasCustomPalette);
    }

    vkCmdPushConstants(cmd, pointPipelineLayout_,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(pc), &pc);
}

} // namespace renderer
} // namespace workstation
