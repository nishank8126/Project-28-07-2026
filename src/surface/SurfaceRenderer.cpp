#include "workstation/surface/SurfaceRenderer.h"
#include "workstation/vulkan/VulkanDevice.h"
#include "workstation/vulkan/VulkanSwapchain.h"
#include "workstation/vulkan/VulkanRenderPass.h"
#include "workstation/vulkan/VulkanPipelineManager.h"
#include "workstation/vulkan/VulkanShaderManager.h"
#include "workstation/vulkan/VulkanDescriptorManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/surface/SurfaceLog.h"

#include <cstring>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
static std::string GetSurfaceShaderDir() {
    char buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return "shaders/";
    std::string exePath(buf, len);
    auto pos = exePath.find_last_of("\\/");
    if (pos == std::string::npos) return "shaders/";
    return exePath.substr(0, pos + 1) + "shaders/";
}
#else
static std::string GetSurfaceShaderDir() {
    return "shaders/";
}
#endif

namespace workstation {
namespace surface {

static_assert(sizeof(SurfacePushConstants) == 256,
    "SurfacePushConstants must be exactly 256 bytes to match GLSL shaders");

bool SurfaceRenderer::Initialize(const SurfaceInitParams& params) {
    if (initialized_) return true;
    initParams_ = params;
    allocator_ = params.allocator;

    SLOG_INFO("Initialize: shaders='%s' allocator=%s",
              GetSurfaceShaderDir().c_str(),
              allocator_ ? "yes" : "NO (CPU-only: surfaces will not upload)");

    if (!CreatePipeline()) {
        SLOG_ERROR("Initialize: CreatePipeline FAILED - surface display disabled");
        return false;
    }
    if (!CreateWireframePipeline()) {
        SLOG_WARN("Initialize: wireframe pipeline failed (filled surface still available)");
    }

    initialized_ = true;
    SLOG_INFO("Initialize: OK (log file: %s)", SurfaceLog::GetPath());
    return true;
}

void SurfaceRenderer::Shutdown() {
    ClearAllMeshes();
    DestroyPipeline();
    if (wireframePipeline_ != VK_NULL_HANDLE && initParams_.pipelineManager) {
        initParams_.pipelineManager->DestroyPipeline(wireframePipeline_);
        wireframePipeline_ = VK_NULL_HANDLE;
    }
    initialized_ = false;
}

bool SurfaceRenderer::CreatePipeline() {
    if (!initParams_.pipelineManager || !initParams_.shaderManager || !initParams_.swapchain)
        return false;

    if (pipeline_ != VK_NULL_HANDLE) return true;

    auto shaderDir = GetSurfaceShaderDir();
    auto shaders = initParams_.shaderManager->LoadSPIRVFiles({
        {shaderDir + "surface.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {shaderDir + "surface.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    for (auto& shader : shaders) {
        if (shader.module == VK_NULL_HANDLE) {
            fprintf(stderr, "[SurfaceRenderer] Failed to load surface shaders\n");
            return false;
        }
    }

    // The surface fragment shader reads the shared classification palette
    // SSBO (set 0, binding 0) for every PTC shading mode. This layout was
    // previously created EMPTY, so the SSBO was never bound on the surface
    // pipeline: classificationColors[] read undefined (zero) data and every
    // PTC surface mode fell back to grey. Declaring the binding here (and
    // binding Renderer's classificationDescriptorSet_ in Render()) makes the
    // surface pipeline read the SAME palette buffer as the point pipeline.
    VkDescriptorSetLayout layout = initParams_.descriptorManager->CreateLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}});

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(SurfacePushConstants);

    pipelineLayout_ = initParams_.pipelineManager->CreatePipelineLayout({layout}, {pushRange});
    descriptorLayout_ = layout;

    vulkan::PipelineConfig pconfig;
    pconfig.SetDefaults();
    pconfig.colorFormat = initParams_.swapchain->GetImageFormat();

    pconfig.vertexBindings = {
        {0, sizeof(SurfaceVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    pconfig.vertexAttributes = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SurfaceVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SurfaceVertex, normal)},
{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SurfaceVertex, color)},
    {3, 0, VK_FORMAT_R32_UINT, offsetof(SurfaceVertex, classificationID)},
};
    pconfig.vertexInput.vertexBindingDescriptionCount =
        static_cast<uint32_t>(pconfig.vertexBindings.size());
    pconfig.vertexInput.pVertexBindingDescriptions = pconfig.vertexBindings.data();
    pconfig.vertexInput.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(pconfig.vertexAttributes.size());
    pconfig.vertexInput.pVertexAttributeDescriptions = pconfig.vertexAttributes.data();

