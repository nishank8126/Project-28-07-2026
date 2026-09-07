#pragma once
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/vulkan/VulkanHeaders.h"
#include "workstation/vulkan/VulkanPipelineManager.h"
#include "workstation/vulkan/VulkanShaderManager.h"
#include "workstation/vulkan/VulkanDescriptorManager.h"
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/surface/SurfaceGPUBuffer.h"
#include "workstation/surface/SurfaceMeshGenerator.h"
#include "workstation/surface/SurfaceLODManager.h"
#include "workstation/renderer/Camera.h"
#include "workstation/spatial/BoundingBox.h"

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

namespace workstation {
namespace vulkan { class VulkanDevice; class VulkanSwapchain; class VulkanRenderPass; }
namespace renderer { struct RendererConfig; }

namespace surface {

enum class SurfaceMode {
    Points = 0,
    Surface = 1,
    Hybrid = 2,
    Wireframe = 3,
    ShadedSurface = 4
};

enum class ShadingType {
    Phong = 0,
    DepthShading = 1,
    SurfaceShading = 2,
    EyeDomeLighting = 3,
    SurfaceDebug = 4,
    Unlit = 5,
    ElevationHeatmap = 6,   // blue -> cyan -> green -> yellow -> red
    Hillshade = 7,          // elevation colormap + Phong lighting
    Slope = 8,              // flat=green, steep=red
    Aspect = 9,             // compass direction coloring
    ElevationComposite = 10, // elevation + hillshade + EDL
    PTCClassification = 11, // PTC classification colors
    PTC_Hillshade = 12,     // PTC color + Phong lighting
    ElevationPTCComposite = 13, // elevation heatmap + PTC tint
    // MicroStation-style PTC Shading modes
    PTCShading = 14,        // PTC classification color + Phong lighting
    PTCEDL = 15,            // PTC classification color + EDL
    PTCComposite = 16,      // PTC color + Phong + EDL + composite depth
    // Temporary debug modes (remove after validation)
    DebugPTCOnly = 17,      // PTC base color only
    DebugNormals = 18,      // Normal visualization
    DebugNdotL = 19,        // NdotL lighting factor
    DebugLightingOnly = 20, // Phong on white
    DebugPTCLighting = 21,  // PTC x Lighting
    DebugDepth = 22,        // Depth visualization
    DebugEDL = 23,          // EDL visualization
    DebugAO = 24,           // AO visualization
};

struct SurfaceRenderParams {
    SurfaceMode mode = SurfaceMode::Points;
    ShadingType shading = ShadingType::Phong;
    float ambient = 0.1f;
    float diffuse = 1.0f;
    float specular = 0.05f;
    float shininess = 32.0f;
    float depthMin = 0.0f;
    float depthMax = 1000.0f;
    float edlStrength = 1.5f;
    float lightDirX = 0.35f;
    float lightDirY = 0.35f;
    float lightDirZ = 0.87f;
    float elevationMin = 0.0f;
    float elevationMax = 1000.0f;
};

struct SurfacePushConstants {
    float model[16];
    float view[16];
    float projection[16];
    float lightDir[3];
    float surfaceAlpha = 1.0f;   // GLSL vec4 lightDir.w: surface alpha (1.0 solid, <1.0 hybrid)
    float cameraPos[3];
    float pad1 = 0.0f;
    float material[4];
    float shadingParams[4];      // GLSL vec4: x=shadingMode, y=depthMin, z=depthMax, w=edlStrength
};

struct SurfaceInitParams {
    vulkan::VulkanDevice* device = nullptr;
    vulkan::VulkanAllocator* allocator = nullptr;
    vulkan::VulkanPipelineManager* pipelineManager = nullptr;
    vulkan::VulkanShaderManager* shaderManager = nullptr;
    vulkan::VulkanDescriptorManager* descriptorManager = nullptr;
    vulkan::VulkanSwapchain* swapchain = nullptr;
    vulkan::VulkanRenderPass* renderPass = nullptr;
};

class SurfaceRenderer {
public:
    SurfaceRenderer() = default;
    ~SurfaceRenderer() = default;

    SurfaceRenderer(const SurfaceRenderer&) = delete;
    SurfaceRenderer& operator=(const SurfaceRenderer&) = delete;

    bool Initialize(const SurfaceInitParams& params);
    void Shutdown();

    uint32_t AddSurfaceMesh(const SurfaceMesh& mesh);
    void RemoveSurfaceMesh(uint32_t meshID);
    void ClearAllMeshes();

    void GenerateSurfaceFromCloud(pointcloud::PointCloud& cloud,
                                   const SurfaceGenerationParams& params = {});

    void GenerateLODs(pointcloud::PointCloud& cloud,
                      const SurfaceGenerationParams& params = {});
    const SurfaceLODManager& GetLODManager() const { return lodManager_; }
    SurfaceLODManager& GetLODManager() { return lodManager_; }

    void Render(VkCommandBuffer cmd, const renderer::Camera& camera,
                const SurfaceRenderParams& params = {});

    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }
    bool IsInitialized() const { return initialized_; }
    void SetMode(SurfaceMode mode) { params_.mode = mode; }
    SurfaceMode GetMode() const { return params_.mode; }
    void SetShading(ShadingType shading) { params_.shading = shading; }

    // Descriptor set pointing at the shared classification palette SSBO
    // (owned by Renderer). The surface pipeline layout declares the SAME
    // set 0 / binding 0 the point pipeline reads, so every PTC surface
    // shading mode looks up the SAME palette buffer (no duplicated palettes).
    // Without this binding the SSBO read is undefined -> grey PTC output.
    void SetClassificationDescriptorSet(VkDescriptorSet set) { classificationSet_ = set; }

    uint32_t GetMeshCount() const { return static_cast<uint32_t>(meshes_.size()); }
    uint32_t GetTotalTriangleCount() const;
    uint32_t GetTotalVertexCount() const;
    size_t GetGPUMemoryUsage() const;

    // Phase 12 metrics from the most recent surface generation (either the
    // renderer's own generator or the SurfaceMeshCache path via
    // SetLastGenerationStats).
    void SetLastGenerationStats(const SurfaceGenerationStats& stats) { lastGenStats_ = stats; }
    const SurfaceGenerationStats& GetLastGenerationStats() const { return lastGenStats_; }

    void SetParams(const SurfaceRenderParams& params) { params_ = params; }
    SurfaceRenderParams& GetParams() { return params_; }

private:
    bool CreatePipeline();
    bool CreateWireframePipeline();
    void DestroyPipeline();
    void UpdatePushConstants(VkCommandBuffer cmd, const renderer::Camera& camera);

    struct SurfaceMeshEntry {
        SurfaceMesh mesh;
        SurfaceGPUBuffer gpuBuffer;
        uint32_t id = 0;
        bool dirty = true;
    };

    SurfaceInitParams initParams_;
    vulkan::VulkanAllocator* allocator_ = nullptr;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipeline wireframePipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet classificationSet_ = VK_NULL_HANDLE;

    std::vector<std::unique_ptr<SurfaceMeshEntry>> meshes_;
    uint32_t nextMeshID_ = 1;
    bool initialized_ = false;
    bool visible_ = true;
    SurfaceRenderParams params_;
    SurfaceMeshGenerator meshGenerator_;
    SurfaceLODManager lodManager_;
    SurfaceGenerationStats lastGenStats_;
    // Render decision log de-duplication: only state *changes* are written
    // to the surface log, so per-frame calls stay silent.
    std::string lastRenderState_;
};

} // namespace surface
} // namespace workstation
