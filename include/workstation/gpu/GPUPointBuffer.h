#pragma once
#include "workstation/vulkan/VulkanHeaders.h"
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/gpu/GPUBufferManager.h"
#include "workstation/gpu/PreparedGeometry.h"

#include <cstdint>
#include <memory>

namespace workstation {
namespace gpu {

class GPUPointBuffer {
public:
    GPUPointBuffer() = default;
    ~GPUPointBuffer();

    GPUPointBuffer(const GPUPointBuffer&) = delete;
    GPUPointBuffer& operator=(const GPUPointBuffer&) = delete;

    bool Initialize(GPUBufferManager& bufferManager, uint64_t maxPoints);
    void Shutdown();

    bool UploadFromGeometry(const PreparedGeometry& geometry);
    void Bind(VkCommandBuffer cmd) const;
    void Draw(VkCommandBuffer cmd, uint32_t count = 0, uint32_t offset = 0) const;
    void DrawIndirect(VkCommandBuffer cmd, VkBuffer indirectBuffer,
                       uint32_t indirectOffset, uint32_t drawCount) const;

    uint64_t GetMaxPoints() const { return maxPoints_; }
    uint64_t GetPointCount() const { return pointCount_; }
    VkBuffer GetPositionBuffer() const { return positionBuffer_ ? positionBuffer_->buffer : VK_NULL_HANDLE; }
    VkBuffer GetColorBuffer() const { return colorBuffer_ ? colorBuffer_->buffer : VK_NULL_HANDLE; }
    VkBuffer GetIntensityBuffer() const { return intensityBuffer_ ? intensityBuffer_->buffer : VK_NULL_HANDLE; }
    VkBuffer GetClassificationBuffer() const { return classificationBuffer_ ? classificationBuffer_->buffer : VK_NULL_HANDLE; }
    VkBuffer GetNormalBuffer() const { return normalBuffer_ ? normalBuffer_->buffer : VK_NULL_HANDLE; }

    bool IsValid() const { return positionBuffer_ != nullptr; }

    static uint32_t GetVertexAttributeCount() { return 5; }

private:
    GPUBufferManager* bufferManager_ = nullptr;
    uint64_t maxPoints_ = 0;
    uint64_t pointCount_ = 0;

    GPUBufferAllocation* positionBuffer_ = nullptr;
    GPUBufferAllocation* colorBuffer_ = nullptr;
    GPUBufferAllocation* intensityBuffer_ = nullptr;
    GPUBufferAllocation* classificationBuffer_ = nullptr;
    GPUBufferAllocation* normalBuffer_ = nullptr;
};

} // namespace gpu
} // namespace workstation
