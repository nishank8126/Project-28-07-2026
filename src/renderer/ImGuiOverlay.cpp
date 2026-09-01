#include "workstation/renderer/ImGuiOverlay.h"
#include "workstation/renderer/Renderer.h"

namespace workstation {
namespace renderer {

ImGuiOverlay::~ImGuiOverlay() { Shutdown(); }

bool ImGuiOverlay::Initialize(SDL_Window* window, VkInstance instance,
                               vulkan::VulkanDevice& device,
                               VkRenderPass renderPass, uint32_t imageCount) {
    window_ = window;
    device_ = device.GetDevice();

    VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    vkCreateDescriptorPool(device_, &poolInfo, nullptr, &imguiPool_);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    SetupStyle();

    ImGui_ImplSDL3_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = device.GetPhysicalDevice();
    initInfo.Device = device_;
    initInfo.QueueFamily = device.GetQueueFamilies().graphicsFamily;
    initInfo.Queue = device.GetGraphicsQueue();
    initInfo.DescriptorPool = imguiPool_;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.RenderPass = renderPass;
    ImGui_ImplVulkan_Init(&initInfo);

    initialized_ = true;
    return true;
}

void ImGuiOverlay::Shutdown() {
    if (!initialized_) return;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (imguiPool_) {
        vkDestroyDescriptorPool(device_, imguiPool_, nullptr);
        imguiPool_ = VK_NULL_HANDLE;
    }
    initialized_ = false;
}

void ImGuiOverlay::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::Render(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiOverlay::EndFrame() {
    ImGui::EndFrame();
}

void ImGuiOverlay::RenderDebugPanel(RenderContext& context) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Renderer Debug");

    auto& stats = context.GetStats();
    ImGui::Text("FPS: %.1f (%.2f ms)", stats.fps, stats.frameTimeMs);
    ImGui::Separator();
    ImGui::Text("Visible Points: %llu", stats.visiblePoints);
    ImGui::Text("Visible Nodes: %u", stats.visibleNodes);
    ImGui::Text("Draw Calls: %u", stats.drawCalls);
    ImGui::Text("GPU Buffers: %u", stats.gpuBuffers);
    ImGui::Separator();

    auto& cam = context.GetCamera();
    auto pos = cam.GetPosition();
    ImGui::Text("Camera: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
    ImGui::Text("FOV: %.1f", cam.GetFOV());

    ImGui::End();
}

void ImGuiOverlay::RenderLODPanel(RenderContext& context) {
    ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 280), ImGuiCond_FirstUseEver);
    ImGui::Begin("LOD");

    auto& lodStats = context.GetLODDebugStats();
    auto& config = context.GetConfig();

    ImGui::Checkbox("LOD Enabled", &config.lodEnabled);
    ImGui::Separator();

    ImGui::Text("Visible Nodes:   %u", lodStats.visibleNodes);
    ImGui::Text("Selected Nodes:  %u", lodStats.selectedNodes);
    ImGui::Text("Rejected Nodes:  %u", lodStats.rejectedNodes);
    ImGui::Separator();
    ImGui::Text("Visible Points:  %llu", lodStats.visiblePoints);
    ImGui::Text("Rendered Points: %llu", lodStats.renderedPoints);
    ImGui::Text("Point Budget:    %llu", lodStats.pointBudget);
    ImGui::Separator();
    ImGui::Text("Average SSE:     %.2f", lodStats.averageSSE);
    ImGui::Text("Current LOD:     %u", lodStats.currentLOD);
    ImGui::Text("LOD Time:        %.3f ms", lodStats.lodTimeMs);
    ImGui::Separator();

    float budgetMillions = config.pointBudgetMillions;
    if (ImGui::SliderFloat("Budget (M)", &budgetMillions, 1.0f, 100.0f, "%.1f M")) {
        config.pointBudgetMillions = budgetMillions;
    }

    int forceLOD = config.forceLODLevel;
    if (ImGui::SliderInt("Force LOD", &forceLOD, -1, 15)) {
        config.forceLODLevel = forceLOD;
    }
    if (forceLOD >= 0) {
        ImGui::SameLine();
        ImGui::Text("(Active: %d)", forceLOD);
    } else {
        ImGui::SameLine();
        ImGui::Text("(Auto)");
    }

    ImGui::SliderFloat("Point Size", &config.pointSize, 0.5f, 10.0f);

