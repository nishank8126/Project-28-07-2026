#pragma once
#include "workstation/gpu/PreparedGeometry.h"
#include "workstation/gpu/GPUPointBuffer.h"
#include "workstation/renderer/RenderCommand.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/spatial/BoundingBox.h"

#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace workstation {
namespace renderer {

struct GPUPoint {
    float position[3];
    uint32_t packedColor;
    float intensity;
    uint32_t classification;
    float normal[3];
    float padding;

    static uint32_t GetStride() { return sizeof(GPUPoint); }

    static uint32_t PackColor(float r, float g, float b) {
        uint8_t ri = static_cast<uint8_t>(r * 255.0f);
        uint8_t gi = static_cast<uint8_t>(g * 255.0f);
        uint8_t bi = static_cast<uint8_t>(b * 255.0f);
        return static_cast<uint32_t>(ri) | (static_cast<uint32_t>(gi) << 8) |
               (static_cast<uint32_t>(bi) << 16) | (0xFFu << 24);
    }
};

class PointCloudRenderAdapter {
public:
    bool Initialize();

    gpu::PreparedGeometry* PreparePointCloud(pointcloud::PointCloud& cloud);
    gpu::PreparedGeometry* PrepareNode(uint64_t nodeKey,
                                         const pointcloud::PointCloudNode* node);
    gpu::PreparedGeometry* GetPreparedGeometry(uint64_t nodeKey);
    const std::unordered_map<uint64_t, std::unique_ptr<gpu::PreparedGeometry>>&
        GetAllPreparedGeometries() const { return preparedGeometries_; }

    RenderCommand CreateRenderCommand(
        uint64_t nodeKey,
        gpu::PreparedGeometry* geometry,
        VkPipeline pipeline,
        VkPipelineLayout pipelineLayout,
        VkDescriptorSet descriptorSet);

    void CreateRenderCommandsForVisibleNodes(
        const std::vector<uint64_t>& visibleNodeKeys,
        VkPipeline pipeline,
        VkPipelineLayout pipelineLayout,
        VkDescriptorSet descriptorSet,
        std::vector<RenderCommand>& outCommands);

    void ReleaseAll();

    uint64_t GetTotalPreparedPoints() const { return totalPreparedPoints_; }
    uint32_t GetPreparedNodeCount() const { return static_cast<uint32_t>(preparedGeometries_.size()); }

private:
    std::unordered_map<uint64_t, std::unique_ptr<gpu::PreparedGeometry>> preparedGeometries_;
    uint64_t totalPreparedPoints_ = 0;

    void ExtractPointCloudData(
        gpu::PreparedGeometry& geo,
        const pointcloud::PointCloudNode* node);
};

} // namespace renderer
} // namespace workstation
