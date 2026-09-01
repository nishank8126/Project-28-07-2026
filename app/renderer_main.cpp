#include "workstation/renderer/Renderer.h"
#include "workstation/renderer/PointCloudRenderAdapter.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointStorage.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointChannelManager.h"
#include "workstation/pointcloud/BoundingBox.h"

#include <cstdio>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>

static workstation::pointcloud::PointCloud CreateSyntheticPointCloud(
    uint32_t pointCount, float radius) {
    workstation::pointcloud::PointCloud cloud;
    cloud.SetName("Synthetic Sphere");

    std::vector<float> positions(pointCount * 3);
    std::vector<float> colors(pointCount * 3);
    std::vector<float> intensities(pointCount);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (uint32_t i = 0; i < pointCount; ++i) {
        float theta = dist(rng) * 3.14159f;
        float phi = dist(rng) * 3.14159f;
        float r = radius * (0.5f + 0.5f * std::abs(dist(rng)));

        positions[i * 3 + 0] = r * std::sin(theta) * std::cos(phi);
        positions[i * 3 + 1] = r * std::sin(theta) * std::sin(phi);
        positions[i * 3 + 2] = r * std::cos(theta);

        float h = (positions[i * 3 + 1] / radius + 1.0f) * 0.5f;
        colors[i * 3 + 0] = h;
        colors[i * 3 + 1] = 1.0f - h;
        colors[i * 3 + 2] = 0.5f;

        intensities[i] = h;
    }

    workstation::pointcloud::BoundingBox bounds;
    bounds.minX = -radius; bounds.minY = -radius; bounds.minZ = -radius;
    bounds.maxX = radius; bounds.maxY = radius; bounds.maxZ = radius;

    auto node = std::make_unique<workstation::pointcloud::PointCloudNode>();
    node->setBounds(bounds);

    auto posChannel = workstation::pointcloud::CreateChannel(
        workstation::pointcloud::ChannelId::XYZ,
        workstation::pointcloud::PointFormat::Float32,
        pointCount, positions.data());

    auto colorChannel = workstation::pointcloud::CreateChannel(
        workstation::pointcloud::ChannelId::RGB,
        workstation::pointcloud::PointFormat::Float32,
        pointCount, colors.data());

    auto intensityChannel = workstation::pointcloud::CreateChannel(
        workstation::pointcloud::ChannelId::Intensity,
        workstation::pointcloud::PointFormat::Float32,
        pointCount, intensities.data());

    node->channels().AddChannel(std::move(posChannel));
    node->channels().AddChannel(std::move(colorChannel));
    node->channels().AddChannel(std::move(intensityChannel));

    cloud.SetRoot(node.release());
    cloud.Finalize();

    return cloud;
}

int main(int, char**) {
    printf("NakshaPointEngine :: First Point Cloud Render\n");
    printf("=============================================\n\n");

    workstation::renderer::RendererConfig config{};
    config.appName = "NakshaPointEngine";
    config.initialWidth = 1920;
    config.initialHeight = 1080;
    config.enableValidation = true;
    config.enableImGui = true;
    config.gpuPointBudget = 50'000'000;

    workstation::renderer::Renderer renderer;
    if (!renderer.Initialize(config)) {
        fprintf(stderr, "Failed to initialize renderer\n");
        return 1;
    }

    printf("Vulkan renderer initialized.\n");
    printf("Creating synthetic point cloud...\n");

    auto cloud = CreateSyntheticPointCloud(1'000'000, 100.0f);
    printf("Point cloud created: %llu points\n", cloud.PointCount());

    renderer.SetPointCloud(&cloud);
    printf("Point cloud uploaded to GPU.\n");

    auto& ctx = renderer.GetContext();
    auto& cam = ctx.GetCamera();
    cam.SetPerspective(45.0, 1920.0 / 1080.0, 0.1, 100000.0);

    printf("\nControls:\n");
    printf("  WASD     - Move\n");
    printf("  QE       - Up/Down\n");
    printf("  Mouse R  - Rotate\n");
    printf("  Mouse M  - Pan\n");
    printf("  Scroll   - Zoom\n");
    printf("  ESC      - Exit\n\n");

    bool running = true;
    SDL_Event event;

    auto lastTime = std::chrono::high_resolution_clock::now();
    uint32_t frameCount = 0;
    float fpsTimer = 0.0f;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
                if (event.key.scancode == SDL_SCANCODE_W) cam.MoveForward(2.0f);
                if (event.key.scancode == SDL_SCANCODE_S) cam.MoveForward(-2.0f);
                if (event.key.scancode == SDL_SCANCODE_A) cam.MoveRight(-2.0f);
                if (event.key.scancode == SDL_SCANCODE_D) cam.MoveRight(2.0f);
                if (event.key.scancode == SDL_SCANCODE_Q) cam.MoveUp(2.0f);
                if (event.key.scancode == SDL_SCANCODE_E) cam.MoveUp(-2.0f);
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (event.motion.state & SDL_BUTTON_RMASK)
                    cam.Rotate(event.motion.xrel * 0.3f, event.motion.yrel * 0.3f);
                if (event.motion.state & SDL_BUTTON_MMASK)
                    cam.Pan(event.motion.xrel * 0.05f, event.motion.yrel * 0.05f);
            }
            if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                cam.Zoom(event.wheel.y * 5.0f);
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        frameCount++;
        fpsTimer += dt;
        if (fpsTimer >= 1.0f) {
            ctx.GetStats().fps = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        renderer.BeginFrame();
        renderer.RenderFrame();
        renderer.EndFrame();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    printf("Shutting down...\n");
    renderer.Shutdown();
    return 0;
}
