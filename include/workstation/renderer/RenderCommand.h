#pragma once
#include "workstation/gpu/PreparedGeometry.h"
#include "workstation/spatial/BoundingBox.h"
#include "workstation/math/Matrix4d.h"

#include <cstdint>
#include <functional>

namespace workstation {
namespace renderer {

struct RenderCommand {
    uint64_t nodeKey = 0;
    gpu::PreparedGeometry* geometry = nullptr;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    spatial::BoundingBox bounds = {};
    math::Matrix4d transform = {};

    uint32_t pointCount = 0;
    float distanceToCamera = 0.0f;
    float screenSpaceError = 0.0f;

    uint64_t geometryRevision = 0;
    uint64_t lastUsedFrame = 0;
    bool isVisible = true;
    bool isPersistent = true;

    struct SortKey {
        uint64_t pipelineId = 0;
        uint64_t bufferId = 0;
        uint32_t materialId = 0;

        bool operator<(const SortKey& o) const {
            if (pipelineId != o.pipelineId) return pipelineId < o.pipelineId;
            if (bufferId != o.bufferId) return bufferId < o.bufferId;
            return materialId < o.materialId;
        }
    };

    SortKey GetSortKey() const {
        return {
            reinterpret_cast<uint64_t>(pipeline),
            reinterpret_cast<uint64_t>(geometry ? geometry->Position().buffer : nullptr),
            0
        };
    }

    bool NeedsUpdate() const {
        return geometry && static_cast<uint64_t>(geometry->GetRevision()) != geometryRevision;
    }
};

} // namespace renderer
} // namespace workstation
