#include "workstation/renderer/ImGuiOverlay.h"
#include "workstation/renderer/Renderer.h"
#include "workstation/renderer/DebugRenderer.h"
#include "workstation/renderer/VisualizationManager.h"
#include "workstation/tools/ToolManager.h"
#include "workstation/tools/SelectionTool.h"
#include "workstation/tools/MeasurementTool.h"
#include "workstation/tools/ClipTool.h"
#include "workstation/tools/SectionTool.h"
#include "workstation/tools/CrossSectionTool.h"
#include "workstation/tools/ClassificationTool.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

namespace workstation {
namespace renderer {

ImGuiOverlay::~ImGuiOverlay() { Shutdown(); }

bool ImGuiOverlay::Initialize(SDL_Window* window, VkInstance instance,
                               vulkan::VulkanDevice& device,
                               VkRenderPass renderPass, uint32_t imageCount) {
    window_ = window;
    device_ = device.GetDevice();

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
    // Let the backend manage its own descriptor pool sized to its actual
    // requirements (it needs both VK_DESCRIPTOR_TYPE_SAMPLER and
    // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, which a hand-rolled pool
    // easily under-provisions).
    initInfo.DescriptorPoolSize = 64;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    ImGui_ImplVulkan_Init(&initInfo);

    initialized_ = true;
    return true;
}

void ImGuiOverlay::Shutdown() {
    if (!initialized_) return;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
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
    ImGui::SetNextWindowSize(ImVec2(300, 270), ImGuiCond_FirstUseEver);
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

void ImGuiOverlay::RenderVisibilityPanel(const VisibilityDebugStats& stats, RenderConfig& config) {
    ImGui::SetNextWindowPos(ImVec2(630, 10), ImGuiCond_FirstUseEver);
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

void ImGuiOverlay::RenderToolsPanel(tools::ToolManager& toolManager, RenderContext& context) {
    ImGui::SetNextWindowPos(ImVec2(630, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_FirstUseEver);
    ImGui::Begin("Tools");

    toolManager.RenderUI(context);

    ImGui::End();
}

void ImGuiOverlay::RenderDebugOverlay(DebugRenderer& debugRenderer, RenderContext& context) {
    ImGui::SetNextWindowPos(ImVec2(940, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug Overlay");

    auto& lines = debugRenderer.GetLines();
    auto& boxes = debugRenderer.GetBoxes();
    auto& texts = debugRenderer.GetTexts();

    ImGui::Text("Lines: %zu", lines.size());
    ImGui::Text("Boxes: %zu", boxes.size());
    ImGui::Text("Texts: %zu", texts.size());
    ImGui::Separator();

    for (size_t i = 0; i < boxes.size() && i < 10; ++i) {
        const auto& box = boxes[i];
        ImGui::Text("Box %zu: [%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f]",
                    i,
                    box.bounds.minX, box.bounds.minY, box.bounds.minZ,
                    box.bounds.maxX, box.bounds.maxY, box.bounds.maxZ);
    }
    if (boxes.size() > 10) {
        ImGui::Text("... and %zu more boxes", boxes.size() - 10);
    }

    ImGui::Separator();
    for (size_t i = 0; i < texts.size() && i < 5; ++i) {
        const auto& txt = texts[i];
        ImGui::Text("Text %zu: %s", i, txt.text.c_str());
    }
    if (texts.size() > 5) {
        ImGui::Text("... and %zu more texts", texts.size() - 5);
    }

    if (ImGui::Button("Clear Debug")) {
        debugRenderer.Clear();
    }

    ImGui::End();
}

void ImGuiOverlay::RenderVisualizationManagerPanel(VisualizationManager& vizManager, RenderContext& context) {
    ImGui::SetNextWindowPos(ImVec2(320, 290), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("Visualization Manager");

    vizManager.RenderUI(context);

    ImGui::End();
}

} // namespace renderer
} // namespace workstation
