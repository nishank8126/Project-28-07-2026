#pragma once
#include "workstation/pod/PodHandlerRegistry.h"
#include "workstation/pod/PodDecodeContext.h"
#include "workstation/pointcloud/BoundingBox.h"
#include <cstdint>
#include <vector>

namespace workstation { namespace pod {

// Record produced by the double-bounding handler: a key + bounding box + metadata.
struct BoundingBoxRecord {
    std::uint32_t key = 0;
    pointcloud::BoundingBox bounds{};
    std::uint64_t metadata[6] = {};
};

// Concrete handler for double-precision bounding-box records (FUN_1800780C0).
// Reads handler/index records and produces bounding-box entries.
// Clean-room implementation; no proprietary class name reproduced.
class DoubleBoundingBlockHandler : public PodBlockHandler {
public:
    // Process the handler on the given decode context.
    // Reads marker type 1, record count, and per-record bounding data.
    bool process(PodDecodeContext& context) override;

    // Access the collected records after processing.
    const std::vector<BoundingBoxRecord>& records() const { return records_; }

private:
    std::vector<BoundingBoxRecord> records_;
};

}} // namespace workstation::pod
