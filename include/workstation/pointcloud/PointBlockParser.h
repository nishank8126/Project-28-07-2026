#pragma once
#include "workstation/pointcloud/PointCloudStreamReader.h"
#include "workstation/pointcloud/NodeDescriptor.h"
#include "workstation/pointcloud/ChannelDescriptor.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include <memory>
#include <vector>

namespace workstation { namespace pointcloud {

// Block parser (FUN_180078840 / FUN_180081480). Reads node descriptors and
// channel descriptors from a PointCloudStreamReader and builds the node
// hierarchy + PointAttributeChannel objects. No proprietary binary format.
class PointBlockParser {
public:
    // Parse one node (descriptor + channels) and return the created node.
    static std::unique_ptr<PointCloudNode> ParseNode(PointCloudStreamReader& r, bool& ok);

    static bool ParseNodeDescriptor(PointCloudStreamReader& r, NodeDescriptor& d);
    static bool ParseChannelDescriptor(PointCloudStreamReader& r, ChannelDescriptor& c);

private:
    static size_t ChannelByteSize(ChannelId id, PointFormat fmt, size_t count, size_t stride);
};

} // namespace pointcloud
} // namespace workstation
