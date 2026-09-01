#include "workstation/gpu/PreparedGeometry.h"
#include "workstation/vulkan/VulkanAllocator.h"

namespace workstation {
namespace gpu {

PreparedGeometry::~PreparedGeometry() { Shutdown(); }

bool PreparedGeometry::Initialize(vulkan::VulkanAllocator& allocator, uint64_t maxPoints) {
    allocator_ = &allocator;
    pointCount_ = 0;
    return true;
}

void PreparedGeometry::Shutdown() {
    DestroyAttribute(position_);
    DestroyAttribute(color_);
    DestroyAttribute(intensity_);
    DestroyAttribute(classification_);
    DestroyAttribute(normal_);
    pointCount_ = 0;
}

GeometryAttribute PreparedGeometry::CreateAttribute(uint32_t elementSize,
                                                      uint32_t elementCount,
                                                      const void* data) {
    GeometryAttribute attr{};
    if (!allocator_ || elementCount == 0) return attr;

    VkDeviceSize size = static_cast<VkDeviceSize>(elementSize) * elementCount;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vmaCreateBuffer(allocator_->GetAllocator(), &bufferInfo, &allocInfo,
                    &attr.buffer, &attr.allocation, &attr.allocationInfo);
    attr.size = size;
    attr.elementSize = elementSize;
    attr.elementCount = elementCount;

    if (attr.allocationInfo.pMappedData) {
        attr.isMapped = true;
    } else {
        void* mapped = nullptr;
        VkResult result = vmaMapMemory(allocator_->GetAllocator(), attr.allocation, &mapped);
        if (result == VK_SUCCESS && mapped) {
            attr.allocationInfo.pMappedData = mapped;
            attr.isMapped = true;
        } else {
            attr.isMapped = false;
        }
    }

    if (data && attr.isMapped) {
        memcpy(attr.allocationInfo.pMappedData, data, size);
        vmaFlushAllocation(allocator_->GetAllocator(), attr.allocation, 0, size);
    }

    return attr;
}

void PreparedGeometry::DestroyAttribute(GeometryAttribute& attr) {
    if (attr.IsValid() && allocator_) {
        vmaDestroyBuffer(allocator_->GetAllocator(), attr.buffer, attr.allocation);
    }
    attr = {};
}

bool PreparedGeometry::UploadAttributeData(GeometryAttribute& attr, const void* data,
                                            uint32_t count, uint32_t elementSize) {
    if (!allocator_ || !data || count == 0) return false;

    if (attr.IsValid() && attr.elementCount >= count) {
        if (attr.isMapped) {
            memcpy(attr.allocationInfo.pMappedData, data,
                   static_cast<size_t>(elementSize) * count);
            vmaFlushAllocation(allocator_->GetAllocator(), attr.allocation, 0, attr.size);
            return true;
        }
    }

    DestroyAttribute(attr);
    attr = CreateAttribute(elementSize, count, data);
    return attr.IsValid();
}

bool PreparedGeometry::UploadPosition(const float* data, uint32_t count) {
    return UploadAttributeData(position_, data, count, sizeof(float) * 3);
}

bool PreparedGeometry::UploadColor(const float* data, uint32_t count) {
    return UploadAttributeData(color_, data, count, sizeof(float) * 3);
}

bool PreparedGeometry::UploadIntensity(const float* data, uint32_t count) {
    return UploadAttributeData(intensity_, data, count, sizeof(float));
}

bool PreparedGeometry::UploadClassification(const float* data, uint32_t count) {
    return UploadAttributeData(classification_, data, count, sizeof(float));
}

bool PreparedGeometry::UploadNormal(const float* data, uint32_t count) {
    return UploadAttributeData(normal_, data, count, sizeof(float) * 3);
}

void PreparedGeometry::BindPosition(VkCommandBuffer cmd, uint32_t binding) const {
    if (position_.IsValid()) {
        VkBuffer bufs[] = {position_.buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, binding, 1, bufs, offs);
    }
}

void PreparedGeometry::BindColor(VkCommandBuffer cmd, uint32_t binding) const {
    if (color_.IsValid()) {
        VkBuffer bufs[] = {color_.buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, binding, 1, bufs, offs);
    }
}

void PreparedGeometry::BindIntensity(VkCommandBuffer cmd, uint32_t binding) const {
    if (intensity_.IsValid()) {
        VkBuffer bufs[] = {intensity_.buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, binding, 1, bufs, offs);
    }
}

void PreparedGeometry::BindClassification(VkCommandBuffer cmd, uint32_t binding) const {
    if (classification_.IsValid()) {
        VkBuffer bufs[] = {classification_.buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, binding, 1, bufs, offs);
    }
}

void PreparedGeometry::BindNormal(VkCommandBuffer cmd, uint32_t binding) const {
    if (normal_.IsValid()) {
        VkBuffer bufs[] = {normal_.buffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(cmd, binding, 1, bufs, offs);
    }
}

void PreparedGeometry::Draw(VkCommandBuffer cmd, uint32_t count) const {
    uint32_t drawCount = count ? count : pointCount_;
    if (drawCount > 0) {
        vkCmdDraw(cmd, drawCount, 1, 0, 0);
    }
}

uint64_t PreparedGeometry::GetGPUMemoryBytes() const {
    return position_.size + color_.size + intensity_.size +
           classification_.size + normal_.size;
}

} // namespace gpu
} // namespace workstation
