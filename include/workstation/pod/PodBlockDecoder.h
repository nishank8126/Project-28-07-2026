#pragma once
#include "workstation/pod/PodBinaryReader.h"
#include "workstation/pod/PodHandlerRegistry.h"
#include "workstation/pointcloud/PointCloud.h"
#include <cstdint>
#include <memory>

namespace workstation { namespace pod {

// Post-load metadata stage interfaces (FUN_1800774c0, FUN_180078620,
// FUN_180077c80). Exact semantics not yet reversed; these are stubs.
class PodPostLoadHandler {
public:
    virtual ~PodPostLoadHandler() = default;
    virtual void onPostLoad(pointcloud::PointCloud& cloud) { (void)cloud; }
};

// Main POD block decoder (FUN_180078840). Replaces the Piece 3 generic
// pipeline with evidence-backed POD decoding. Reuses the existing
// PointCloud / PointCloudNode / VoxelNode / PointAttributeChannel types.
class PodBlockDecoder {
public:
    PodBlockDecoder() = default;

    // Decode a POD stream into the given cloud. Returns false on error.
    bool decode(PodDataSource& source, pointcloud::PointCloud& cloud);

    PodHandlerRegistry& handlers() { return handlers_; }

private:
    bool decodeChannels(PodBinaryReader& reader, pointcloud::PointCloudNode& node);
    bool processHandlerTable(PodBinaryReader& reader, pointcloud::PointCloud& cloud);

    PodHandlerRegistry handlers_;
    PodPostLoadHandler postLoad_;
    std::unique_ptr<pointcloud::PointCloudNode> root_;  // owns decoded nodes
};

} // namespace pod
} // namespace workstation