    pconfig.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pconfig.rasterizer.cullMode = VK_CULL_MODE_NONE;
    pconfig.depthStencil.depthTestEnable = VK_TRUE;
    pconfig.depthStencil.depthWriteEnable = VK_TRUE;
    // NOTE: culling must stay OFF. The camera bakes a Y-flip into the
    // projection matrix (Camera.cpp: -1.0/tanHalfFov), which flips triangle
    // winding in the framebuffer, so our consistently CCW-wound terrain
    // triangles appear CW on screen and BACK_BIT + CCW would cull exactly
    // the faces we want to see (the #1 cause of "surface not showing").
    // Lighting is unaffected: the shader uses vertex normals, not winding.
    pconfig.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pconfig.rasterizer.lineWidth = 1.0f;

    pipeline_ = initParams_.pipelineManager->CreateGraphicsPipeline(
        pipelineLayout_, shaders, pconfig, initParams_.renderPass->GetRenderPass());

    for (auto& shader : shaders) {
        initParams_.shaderManager->DestroyShaderModule(shader.module);
    }

    SLOG_INFO("CreatePipeline: fill pipeline %s (triangle list, cull=NONE, depthTest=on)",
              pipeline_ != VK_NULL_HANDLE ? "OK" : "FAILED");
    return pipeline_ != VK_NULL_HANDLE;
}
bool SurfaceRenderer::CreateWireframePipeline() {
    if (!initParams_.pipelineManager || !initParams_.shaderManager || !initParams_.swapchain)
        return false;
    if (wireframePipeline_ != VK_NULL_HANDLE) return true;

    auto shaderDir = GetSurfaceShaderDir();
    auto shaders = initParams_.shaderManager->LoadSPIRVFiles({
        {shaderDir + "surface.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {shaderDir + "surface.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    for (auto& shader : shaders) {
        if (shader.module == VK_NULL_HANDLE) {
            fprintf(stderr, "[SurfaceRenderer] Failed to load wireframe shaders\n");
            return false;
        }
    }

    vulkan::PipelineConfig pconfig;
    pconfig.SetDefaults();
    pconfig.colorFormat = initParams_.swapchain->GetImageFormat();

    pconfig.vertexBindings = {
        {0, sizeof(SurfaceVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    pconfig.vertexAttributes = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SurfaceVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SurfaceVertex, normal)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SurfaceVertex, color)},
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

    wireframePipeline_ = initParams_.pipelineManager->CreateGraphicsPipeline(
        pipelineLayout_, shaders, pconfig, initParams_.renderPass->GetRenderPass());

    for (auto& shader : shaders) {
        initParams_.shaderManager->DestroyShaderModule(shader.module);
    }

    SLOG_INFO("CreateWireframePipeline: edge pipeline %s (line list, cull=NONE)",
              wireframePipeline_ != VK_NULL_HANDLE ? "OK" : "FAILED");
    return wireframePipeline_ != VK_NULL_HANDLE;
}

void SurfaceRenderer::DestroyPipeline() {
    if (pipeline_ != VK_NULL_HANDLE && initParams_.pipelineManager) {
        initParams_.pipelineManager->DestroyPipeline(pipeline_);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE && initParams_.pipelineManager) {
        initParams_.pipelineManager->DestroyPipelineLayout(pipelineLayout_);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    descriptorLayout_ = VK_NULL_HANDLE;
}

uint32_t SurfaceRenderer::AddSurfaceMesh(const SurfaceMesh& mesh) {
    auto entry = std::make_unique<SurfaceMeshEntry>();
    entry->id = nextMeshID_++;
    entry->mesh = mesh;
    entry->mesh.ComputeEdges();

    if (allocator_) {
        entry->gpuBuffer.Initialize(*allocator_);
        bool uploaded = entry->gpuBuffer.UploadMesh(entry->mesh);
        entry->dirty = false;
        if (uploaded) {
            SLOG_INFO("AddSurfaceMesh: id=%u verts=%zu tris=%zu edges=%zu gpu=%.2f MB",
                      entry->id,
                      static_cast<size_t>(entry->mesh.VertexCount()),
                      static_cast<size_t>(entry->mesh.TriangleCount()),
                      static_cast<size_t>(entry->mesh.EdgeCount()),
                      (entry->gpuBuffer.GetVertexBufferSize() +
                       entry->gpuBuffer.GetIndexBufferSize()) / (1024.0 * 1024.0));
        } else {
            // UploadMesh() failed (see its own early-returns - most likely
            // GPU buffer allocation itself failed) and reset vertex/index
            // counts to 0, so HasGeometry() will be false and this mesh will
            // silently never draw. Logging this as if it succeeded (with a
            // misleading "0.00 MB") is exactly what made a real allocation
            // failure look like an empty-but-fine upload for hours of
            // debugging - flag it loudly instead.
            SLOG_WARN("AddSurfaceMesh: id=%u GPU UPLOAD FAILED (verts=%zu tris=%zu) - "
                      "mesh will not render",
                      entry->id,
                      static_cast<size_t>(entry->mesh.VertexCount()),
                      static_cast<size_t>(entry->mesh.TriangleCount()));
        }
    } else {
        entry->dirty = true;
        SLOG_WARN("AddSurfaceMesh: id=%u kept CPU-only (no allocator - will not draw)",
                  entry->id);
    }

    uint32_t id = entry->id;
    meshes_.push_back(std::move(entry));
    return id;
}

void SurfaceRenderer::RemoveSurfaceMesh(uint32_t meshID) {
    meshes_.erase(
        std::remove_if(meshes_.begin(), meshes_.end(),
                        [meshID](const std::unique_ptr<SurfaceMeshEntry>& e) {
                            return e->id == meshID;
                        }),
        meshes_.end());
}

void SurfaceRenderer::ClearAllMeshes() {
    for (auto& entry : meshes_) {
        if (entry->gpuBuffer.IsInitialized()) {
            entry->gpuBuffer.Shutdown();
        }
    }
    if (!meshes_.empty()) {
        SLOG_INFO("ClearAllMeshes: removed %u mesh(es)",
                  static_cast<uint32_t>(meshes_.size()));
    }
    meshes_.clear();
}

void SurfaceRenderer::GenerateSurfaceFromCloud(pointcloud::PointCloud& cloud,
                                                  const SurfaceGenerationParams& params) {
    auto mesh = meshGenerator_.Generate(cloud, params);
    lastGenStats_ = meshGenerator_.GetLastStats();
    if (!mesh.IsEmpty()) {
        mesh.SetName(cloud.Name() ? cloud.Name() : "Surface");
        AddSurfaceMesh(mesh);
    }
}

void SurfaceRenderer::GenerateLODs(pointcloud::PointCloud& cloud,
                                    const SurfaceGenerationParams& params) {
    lodManager_.GenerateLODs(cloud, params);

    ClearAllMeshes();

    for (uint32_t i = 0; i < lodManager_.GetLODCount(); ++i) {
        const auto* mesh = lodManager_.GetMesh(i);
        if (mesh && !mesh->IsEmpty()) {
            SurfaceMesh m = *mesh;
            m.SetName(std::string("LOD") + std::to_string(i));
            AddSurfaceMesh(m);
        }
    }

    fprintf(stderr, "[SurfaceRenderer] LOD generation complete: %u levels, %u meshes in GPU\n",
            lodManager_.GetLODCount(), GetMeshCount());
    fflush(stderr);
}

void SurfaceRenderer::UpdatePushConstants(VkCommandBuffer cmd,
                                            const renderer::Camera& camera) {
    SurfacePushConstants pc = {};

    const float identity[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    memcpy(pc.model, identity, sizeof(identity));

    auto& view = camera.GetViewMatrix();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            pc.view[c * 4 + r] = static_cast<float>(view(r, c));

    auto& proj = camera.GetProjectionMatrix();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            pc.projection[c * 4 + r] = static_cast<float>(proj(r, c));

    auto pos = camera.GetPosition();
    pc.cameraPos[0] = static_cast<float>(pos.x);
    pc.cameraPos[1] = static_cast<float>(pos.y);
    pc.cameraPos[2] = static_cast<float>(pos.z);

    pc.lightDir[0] = params_.lightDirX;
    pc.lightDir[1] = params_.lightDirY;
    pc.lightDir[2] = params_.lightDirZ;
    // Hybrid mode overlays the mesh semi-transparently on top of the point
    // cloud (which the point pipeline renders first); all other surface modes
    // draw the mesh fully opaque.
    pc.surfaceAlpha = (params_.mode == SurfaceMode::Hybrid) ? 0.35f : 1.0f;

    pc.material[0] = params_.ambient;
    pc.material[1] = params_.diffuse;
    pc.material[2] = params_.specular;
    pc.material[3] = params_.shininess;

    // Wireframe passes draw crisp unlit edges; the actual surface shading
    // choice is preserved in params_.shading for when the user switches back.
    float shadingMode = static_cast<float>(static_cast<int>(params_.shading));
    if (params_.mode == SurfaceMode::Wireframe) {
        shadingMode = static_cast<float>(static_cast<int>(ShadingType::Unlit));
    }
    pc.shadingParams[0] = shadingMode;

    // Elevation modes repurpose depthMin/depthMax for elevation range
    bool isElev = (params_.shading == ShadingType::ElevationHeatmap ||
                   params_.shading == ShadingType::Hillshade ||
                   params_.shading == ShadingType::Slope ||
                   params_.shading == ShadingType::Aspect ||
                   params_.shading == ShadingType::ElevationComposite);
    pc.shadingParams[1] = isElev ? params_.elevationMin : params_.depthMin;
    pc.shadingParams[2] = isElev ? params_.elevationMax : params_.depthMax;
    pc.shadingParams[3] = params_.edlStrength;

    vkCmdPushConstants(cmd, pipelineLayout_,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(pc), &pc);
}

void SurfaceRenderer::Render(VkCommandBuffer cmd, const renderer::Camera& camera,
                               const SurfaceRenderParams& params) {
    // -----------------------------------------------------------------------
    // SHADING GEOMETRY DEBUG - print once per session
    // -----------------------------------------------------------------------
    static bool diagPrinted = false;
    if (!diagPrinted && !meshes_.empty()) {
        uint32_t totalVerts = GetTotalVertexCount();
        uint32_t totalTris = GetTotalTriangleCount();
        SLOG_INFO("\n[SHADING GEOMETRY DEBUG]"
                  "\n  Visualization mode: PTC Shaded"
                  "\n  Shading mode: %d (%s)"
                  "\n  Surface mode: %d"
                  "\n  Rendering object: SurfaceMesh (ElevationGrid/Triangulated)"
                  "\n  Vertex count: %u"
                  "\n  Triangle count: %u"
                  "\n  Normal source: Vertex normal (pre-computed from mesh)"
                  "\n  Surface GPU buffer: VALID"
                  "\n  Point GPU buffer: N/A (surface path)",
                  static_cast<int>(params.shading),
                  (params.shading == ShadingType::PTCShading) ? "PTCShading" :
                  (params.shading == ShadingType::PTCEDL) ? "PTCEDL" :
                  (params.shading == ShadingType::PTCComposite) ? "PTCComposite" :
                  "Other",
                  static_cast<int>(params.mode),
                  totalVerts, totalTris);
        diagPrinted = true;
    }

    const char* skip = nullptr;
    if (!initialized_)                    skip = "not-initialized";
    else if (!visible_)                   skip = "hidden";
    else if (meshes_.empty())             skip = "no-mesh-generated";
    else if (pipeline_ == VK_NULL_HANDLE) skip = "fill-pipeline-failed";
    if (skip) {
        if (lastRenderState_ != skip) {
            SLOG_WARN("Render: surface skipped (%s)", skip);
            lastRenderState_ = skip;
        }
        return;
    }

    if (params.shading != params_.shading) {
        params_.shading = params.shading;
    }
    if (params.mode != params_.mode) {
        params_.mode = params.mode;
    }

    if (params_.mode == SurfaceMode::Points) {
        if (lastRenderState_ != "points-mode") {
            SLOG_INFO("Render: points mode - surface pass idle");
            lastRenderState_ = "points-mode";
        }
        return;
    }

    // Frustum culling: skip entire surface if bounds are outside the view.
    if (lodManager_.GetLODCount() > 0 && !lodManager_.IsVisible(camera)) {
        if (lastRenderState_ != "frustum-culled") {
            SLOG_INFO("Render: frustum culled");
            lastRenderState_ = "frustum-culled";
        }
        return;
    }

    // LOD selection: pick the mesh level appropriate for current camera distance.
    uint32_t activeLOD = 0;
    if (lodManager_.GetLODCount() > 0) {
        activeLOD = lodManager_.SelectLOD(camera);
    }

    const bool drawTriangles =
        params_.mode == SurfaceMode::Surface ||
        params_.mode == SurfaceMode::ShadedSurface ||
        params_.mode == SurfaceMode::Hybrid;
    const bool drawWireframe = params_.mode == SurfaceMode::Wireframe;

    VkPipeline activePipeline = VK_NULL_HANDLE;
    if (drawTriangles) {
        activePipeline = pipeline_;
    } else if (drawWireframe) {
        activePipeline = wireframePipeline_;
    }
    if (activePipeline == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);

    // Bind the shared classification palette SSBO (set 0, binding 0) so all
    // PTC surface shading modes read the SAME palette as the point pipeline.
    if (classificationSet_ != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout_, 0, 1, &classificationSet_, 0, nullptr);
        static bool logged = false;
        if (!logged) {
            fprintf(stderr, "[SurfaceRenderer] Bound classification SSBO descriptor set=%p "
                            "(must match the point pipeline's set for shared-palette lookup)\n",
                    (void*)classificationSet_);
            fflush(stderr);
            logged = true;
        }
    }

    UpdatePushConstants(cmd, camera);

    // With LOD, draw only the mesh at the selected LOD level.
    // Without LOD, draw all meshes (legacy single-mesh path).
    if (lodManager_.GetLODCount() > 0) {
        if (activeLOD < meshes_.size()) {
            auto& entry = meshes_[activeLOD];
            if (entry->dirty && allocator_) {
                const_cast<SurfaceMeshEntry*>(entry.get())->mesh.ComputeEdges();
                const_cast<SurfaceGPUBuffer&>(entry->gpuBuffer).Shutdown();
                const_cast<SurfaceGPUBuffer&>(entry->gpuBuffer).Initialize(*allocator_);
                const_cast<SurfaceGPUBuffer&>(entry->gpuBuffer).UploadMesh(entry->mesh);
                const_cast<SurfaceMeshEntry*>(entry.get())->dirty = false;
            }
            if (drawWireframe) {
                if (entry->gpuBuffer.HasEdges()) entry->gpuBuffer.DrawIndexedEdges(cmd);
            } else {
                if (entry->gpuBuffer.HasGeometry()) entry->gpuBuffer.DrawIndexed(cmd);
            }
        }
    } else {
        for (const auto& entry : meshes_) {
            if (entry->dirty && allocator_) {
                const_cast<SurfaceMeshEntry*>(entry.get())->mesh.ComputeEdges();
                const_cast<SurfaceGPUBuffer&>(entry->gpuBuffer).Shutdown();
                const_cast<SurfaceGPUBuffer&>(entry->gpuBuffer).Initialize(*allocator_);
                const_cast<SurfaceGPUBuffer&>(entry->gpuBuffer).UploadMesh(entry->mesh);
                const_cast<SurfaceMeshEntry*>(entry.get())->dirty = false;
            }
            if (drawWireframe) {
                if (entry->gpuBuffer.HasEdges()) entry->gpuBuffer.DrawIndexedEdges(cmd);
            } else {
                if (entry->gpuBuffer.HasGeometry()) entry->gpuBuffer.DrawIndexed(cmd);
            }
        }
    }
}

uint32_t SurfaceRenderer::GetTotalTriangleCount() const {
    uint32_t total = 0;
    for (const auto& entry : meshes_) {
        total += entry->gpuBuffer.GetTriangleCount();
    }
    return total;
}

uint32_t SurfaceRenderer::GetTotalVertexCount() const {
    uint32_t total = 0;
    for (const auto& entry : meshes_) {
        total += entry->gpuBuffer.GetVertexCount();
    }
    return total;
}

size_t SurfaceRenderer::GetGPUMemoryUsage() const {
    size_t total = 0;
    for (const auto& entry : meshes_) {
        total += entry->gpuBuffer.GetVertexBufferSize();
        total += entry->gpuBuffer.GetIndexBufferSize();
    }
    return total;
}

} // namespace surface
} // namespace workstation
