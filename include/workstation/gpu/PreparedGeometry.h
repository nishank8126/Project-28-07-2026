#pragma once
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/spatial/BoundingBox.h"

#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace workstation {
namespace gpu {

struct GeometryAttribute {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo = {};
    VkDeviceSize size = 0;
    uint32_t elementSize = 0;
    uint32_t elementCount = 0;
    bool isMapped = false;

    bool IsValid() const { return buffer != VK_NULL_HANDLE; }
};

enum class GeometryRevision : uint64_t {};

class PreparedGeometry {
public:
    PreparedGeometry() = default;
    ~PreparedGeometry();

    PreparedGeometry(const PreparedGeometry&) = delete;
    PreparedGeometry& operator=(const PreparedGeometry&) = delete;

    bool Initialize(vulkan::VulkanAllocator& allocator, uint64_t maxPoints);
    void Shutdown();

    GeometryAttribute& Position() { return position_; }
    GeometryAttribute& Color() { return color_; }
    GeometryAttribute& Intensity() { return intensity_; }
    GeometryAttribute& Classification() { return classification_; }
    GeometryAttribute& Normal() { return normal_; }

    const GeometryAttribute& Position() const { return position_; }
    const GeometryAttribute& Color() const { return color_; }
    const GeometryAttribute& Intensity() const { return intensity_; }
    const GeometryAttribute& Classification() const { return classification_; }
    const GeometryAttribute& Normal() const { return normal_; }

    void SetBounds(const spatial::BoundingBox& b) { bounds_ = b; }
    const spatial::BoundingBox& GetBounds() const { return bounds_; }

    void SetPointCount(uint32_t count) { pointCount_ = count; }
    uint32_t GetPointCount() const { return pointCount_; }

    GeometryRevision GetRevision() const { return revision_; }
    void IncrementRevision() {
        revision_ = static_cast<GeometryRevision>(
            static_cast<uint64_t>(revision_) + 1);
    }

    bool IsDirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }
    void MarkDirty() { dirty_ = true; }

    uint64_t GetNodeKey() const { return nodeKey_; }
    void SetNodeKey(uint64_t key) { nodeKey_ = key; }

    uint64_t GetGPUMemoryBytes() const;

    bool UploadPosition(const float* data, uint32_t count);
    bool UploadColor(const float* data, uint32_t count);
    bool UploadIntensity(const float* data, uint32_t count);
    bool UploadClassification(const float* data, uint32_t count);
    bool UploadNormal(const float* data, uint32_t count);

    void BindPosition(VkCommandBuffer cmd, uint32_t binding = 0) const;
    void BindColor(VkCommandBuffer cmd, uint32_t binding = 1) const;
    void BindIntensity(VkCommandBuffer cmd, uint32_t binding = 2) const;
    void BindClassification(VkCommandBuffer cmd, uint32_t binding = 3) const;
    void BindNormal(VkCommandBuffer cmd, uint32_t binding = 4) const;

    void Draw(VkCommandBuffer cmd, uint32_t count = 0) const;

    static uint32_t GetPositionStride() { return sizeof(float) * 3; }
    static uint32_t GetColorStride() { return sizeof(float) * 3; }
    static uint32_t GetIntensityStride() { return sizeof(float); }
    static uint32_t GetClassificationStride() { return sizeof(float); }
    static uint32_t GetNormalStride() { return sizeof(float) * 3; }

private:
    vulkan::VulkanAllocator* allocator_ = nullptr;
    uint64_t nodeKey_ = 0;

    GeometryAttribute position_ = {};
    GeometryAttribute color_ = {};
    GeometryAttribute intensity_ = {};
    GeometryAttribute classification_ = {};
    GeometryAttribute normal_ = {};

    spatial::BoundingBox bounds_ = {};
    uint32_t pointCount_ = 0;
    GeometryRevision revision_ = {};
    bool dirty_ = false;

    GeometryAttribute CreateAttribute(uint32_t elementSize, uint32_t elementCount,
                                       const void* data = nullptr);
    void DestroyAttribute(GeometryAttribute& attr);
    bool UploadAttributeData(GeometryAttribute& attr, const void* data,
                              uint32_t count, uint32_t elementSize);
};

} // namespace gpu
} // namespace workstation
