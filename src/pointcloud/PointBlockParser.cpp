#include "workstation/pointcloud/PointBlockParser.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/VoxelNode.h"
#include <cstring>

namespace workstation { namespace pointcloud {

size_t PointBlockParser::ChannelByteSize(ChannelId id, PointFormat fmt, size_t count, size_t stride) {
    size_t comps = (id == ChannelId::Intensity || id == ChannelId::Classification) ? 1 : 3;
    size_t fmtSize = (fmt == PointFormat::Int16) ? 2
                    : (fmt == PointFormat::Float32) ? 4 : 1;
    size_t es = comps * fmtSize;
    size_t st = stride ? stride : es;
    return count * st;
}

bool PointBlockParser::ParseNodeDescriptor(PointCloudStreamReader& r, NodeDescriptor& d) {
    uint32_t type; if (!r.ReadU32(type)) return false; d.type = type;
    if (!r.ReadDouble(d.bounds.minX)) return false;
    if (!r.ReadDouble(d.bounds.minY)) return false;
    if (!r.ReadDouble(d.bounds.minZ)) return false;
    if (!r.ReadDouble(d.bounds.maxX)) return false;
    if (!r.ReadDouble(d.bounds.maxY)) return false;
    if (!r.ReadDouble(d.bounds.maxZ)) return false;
    if (!r.ReadU32(d.flags)) return false;
    return true;
}

bool PointBlockParser::ParseChannelDescriptor(PointCloudStreamReader& r, ChannelDescriptor& c) {
    uint32_t id, fmt, count, stride;
    if (!r.ReadU32(id))    return false;
    if (!r.ReadU32(fmt))    return false;
    if (!r.ReadU32(count))  return false;
    if (!r.ReadU32(stride)) return false;
    c.id = static_cast<ChannelId>(id);
    c.format = static_cast<PointFormat>(fmt);
    c.count = count;
    c.stride = stride;
    for (int i = 0; i < 3; ++i) if (!r.ReadDouble(c.scale[i]))  return false;
    for (int i = 0; i < 3; ++i) if (!r.ReadDouble(c.offset[i])) return false;
    size_t bytes = ChannelByteSize(c.id, c.format, c.count, c.stride);
    c.data.resize(bytes);
    if (bytes > 0 && !r.Read(c.data.data(), bytes)) return false;
    return true;
}

std::unique_ptr<PointCloudNode> PointBlockParser::ParseNode(PointCloudStreamReader& r, bool& ok) {
    ok = false;
    NodeDescriptor d;
    if (!ParseNodeDescriptor(r, d)) return nullptr;

    std::unique_ptr<PointCloudNode> node =
        (d.type == static_cast<uint32_t>(NodeDescriptor::Type::Voxel))
            ? static_cast<std::unique_ptr<PointCloudNode>>(CreateVoxelNode(d.bounds))
            : CreatePointNode(d.bounds);

    uint32_t chCount = 0;
    if (!r.ReadU32(chCount)) return nullptr;
    for (uint32_t i = 0; i < chCount; ++i) {
        ChannelDescriptor c;
        if (!ParseChannelDescriptor(r, c)) return nullptr;
        node->channels().AddChannel(
            CreateChannel(c.id, c.format, c.count, c.Data(), c.scale, c.offset, c.stride));
    }
    ok = true;
    return node;
}

} // namespace pointcloud
} // namespace workstation
