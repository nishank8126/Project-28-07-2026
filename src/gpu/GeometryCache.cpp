#include "workstation/gpu/GeometryCache.h"
#include "workstation/pointcloud/PointAttributeChannel.h"

namespace workstation {
namespace gpu {

bool GeometryCache::Initialize(GPUBufferManager* bufferManager) {
    bufferManager_ = bufferManager;
    return true;
}

void GeometryCache::Shutdown() {
    geometries_.clear();
    bufferManager_ = nullptr;
}

PreparedGeometry* GeometryCache::PrepareNode(uint64_t nodeKey,
                                               const pointcloud::PointCloudNode* node) {
    if (!node) return nullptr;

    auto it = geometries_.find(nodeKey);
    if (it != geometries_.end()) {
        return it->second.get();
    }

    auto geo = std::make_unique<PreparedGeometry>();
    geo->Initialize(vulkan::VulkanAllocator::Get(), node->PointCount());
    geo->SetNodeKey(nodeKey);
    geo->SetBounds(node->bounds());
    geo->SetPointCount(static_cast<uint32_t>(node->PointCount()));

    ExtractNodeGeometry(*geo, node);

    PreparedGeometry* ptr = geo.get();
    geometries_[nodeKey] = std::move(geo);
    return ptr;
}

PreparedGeometry* GeometryCache::GetGeometry(uint64_t nodeKey) {
    auto it = geometries_.find(nodeKey);
    return it != geometries_.end() ? it->second.get() : nullptr;
}

void GeometryCache::ReleaseGeometry(uint64_t nodeKey) {
    geometries_.erase(nodeKey);
}

void GeometryCache::MarkDirty(uint64_t nodeKey) {
    auto* geo = GetGeometry(nodeKey);
    if (geo) geo->MarkDirty();
}

void GeometryCache::EvictUnused(uint64_t currentFrame, uint64_t maxAge) {
    (void)currentFrame; (void)maxAge;
}

uint64_t GeometryCache::GetTotalGPUMemory() const {
    uint64_t total = 0;
    for (auto& [key, geo] : geometries_) {
        total += geo->GetGPUMemoryBytes();
    }
    return total;
}

void GeometryCache::PrepareCloud(pointcloud::PointCloud& cloud) {
    auto* root = cloud.Root();
    if (!root) return;

    PrepareNode(0, root);
}

void GeometryCache::ExtractNodeGeometry(PreparedGeometry& geo,
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
            if (ch && ch->Data() && i < ch->Count()) {
                intensities[i] = reinterpret_cast<const float*>(ch->Data())[i];
            }
        }

        if (attrs.Has(pointcloud::PointAttribute::Classification)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Classification);
            if (ch && ch->Data() && i < ch->Count()) {
                classifications[i] = static_cast<float>(ch->Data()[i]);
            }
        }

        if (attrs.Has(pointcloud::PointAttribute::Normals)) {
            auto* ch = channels.GetChannel(pointcloud::ChannelId::Normals);
            if (ch && ch->Data() && i < ch->Count()) {
                const float* n = reinterpret_cast<const float*>(ch->Data()) + i * 3;
                normals[i * 3 + 0] = n[0];
                normals[i * 3 + 1] = n[1];
                normals[i * 3 + 2] = n[2];
            } else {
                normals[i * 3 + 0] = 0;
                normals[i * 3 + 1] = 1;
                normals[i * 3 + 2] = 0;
            }
        } else {
            normals[i * 3 + 0] = 0;
            normals[i * 3 + 1] = 1;
            normals[i * 3 + 2] = 0;
        }
    }

    geo.UploadPosition(positions.data(), static_cast<uint32_t>(count));
    geo.UploadColor(colors.data(), static_cast<uint32_t>(count));
    geo.UploadIntensity(intensities.data(), static_cast<uint32_t>(count));
    geo.UploadClassification(classifications.data(), static_cast<uint32_t>(count));
    geo.UploadNormal(normals.data(), static_cast<uint32_t>(count));
}

} // namespace gpu
} // namespace workstation
