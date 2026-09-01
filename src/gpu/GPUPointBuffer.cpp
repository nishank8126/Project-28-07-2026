#include "workstation/gpu/GPUPointBuffer.h"

#include <algorithm>
#include <vector>

namespace workstation {
namespace gpu {

GPUPointBuffer::~GPUPointBuffer() { Shutdown(); }

bool GPUPointBuffer::Initialize(GPUBufferManager& bufferManager, uint64_t maxPoints) {
    bufferManager_ = &bufferManager;
    maxPoints_ = maxPoints;

    positionBuffer_ = bufferManager.Allocate(BufferType::PointPosition,
                                              maxPoints * sizeof(float) * 3);
    colorBuffer_ = bufferManager.Allocate(BufferType::PointColor,
                                            maxPoints * sizeof(float) * 3);
    intensityBuffer_ = bufferManager.Allocate(BufferType::PointIntensity,
                                                maxPoints * sizeof(float));
    classificationBuffer_ = bufferManager.Allocate(BufferType::PointClassification,
                                                     maxPoints * sizeof(float));
    normalBuffer_ = bufferManager.Allocate(BufferType::PointNormal,
                                             maxPoints * sizeof(float) * 3);

    return IsValid();
}

void GPUPointBuffer::Shutdown() {
    if (bufferManager_) {
        if (positionBuffer_) bufferManager_->Free(positionBuffer_);
        if (colorBuffer_) bufferManager_->Free(colorBuffer_);
        if (intensityBuffer_) bufferManager_->Free(intensityBuffer_);
        if (classificationBuffer_) bufferManager_->Free(classificationBuffer_);
        if (normalBuffer_) bufferManager_->Free(normalBuffer_);
    }
    positionBuffer_ = nullptr;
    colorBuffer_ = nullptr;
    intensityBuffer_ = nullptr;
    classificationBuffer_ = nullptr;
    normalBuffer_ = nullptr;
    pointCount_ = 0;
}

bool GPUPointBuffer::UploadFromGeometry(const PreparedGeometry& geometry) {
    if (!bufferManager_ || !geometry.Position().IsValid()) return false;

    auto uploadAttr = [&](GPUBufferAllocation* dst, const GeometryAttribute& src) {
        if (!dst || !src.IsValid()) return;
        VkDeviceSize size = src.elementCount * src.elementSize;
        if (src.isMapped && dst->info.pMappedData) {
            memcpy(dst->info.pMappedData, src.allocationInfo.pMappedData, size);
            vmaFlushAllocation(bufferManager_->GetAllocator(), dst->allocation, 0, size);
        }
    };

    uploadAttr(positionBuffer_, geometry.Position());
    uploadAttr(colorBuffer_, geometry.Color());
    uploadAttr(intensityBuffer_, geometry.Intensity());
    uploadAttr(classificationBuffer_, geometry.Classification());
    uploadAttr(normalBuffer_, geometry.Normal());

    pointCount_ = geometry.GetPointCount();
    return true;
}

bool GPUPointBuffer::UploadPoints(const PointVertex* vertices, uint64_t count) {
    if (!bufferManager_ || !vertices || count == 0) return false;
    count = std::min(count, maxPoints_);

    std::vector<float> positions(count * 3);
    std::vector<float> colors(count * 3);
    std::vector<float> intensities(count);
    std::vector<float> classifications(count);
    std::vector<float> normals(count * 3);

    for (uint64_t i = 0; i < count; ++i) {
        const PointVertex& v = vertices[i];
        positions[i * 3 + 0] = v.position[0];
        positions[i * 3 + 1] = v.position[1];
        positions[i * 3 + 2] = v.position[2];
        colors[i * 3 + 0] = v.color[0];
        colors[i * 3 + 1] = v.color[1];
        colors[i * 3 + 2] = v.color[2];
        intensities[i] = v.intensity;
        classifications[i] = v.classification;
        normals[i * 3 + 0] = v.normal[0];
        normals[i * 3 + 1] = v.normal[1];
        normals[i * 3 + 2] = v.normal[2];
    }

    if (positionBuffer_)
        bufferManager_->UploadToGPU(positionBuffer_, positions.data(), positions.size() * sizeof(float));
    if (colorBuffer_)
        bufferManager_->UploadToGPU(colorBuffer_, colors.data(), colors.size() * sizeof(float));
    if (intensityBuffer_)
        bufferManager_->UploadToGPU(intensityBuffer_, intensities.data(), intensities.size() * sizeof(float));
    if (classificationBuffer_)
        bufferManager_->UploadToGPU(classificationBuffer_, classifications.data(), classifications.size() * sizeof(float));
    if (normalBuffer_)
        bufferManager_->UploadToGPU(normalBuffer_, normals.data(), normals.size() * sizeof(float));

    pointCount_ = count;
    return true;
}

void GPUPointBuffer::Bind(VkCommandBuffer cmd) const {
    if (positionBuffer_ && positionBuffer_->buffer != VK_NULL_HANDLE) {
        VkBuffer bufs[] = {positionBuffer_->buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, bufs, offs);
    }
    if (colorBuffer_ && colorBuffer_->buffer != VK_NULL_HANDLE) {
        VkBuffer bufs[] = {colorBuffer_->buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, 1, 1, bufs, offs);
    }
    if (intensityBuffer_ && intensityBuffer_->buffer != VK_NULL_HANDLE) {
        VkBuffer bufs[] = {intensityBuffer_->buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, 2, 1, bufs, offs);
    }
    if (classificationBuffer_ && classificationBuffer_->buffer != VK_NULL_HANDLE) {
        VkBuffer bufs[] = {classificationBuffer_->buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, 3, 1, bufs, offs);
    }
    if (normalBuffer_ && normalBuffer_->buffer != VK_NULL_HANDLE) {
        VkBuffer bufs[] = {normalBuffer_->buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, 4, 1, bufs, offs);
    }
}

void GPUPointBuffer::Draw(VkCommandBuffer cmd, uint32_t count, uint32_t offset) const {
    uint32_t drawCount = count ? count : static_cast<uint32_t>(pointCount_);
    if (drawCount > 0) {
        vkCmdDraw(cmd, drawCount, 1, offset, 0);
    }
}

void GPUPointBuffer::DrawIndirect(VkCommandBuffer cmd, VkBuffer indirectBuffer,
                                    uint32_t indirectOffset, uint32_t drawCount) const {
    vkCmdDrawIndirect(cmd, indirectBuffer, indirectOffset, drawCount,
                      sizeof(VkDrawIndirectCommand));
}

} // namespace gpu
} // namespace workstation
