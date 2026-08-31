#include "workstation/pointcloud/CloudBlockLoader.h"
#include "workstation/pointcloud/PointBlockParser.h"
#include "workstation/pointcloud/VoxelNode.h"

namespace workstation { namespace pointcloud {

bool CloudBlockLoader::Load(PointCloud& cloud) {
    // STEP 1: validate cloud state (do not overwrite an already-populated cloud).
    if (cloud.Root() != nullptr) return false;

    // STEP 3: buffered stream reader.
    PointCloudStreamReader reader(source_);

    // STEP 4: block count.
    uint32_t blockCount = 0;
    if (!reader.ReadU32(blockCount)) return false;

    // STEP 5: node index table.
    for (uint32_t i = 0; i < blockCount; ++i) {
        uint32_t idx = 0;
        if (!reader.ReadU32(idx)) return false;
    }

    // STEP 6: for each block, parse and attach a node.
    auto root = CreateVoxelNode(BoundingBox{});
    for (uint32_t i = 0; i < blockCount; ++i) {
        bool ok = false;
        auto node = PointBlockParser::ParseNode(reader, ok);
        if (!ok || !node) return false;
        root->AddChild(std::move(node));
    }
    cloud.SetRoot(root.get());
    root_ = std::move(root);

    // STEP 7: finalize cloud (FUN_18007d990).
    cloud.Finalize();
    return true;
}

} // namespace pointcloud
} // namespace workstation
