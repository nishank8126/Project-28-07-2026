#include "workstation/surface/SurfaceGPUBuffer.h"

#include <cstdio>
#include <cstring>

namespace workstation {
namespace surface {

bool SurfaceGPUBuffer::Initialize(vulkan::VulkanAllocator& allocator) {
    allocator_ = &allocator;
    initialized_ = true;
    return true;
}

void SurfaceGPUBuffer::Shutdown() {
    Clear();
    initialized_ = false;
}

namespace {

// Uploads `size` bytes of host data into a GPU-ONLY `dst` buffer through a
// CPU-visible staging buffer using a single-time command buffer. Returns
// false on failure; on failure `dst` is destroyed and zeroed by the caller.
bool UploadThroughStaging(vulkan::VulkanAllocator& allocator,
                          VkBuffer dstBuffer,
                          const void* data,
                          VkDeviceSize size) {
    if (size == 0 || !data) return true;

    auto staging = allocator.CreateBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        // HOST_ACCESS_SEQUENTIAL_WRITE_BIT alone only steers VMA's memory
        // type selection - it does NOT populate allocationInfo.pMappedData.
        // That needs MAPPED_BIT too (every other CreateBuffer call site for
        // a staging/CPU-writable buffer in this codebase already pairs the
        // two; this was the one place that didn't, which is exactly why
        // `mapped` below always came back null and every surface mesh
        // upload silently failed).
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);
    if (!staging.IsValid()) {
        fprintf(stderr, "[SurfaceGPUBuffer] UploadThroughStaging: staging buffer "
                         "CreateBuffer returned invalid handle (size=%llu)\n",
                static_cast<unsigned long long>(size));
        return false;
    }

    void* mapped = staging.mappedData;
    if (!mapped) {
        fprintf(stderr, "[SurfaceGPUBuffer] UploadThroughStaging: staging buffer has no "
                         "mappedData (size=%llu) - HOST_ACCESS_SEQUENTIAL_WRITE mapping failed\n",
                static_cast<unsigned long long>(size));
        allocator.DestroyBuffer(staging);
        return false;
    }

    memcpy(mapped, data, size);
    staging.FlushMapped();

    auto cmd = allocator.BeginSingleTimeCommands();
    if (cmd == VK_NULL_HANDLE) {
        fprintf(stderr, "[SurfaceGPUBuffer] UploadThroughStaging: "
                         "BeginSingleTimeCommands returned VK_NULL_HANDLE (size=%llu)\n",
                static_cast<unsigned long long>(size));
        allocator.DestroyBuffer(staging);
        return false;
    }
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, staging.buffer, dstBuffer, 1, &copyRegion);
    allocator.EndSingleTimeCommands(cmd);

    allocator.DestroyBuffer(staging);
    return true;
}

} // namespace

bool SurfaceGPUBuffer::UploadMesh(const SurfaceMesh& mesh) {
    if (!initialized_ || !allocator_ || mesh.IsEmpty()) return false;

    // Uploading a new mesh replaces the previous one entirely.
    Clear();

    vertexCount_ = mesh.VertexCount();
    indexCount_ = mesh.IndexCount();
    edgeCount_ = mesh.EdgeCount();
    bounds_ = mesh.GetBounds();

    if (vertexCount_ == 0 || indexCount_ == 0) return false;

    VkDeviceSize vertexSize = vertexCount_ * sizeof(SurfaceVertex);
    vertexBuffer_ = allocator_->CreateBuffer(
        vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    if (!vertexBuffer_.IsValid()) {
        vertexCount_ = 0;
        return false;
    }

    if (!UploadThroughStaging(*allocator_, vertexBuffer_.buffer,
                              mesh.Vertices().data(), vertexSize)) {
        allocator_->DestroyBuffer(vertexBuffer_);
        vertexBuffer_ = {};
        vertexCount_ = 0;
        return false;
    }

    VkDeviceSize indexSize = indexCount_ * sizeof(uint32_t);
    indexBuffer_ = allocator_->CreateBuffer(
        indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    if (!indexBuffer_.IsValid()) {
        allocator_->DestroyBuffer(vertexBuffer_);
        vertexBuffer_ = {};
        vertexCount_ = 0;
        return false;
    }

    auto indices = mesh.GetIndexBuffer();
    if (!UploadThroughStaging(*allocator_, indexBuffer_.buffer,
                              indices.data(), indexSize)) {
        allocator_->DestroyBuffer(indexBuffer_);
        allocator_->DestroyBuffer(vertexBuffer_);
        indexBuffer_ = {};
        vertexBuffer_ = {};
        vertexCount_ = 0;
        indexCount_ = 0;
        return false;
    }

    // Optional unique-edge index buffer for wireframe rendering.
    edgeCount_ = mesh.EdgeCount();
    if (edgeCount_ > 0) {
        VkDeviceSize edgeSize = edgeCount_ * sizeof(uint32_t);
        edgeIndexBuffer_ = allocator_->CreateBuffer(
            edgeSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        if (!edgeIndexBuffer_.IsValid() ||
            !UploadThroughStaging(*allocator_, edgeIndexBuffer_.buffer,
                                  mesh.GetEdgeIndexBuffer().data(), edgeSize)) {
            if (edgeIndexBuffer_.IsValid()) allocator_->DestroyBuffer(edgeIndexBuffer_);
            edgeIndexBuffer_ = {};
            // Geometry still renders; wireframe pass simply has no edges.
            edgeCount_ = 0;
        }
    }

    ++revision_;

    fprintf(stderr, "[SurfaceGPU] Upload complete: vertices=%u indices=%u edges=%u vertexBuf=%s indexBuf=%s\n",
            vertexCount_, indexCount_, edgeCount_,
            vertexBuffer_.IsValid() ? "OK" : "NULL",
            indexBuffer_.IsValid() ? "OK" : "NULL");
    fprintf(stderr, "[SurfaceGPU] GPU memory: vertex=%.1fKB index=%.1fKB edge=%.1fKB total=%.1fKB\n",
            vertexBuffer_.size / 1024.0, indexBuffer_.size / 1024.0,
            edgeIndexBuffer_.IsValid() ? edgeIndexBuffer_.size / 1024.0 : 0.0,
            (vertexBuffer_.size + indexBuffer_.size + (edgeIndexBuffer_.IsValid() ? edgeIndexBuffer_.size : 0)) / 1024.0);
    fflush(stderr);

    return true;
}

void SurfaceGPUBuffer::Clear() {
    if (allocator_) {
        if (vertexBuffer_.IsValid()) allocator_->DestroyBuffer(vertexBuffer_);
        if (indexBuffer_.IsValid()) allocator_->DestroyBuffer(indexBuffer_);
        if (edgeIndexBuffer_.IsValid()) allocator_->DestroyBuffer(edgeIndexBuffer_);
    }
    vertexBuffer_ = {};
    indexBuffer_ = {};
    edgeIndexBuffer_ = {};
    vertexCount_ = 0;
    indexCount_ = 0;
    edgeCount_ = 0;
}

void SurfaceGPUBuffer::DrawIndexed(VkCommandBuffer cmd) const {
    if (!HasGeometry()) return;
    VkBuffer vbs[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

void SurfaceGPUBuffer::DrawIndexedEdges(VkCommandBuffer cmd) const {
    if (!HasEdges()) return;
    VkBuffer vbs[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, edgeIndexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, edgeCount_, 1, 0, 0, 0);
}

} // namespace surface
} // namespace workstation
