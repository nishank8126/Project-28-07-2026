#include "workstation/pod/PodNodeDecoder.h"
#include "workstation/pod/PodRecords.h"
#include "workstation/pointcloud/PointCloud.h"

namespace workstation { namespace pod {

std::unique_ptr<pointcloud::PointCloudNode> PodNodeDecoder::DecodeNode(
    PodBinaryReader& reader, NodeType type, GeometryPath geomPath,
    std::uint64_t& sourceNodeValue, NodeExtraMetadata& extraMeta)
{
    // Read geometry (confirmed: float path = 24 bytes, double path = 48 bytes).
    GeometryPair geom;
    if (geomPath == GeometryPath::Float64) {
        for (int i = 0; i < 3; ++i) if (!reader.readDouble(geom.first[i]))  return nullptr;
        for (int i = 0; i < 3; ++i) if (!reader.readDouble(geom.second[i])) return nullptr;
    } else {
        float tmp[6];
        for (int i = 0; i < 6; ++i) if (!reader.readFloat(tmp[i])) return nullptr;
        for (int i = 0; i < 3; ++i) { geom.first[i]  = tmp[i];     geom.second[i]  = tmp[i + 3]; }
    }

    // Read flags.
    std::uint32_t flags = 0;
    if (!reader.readU32(flags)) return nullptr;

    // Read extra metadata (FUN_180082b40).
    std::uint32_t metaParam = 0;
    if (!reader.readU32(metaParam)) return nullptr;
    if (metaParam <= 0x0C) {
        std::size_t bytes = static_cast<std::size_t>(metaParam) * 4;
        extraMeta.rawSmallData.resize(bytes);
        if (bytes > 0 && !reader.read(extraMeta.rawSmallData.data(), bytes)) return nullptr;
    } else {
        for (int i = 0; i < 6; ++i) if (!reader.readU64(extraMeta.extendedData[i])) return nullptr;
    }
    std::uint32_t metaMode = 0;
    if (!reader.readU32(metaMode)) return nullptr;
    extraMeta.mode = metaMode;

    // Read source node value (8-byte identifier).
    if (!reader.readU64(sourceNodeValue)) return nullptr;

    // Create the appropriate node type.
    pointcloud::BoundingBox bb;
    bb.minX = geom.first[0];  bb.minY = geom.first[1];  bb.minZ = geom.first[2];
    bb.maxX = geom.second[0]; bb.maxY = geom.second[1]; bb.maxZ = geom.second[2];

    std::unique_ptr<pointcloud::PointCloudNode> node;
    if (type == NodeType::Hierarchical) {
        node = pointcloud::CreateVoxelNode(bb);
    } else {
        node = pointcloud::CreatePointNode(bb);
    }
    return node;
}

} // namespace pod
} // namespace workstation
