#pragma once
#include "workstation/pointcloud/PointChannelManager.h"
#include <cstdint>
#include <vector>

namespace workstation { namespace pointcloud {

// Abstraction of a channel's payload descriptor (FUN_180081480).
// Carries the metadata needed to build a PointAttributeChannel plus the
// raw payload bytes (owned here for the duration of parsing).
struct ChannelDescriptor {
    ChannelId id = ChannelId::XYZ;
    PointFormat format = PointFormat::Float32;
    uint32_t count = 0;
    uint32_t stride = 0;
    double scale[3] = {1.0, 1.0, 1.0};
    double offset[3] = {0.0, 0.0, 0.0};
    std::vector<uint8_t> data;

    const uint8_t* Data() const { return data.empty() ? nullptr : data.data(); }
};

} // namespace pointcloud
} // namespace workstation
