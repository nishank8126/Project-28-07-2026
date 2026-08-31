#include "workstation/pod/DoubleBoundingBlockHandler.h"

namespace workstation { namespace pod {

bool DoubleBoundingBlockHandler::process(PodDecodeContext& context) {
    auto& reader = context.reader();

    // Read marker type (confirmed: valid marker is 1).
    std::uint32_t marker = 0;
    if (!reader.readU32(marker)) return false;
    if (marker != 1) return false;

    // Read record count.
    std::uint32_t recordCount = 0;
    if (!reader.readU32(recordCount)) return false;

    records_.clear();
    records_.reserve(recordCount);

    for (std::uint32_t i = 0; i < recordCount; ++i) {
        BoundingBoxRecord rec;

        // Read 32-bit ordering key.
        if (!reader.readU32(rec.key)) return false;

        // Read double-precision bounding box (6 values).
        if (!reader.readDouble(rec.bounds.minX)) return false;
        if (!reader.readDouble(rec.bounds.minY)) return false;
        if (!reader.readDouble(rec.bounds.minZ)) return false;
        if (!reader.readDouble(rec.bounds.maxX)) return false;
        if (!reader.readDouble(rec.bounds.maxY)) return false;
        if (!reader.readDouble(rec.bounds.maxZ)) return false;

        // Read additional metadata (6 uint64 values, observed size 0x88 record).
        for (int j = 0; j < 6; ++j) {
            if (!reader.readU64(rec.metadata[j])) return false;
        }

        records_.push_back(std::move(rec));
    }

    return true;
}

}} // namespace workstation::pod