    ImGui::End();
}

void ImGuiOverlay::RenderGPUPanel(vulkan::VulkanAllocator& allocator) {
    ImGui::SetNextWindowPos(ImVec2(10, 220), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("GPU");

    auto stats = allocator.GetStats();
    ImGui::Text("Allocated: %.2f MB", stats.totalAllocated / (1024.0 * 1024.0));
    ImGui::Text("Used: %.2f MB", stats.totalUsed / (1024.0 * 1024.0));
    ImGui::Text("Buffers: %u", stats.bufferCount);
    ImGui::Text("Images: %u", stats.imageCount);

    ImGui::End();
}

void ImGuiOverlay::RenderStreamingPanel(RenderContext& context) {
    ImGui::SetNextWindowPos(ImVec2(10, 350), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 280), ImGuiCond_FirstUseEver);
    ImGui::Begin("Streaming");

    auto& stats = context.GetStreamingDebugStats();

    ImGui::Text("Requests");
    ImGui::Text("  Requested:   %u", stats.requestedNodes);
    ImGui::Text("  Loading:     %u", stats.loadingNodes);
    ImGui::Text("  CPU Ready:   %u", stats.cpuReadyNodes);
    ImGui::Text("  GPU Upload:  %u", stats.gpuUploadingNodes);
    ImGui::Text("  GPU Resident:%u", stats.gpuResidentNodes);
    ImGui::Separator();

    ImGui::Text("Memory");
    double cpuUsedGB = stats.cpuCacheUsed / (1024.0 * 1024.0 * 1024.0);
    double cpuLimitGB = stats.cpuCacheLimit / (1024.0 * 1024.0 * 1024.0);
    double gpuUsedGB = stats.gpuMemoryUsed / (1024.0 * 1024.0 * 1024.0);
    double gpuLimitGB = stats.gpuMemoryLimit / (1024.0 * 1024.0 * 1024.0);
    ImGui::Text("  CPU Cache:   %.2f / %.2f GB", cpuUsedGB, cpuLimitGB);
    ImGui::Text("  GPU Memory:  %.2f / %.2f GB", gpuUsedGB, gpuLimitGB);
    ImGui::Separator();

    ImGui::Text("Eviction");
    ImGui::Text("  CPU Evictions: %u", stats.cpuEvictions);
    ImGui::Text("  GPU Evictions: %u", stats.gpuEvictions);
    ImGui::Separator();

    ImGui::Text("Performance");
    ImGui::Text("  Queue Size:  %u", stats.queueSize);

    ImGui::End();
}

void ImGuiOverlay::RenderVisualizationPanel(RenderConfig& config) {
    ImGui::SetNextWindowPos(ImVec2(320, 170), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Visualization");

    const char* modes[] = {"RGB", "Intensity", "Classification",
                           "Elevation", "Height Ramp", "Normal Shading", "Density"};
    int currentMode = static_cast<int>(config.visualizationMode);
    if (ImGui::Combo("Mode", &currentMode, modes, 7)) {
        config.visualizationMode = static_cast<VisualizationMode>(currentMode);
    }

    ImGui::Checkbox("Bounding Boxes", &config.showBoundingBoxes);
    ImGui::Checkbox("LOD Colors", &config.showLODColors);
    ImGui::Checkbox("Frustum", &config.showFrustum);
    ImGui::Checkbox("Frustum Culling", &config.frustumCulling);

    ImGui::SliderFloat("Intensity Min", &config.intensityMin, 0.0f, 1.0f);
    ImGui::SliderFloat("Intensity Max", &config.intensityMax, 0.0f, 1.0f);

    ImGui::End();
}

void ImGuiOverlay::RenderVisibilityPanel(const VisibilityDebugStats& stats, RenderConfig& config) {
    ImGui::SetNextWindowPos(ImVec2(640, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 220), ImGuiCond_FirstUseEver);
    ImGui::Begin("Visibility");

    ImGui::Text("Total Nodes:   %u", stats.totalNodes);
    ImGui::Text("Visible Nodes: %u", stats.visibleNodes);
    ImGui::Text("Culled Nodes:  %u", stats.culledNodes);
    ImGui::Separator();
    ImGui::Text("Visible Points: %llu", stats.visiblePoints);
    ImGui::Separator();
    ImGui::Text("Frustum Tests: %u", stats.frustumTests);
    ImGui::Text("Cache Hits:    %u", stats.cacheHits);
    ImGui::Text("Visibility:    %.3f ms", stats.visibilityTimeMs);
    ImGui::Separator();

    ImGui::Checkbox("Frustum Culling", &config.frustumCulling);
    ImGui::Checkbox("Show Node Bounds", &config.showBoundingBoxes);
    ImGui::Checkbox("Show Visible Nodes", &config.showVisibleNodes);
    ImGui::Checkbox("Show Frustum", &config.showFrustum);

    ImGui::End();
}

void ImGuiOverlay::SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.12f, 0.94f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.22f, 0.54f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
}

} // namespace renderer
} // namespace workstation
