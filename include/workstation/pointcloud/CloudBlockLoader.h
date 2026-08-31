#pragma once
#include "workstation/pointcloud/PointCloudStreamReader.h"
#include "workstation/pointcloud/PointCloud.h"
#include <memory>

namespace workstation { namespace pointcloud {

// Cloud block loading pipeline (FUN_180078840). Drives the buffered reader and
// block parser to populate a PointCloud, then finalizes it (FUN_18007d990).
class CloudBlockLoader {
public:
    explicit CloudBlockLoader(PointBlockSource& source) : source_(source) {}

    // LoadCloudBlocks(): validate -> reader -> block count -> index table ->
    // per-block parse + attach -> finalize.
    bool Load(PointCloud& cloud);

private:
    PointBlockSource& source_;
    std::unique_ptr<PointCloudNode> root_; // owns the synthesized root + children
};

} // namespace pointcloud
} // namespace workstation
