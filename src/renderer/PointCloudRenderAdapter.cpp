#include "workstation/renderer/PointCloudRenderAdapter.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointChannelManager.h"

namespace workstation {
namespace renderer {

bool PointCloudRenderAdapter::Initialize() {
    return true;
}

gpu::PreparedGeometry* PointCloudRenderAdapter::PreparePointCloud(
    pointcloud::PointCloud& cloud) {
    auto* root = cloud.Root();
    if (!root) return nullptr;
    return PrepareNode(0, root);
}

gpu::PreparedGeometry* PointCloudRenderAdapter::PrepareNode(
    uint64_t nodeKey,
    const pointcloud::PointCloudNode* node) {
    if (!node) return nullptr;

    auto it = preparedGeometries_.find(nodeKey);
    if (it != preparedGeometries_.end()) {
        return it->second.get();
    }

    auto geo = std::make_unique<gpu::PreparedGeometry>();
    geo->Initialize(vulkan::VulkanAllocator::Get(), node->PointCount());
    geo->SetNodeKey(nodeKey);
    geo->SetBounds(node->bounds());
    geo->SetPointCount(static_cast<uint32_t>(node->PointCount()));

    ExtractPointCloudData(*geo, node);

    gpu::PreparedGeometry* ptr = geo.get();
    preparedGeometries_[nodeKey] = std::move(geo);
    totalPreparedPoints_ += node->PointCount();
    return ptr;
}

gpu::PreparedGeometry* PointCloudRenderAdapter::GetPreparedGeometry(uint64_t nodeKey) {
    auto it = preparedGeometries_.find(nodeKey);
    return it != preparedGeometries_.end() ? it->second.get() : nullptr;
}

RenderCommand PointCloudRenderAdapter::CreateRenderCommand(
    uint64_t nodeKey,
    gpu::PreparedGeometry* geometry,
    VkPipeline pipeline,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet) {
    RenderCommand cmd{};
    cmd.nodeKey = nodeKey;
    cmd.geometry = geometry;
    cmd.pipeline = pipeline;
    cmd.pipelineLayout = pipelineLayout;
    cmd.descriptorSet = descriptorSet;
    cmd.bounds = geometry ? geometry->GetBounds() : spatial::BoundingBox{};
    cmd.pointCount = geometry ? geometry->GetPointCount() : 0;
    cmd.geometryRevision = geometry ?
        static_cast<uint64_t>(geometry->GetRevision()) : 0;
    cmd.isVisible = true;
    cmd.isPersistent = true;
    return cmd;
}

void PointCloudRenderAdapter::CreateRenderCommandsForVisibleNodes(
    const std::vector<uint64_t>& visibleNodeKeys,
    VkPipeline pipeline,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    std::vector<RenderCommand>& outCommands) {
    outCommands.clear();
    outCommands.reserve(visibleNodeKeys.size());

    for (uint64_t key : visibleNodeKeys) {
        auto* geo = GetPreparedGeometry(key);
        if (!geo || geo->GetPointCount() == 0) continue;

        outCommands.push_back(
            CreateRenderCommand(key, geo, pipeline, pipelineLayout, descriptorSet));
    }
}

void PointCloudRenderAdapter::ReleaseAll() {
    preparedGeometries_.clear();
    totalPreparedPoints_ = 0;
}

void PointCloudRenderAdapter::ExtractPointCloudData(
    gpu::PreparedGeometry& geo,
    const pointcloud::PointCloudNode* node) {
    const auto& channels = node->channels();
    size_t count = channels.PointCount();
    if (count == 0) return;

    std::vector<float> positions(count * 3);
    std::vector<float> colors(count * 3, 0.5f);
    std::vector<float> intensities(count, 0.5f);
    std::vector<float> classifications(count, 0.0f);
    std::vector<float> normals(count * 3);

    double xyz[3] = {};
    uint8_t rgb[3] = {};

    for (size_t i = 0; i < count; ++i) {
        if (channels.ReadXYZ(i, xyz)) {
            positions[i * 3 + 0] = static_cast<float>(xyz[0]);
            positions[i * 3 + 1] = static_cast<float>(xyz[1]);
            positions[i * 3 + 2] = static_cast<float>(xyz[2]);
        }
        if (channels.ReadRGB(i, rgb)) {
            colors[i * 3 + 0] = rgb[0] / 255.0f;
            colors[i * 3 + 1] = rgb[1] / 255.0f;
            colors[i * 3 + 2] = rgb[2] / 255.0f;
        }
        auto attrs = channels.Attributes();
        if (attrs.Has(pointcloud::PointAttribute::Intensity)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Intensity);
            if (ch && ch->Data() && i < ch->Count())
                intensities[i] = reinterpret_cast<const float*>(ch->Data())[i];
        }
        if (attrs.Has(pointcloud::PointAttribute::Classification)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Classification);
            if (ch && ch->Data() && i < ch->Count())
                classifications[i] = static_cast<float>(ch->Data()[i]);
        }
        if (attrs.Has(pointcloud::PointAttribute::Normals)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Normals);
            if (ch && ch->Data() && i < ch->Count()) {
                const float* n = reinterpret_cast<const float*>(ch->Data()) + i * 3;
                normals[i * 3 + 0] = n[0];
                normals[i * 3 + 1] = n[1];
                normals[i * 3 + 2] = n[2];
            } else {
                normals[i * 3 + 1] = 1.0f;
            }
        } else {
            normals[i * 3 + 1] = 1.0f;
        }
    }

    geo.UploadPosition(positions.data(), static_cast<uint32_t>(count));
    geo.UploadColor(colors.data(), static_cast<uint32_t>(count));
    geo.UploadIntensity(intensities.data(), static_cast<uint32_t>(count));
    geo.UploadClassification(classifications.data(), static_cast<uint32_t>(count));
    geo.UploadNormal(normals.data(), static_cast<uint32_t>(count));
    geo.IncrementRevision();
}

} // namespace renderer
} // namespace workstation
