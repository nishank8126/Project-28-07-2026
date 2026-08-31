#pragma once
#include "workstation/pod/PodBinaryReader.h"
#include "workstation/pod/PodRecords.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/VoxelNode.h"
#include <cstdint>
#include <memory>

namespace workstation { namespace pod {

// Node decoder (FUN_1800764c0 normal / FUN_180081100 voxel). Reads the
// node record from a PodBinaryReader and produces the appropriate node type.
class PodNodeDecoder {
public:
    // Decode a single node (type flag + geometry + metadata).
    // Returns nullptr on error.
    static std::unique_ptr<pointcloud::PointCloudNode> DecodeNode(
        PodBinaryReader& reader, NodeType type, GeometryPath geomPath,
        std::uint64_t& sourceNodeValue, NodeExtraMetadata& extraMeta);
};

} // namespace pod
} // namespace workstation
