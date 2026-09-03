#pragma once
#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/DwgAttachment.h"
#include "workstation/cad/SntAttachment.h"
#include "workstation/cad/AttachmentManager.h"
#include "workstation/cad/LayerManager.h"
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/spatial/BoundingBox.h"
#include "workstation/spatial/CoordinateNormalizationManager.h"
#include "workstation/renderer/RenderCommand.h"
#include "workstation/renderer/RenderQueue.h"

#include <vector>
#include <memory>
#include <string>
#include <cstdint>

namespace workstation {
namespace renderer {

struct CadLineVertex {
    float position[3];
    float color[3];
};

struct CadPointVertex {
    float position[3];
    float color[3];
};

struct CadSurfaceVertex {
    float position[3];
    float normal[3];
    float color[3];
};

class CadRenderer {
public:
    CadRenderer() = default;
    ~CadRenderer();

    CadRenderer(const CadRenderer&) = delete;
    CadRenderer& operator=(const CadRenderer&) = delete;

    bool Initialize(vulkan::VulkanAllocator* allocator);
    void Shutdown();

    void SetCoordinateNormalizer(spatial::CoordinateNormalizationManager* normalizer);

    void LoadDxfAttachment(cad::DxfAttachment* attachment);
    void LoadDwgAttachment(cad::DwgAttachment* attachment);
    void LoadSntAttachment(cad::SntAttachment* attachment);
    void RemoveAttachment(cad::DxfAttachment* attachment);
    void RemoveAttachment(cad::DwgAttachment* attachment);
    void RemoveAttachment(cad::SntAttachment* attachment);
    void RemoveAllAttachments();

    void SetLayerVisibility(const std::string& layerName, bool visible);
    bool IsLayerVisible(const std::string& layerName) const;

    void BuildGeometry(cad::DxfAttachment* attachment);
    void BuildGeometry(cad::DwgAttachment* attachment);
    void BuildGeometry(cad::SntAttachment* attachment);

    void SubmitCommands(RenderQueue& queue, VkPipeline linePipeline,
                        VkPipelineLayout pipelineLayout);

    uint32_t GetLineVertexCount() const { return m_lineVertexCount; }
    uint32_t GetPointVertexCount() const { return m_pointVertexCount; }
    uint32_t GetTriangleIndexCount() const { return m_triangleIndexCount; }

    const vulkan::GPUBuffer& GetLineVertexBuffer() const { return m_lineVertexBuffer; }
    const vulkan::GPUBuffer& GetPointVertexBuffer() const { return m_pointVertexBuffer; }
    const vulkan::GPUBuffer& GetSurfaceVertexBuffer() const { return m_surfaceVertexBuffer; }
    const vulkan::GPUBuffer& GetIndexBuffer() const { return m_indexBuffer; }

    bool HasGeometry() const { return m_lineVertexCount > 0 || m_pointVertexCount > 0; }

    struct AttachmentGeometry {
        cad::DxfAttachment* dxf = nullptr;
        cad::DwgAttachment* dwg = nullptr;
        cad::SntAttachment* snt = nullptr;
        uint32_t lineVertexOffset = 0;
        uint32_t lineVertexCount = 0;
        uint32_t pointVertexOffset = 0;
        uint32_t pointVertexCount = 0;
        uint32_t triangleIndexOffset = 0;
        uint32_t triangleIndexCount = 0;
        spatial::BoundingBox bounds = {};
    };

    const std::vector<AttachmentGeometry>& GetAttachments() const { return m_attachments; }

private:
    void RebuildAllGeometry();
    void UploadBuffers();
    void ClearBuffers();

    vulkan::VulkanAllocator* m_allocator = nullptr;
    spatial::CoordinateNormalizationManager* m_normalizer = nullptr;

    std::vector<AttachmentGeometry> m_attachments;

    std::vector<CadLineVertex> m_lineVertices;
    std::vector<CadPointVertex> m_pointVertices;
    std::vector<CadSurfaceVertex> m_surfaceVertices;
    std::vector<uint32_t> m_triangleIndices;

    uint32_t m_lineVertexCount = 0;
    uint32_t m_pointVertexCount = 0;
    uint32_t m_triangleVertexCount = 0;
    uint32_t m_triangleIndexCount = 0;

    vulkan::GPUBuffer m_lineVertexBuffer;
    vulkan::GPUBuffer m_pointVertexBuffer;
    vulkan::GPUBuffer m_surfaceVertexBuffer;
    vulkan::GPUBuffer m_indexBuffer;

    bool m_buffersDirty = true;
};

} // namespace renderer
} // namespace workstation
