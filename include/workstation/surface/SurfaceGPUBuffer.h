#pragma once
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/surface/SurfaceMesh.h"
#include "workstation/renderer/RenderCommand.h"

#include <cstdint>

namespace workstation {
namespace surface {

class SurfaceGPUBuffer {
public:
    SurfaceGPUBuffer() = default;
    ~SurfaceGPUBuffer() = default;

    SurfaceGPUBuffer(const SurfaceGPUBuffer&) = delete;
    SurfaceGPUBuffer& operator=(const SurfaceGPUBuffer&) = delete;

    bool Initialize(vulkan::VulkanAllocator& allocator);
    void Shutdown();

    bool UploadMesh(const SurfaceMesh& mesh);
    void Clear();

    bool IsInitialized() const { return initialized_; }
    bool HasGeometry() const { return vertexCount_ > 0 && indexCount_ > 0; }
    bool HasEdges() const { return edgeCount_ > 0; }

    uint32_t GetVertexCount() const { return vertexCount_; }
    uint32_t GetIndexCount() const { return indexCount_; }
    uint32_t GetTriangleCount() const { return indexCount_ / 3; }
    uint32_t GetEdgeCount() const { return edgeCount_; }

    VkBuffer GetVertexBuffer() const { return vertexBuffer_.buffer; }
    VkBuffer GetIndexBuffer() const { return indexBuffer_.buffer; }
    VkBuffer GetEdgeIndexBuffer() const { return edgeIndexBuffer_.buffer; }
    VkDeviceSize GetVertexBufferSize() const { return vertexBuffer_.size; }
    VkDeviceSize GetIndexBufferSize() const { return indexBuffer_.size; }
    VkDeviceSize GetEdgeIndexBufferSize() const { return edgeIndexBuffer_.size; }

    void DrawIndexed(VkCommandBuffer cmd) const;
    void DrawIndexedEdges(VkCommandBuffer cmd) const;

    const spatial::BoundingBox& GetBounds() const { return bounds_; }
    uint64_t GetRevision() const { return revision_; }

private:
    vulkan::VulkanAllocator* allocator_ = nullptr;
    vulkan::GPUBuffer vertexBuffer_ = {};
    vulkan::GPUBuffer indexBuffer_ = {};
    vulkan::GPUBuffer edgeIndexBuffer_ = {};
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    uint32_t edgeCount_ = 0;
    uint64_t revision_ = 0;
    spatial::BoundingBox bounds_ = {};
    bool initialized_ = false;
};

} // namespace surface
} // namespace workstation
