#include "workstation/renderer/PointCloudRenderAdapter.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointChannelManager.h"
#include "workstation/pointcloud/VoxelNode.h"
#include "workstation/surface/SurfaceLog.h"

#include <cstdio>
#include <functional>

namespace workstation {
namespace renderer {

bool PointCloudRenderAdapter::Initialize() {
    return true;
}

gpu::PreparedGeometry* PointCloudRenderAdapter::PreparePointCloud(
    pointcloud::PointCloud& cloud) {
    auto* root = cloud.Root();
    if (!root) return nullptr;

    // Recursively prepare ONLY LEAF nodes — matching BuildSpatialTreeFromCloud.
    // Internal VoxelNodes have 0 point data and waste GPU memory / budget.
    // Keys must match BuildSpatialTreeFromCloud() sequential leaf-only scheme.
    uint32_t preparedCount = 0;
    uint64_t preparedPoints = 0;
    uint64_t nextKey = 0;

    std::function<void(pointcloud::PointCloudNode*)> walk =
        [&](pointcloud::PointCloudNode* n) {
        if (!n) return;

        uint64_t key = nextKey++;

        if (!n->IsVoxel()) {
            // Leaf node: prepare for GPU rendering
            auto* geo = PrepareNode(key, n);
            if (geo) {
                preparedCount++;
                preparedPoints += n->PointCount();
            }
        }
        // VoxelNode: key is consumed but no geometry prepared (0 points)

        if (auto* v = dynamic_cast<pointcloud::VoxelNode*>(n)) {
            for (size_t i = 0; i < v->ChildCount(); ++i) {
                walk(v->Child(i));
            }
        }
    };

    walk(root);

    fprintf(stderr,
        "\n[OCTREE DEBUG]\n"
        "  Stage:          PreparePointCloud\n"
        "  Prepared nodes: %u (leaves only)\n"
        "  Prepared points:%llu\n"
        "  Root points:    %llu\n",
        preparedCount, preparedPoints, root->PointCount());
    fflush(stderr);

    return GetPreparedGeometry(0); // Return root geometry (may be null for VoxelNode root)
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
    float rgbFloat[3] = {};

    for (size_t i = 0; i < count; ++i) {
        if (channels.ReadXYZ(i, xyz)) {
            positions[i * 3 + 0] = static_cast<float>(xyz[0]);
            positions[i * 3 + 1] = static_cast<float>(xyz[1]);
            positions[i * 3 + 2] = static_cast<float>(xyz[2]);
        }
        // Try float RGB first (LasFileReader stores as float32),
        // then fall back to uint8 RGB.
        if (channels.ReadRGBFloat(i, rgbFloat)) {
            colors[i * 3 + 0] = rgbFloat[0];
            colors[i * 3 + 1] = rgbFloat[1];
            colors[i * 3 + 2] = rgbFloat[2];
        } else {
            uint8_t rgb8[3] = {};
            if (channels.ReadRGB(i, rgb8)) {
                colors[i * 3 + 0] = rgb8[0] / 255.0f;
                colors[i * 3 + 1] = rgb8[1] / 255.0f;
                colors[i * 3 + 2] = rgb8[2] / 255.0f;
            }
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
                normals[i * 3 + 2] = 1.0f;
            }
        } else {
            normals[i * 3 + 2] = 1.0f;
        }
    }

    // Diagnostic: log classification distribution once
    {
        static bool histDone = false;
        if (!histDone) {
            int hist[256] = {};
            for (size_t k = 0; k < count; ++k) hist[std::clamp((int)classifications[k],0,255)]++;
            fprintf(stderr, "[Adapter] Classification histogram (%zu pts): ", count);
            for (int c = 0; c < 56; ++c) if (hist[c]) fprintf(stderr, "%d:%d ", c, hist[c]);
            fprintf(stderr, "\n"); fflush(stderr);
            SLOG_INFO("Classification histogram (%zu pts):", count);
            for (int c = 0; c < 56; ++c) if (hist[c]) SLOG_INFO("  class %d: %d pts", c, hist[c]);
            histDone = true;
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
