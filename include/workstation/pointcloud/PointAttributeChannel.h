#pragma once
#include "workstation/pointcloud/PointChannelManager.h"
#include <vector>
#include <cstddef>
#include <cstdint>

namespace workstation { namespace pointcloud {

// Owned, decoded point-attribute channel. Independent of the Piece 1 query
// layer's PointChannel descriptor. Holds its own storage buffer plus the
// per-axis scale/offset transform. Created via CreateChannel (FUN_18006e2f0).
class PointAttributeChannel {
public:
    PointAttributeChannel() = default;

    // Allocate and copy storage for `count` elements of the given format.
    // scale/offset default to the identity transform when omitted.
    bool Create(ChannelId id, PointFormat format, size_t count,
                const void* data,
                const double scale[3] = nullptr,
                const double offset[3] = nullptr,
                size_t stride = 0);

    ChannelId Id() const { return id_; }
    PointFormat Format() const { return format_; }
    size_t Count() const { return count_; }
    size_t ElementSize() const { return elementSize_; }
    size_t Stride() const { return stride_; }
    size_t SizeBytes() const { return count_ * stride_; }
    const double* Scale() const { return scale_; }
    const double* Offset() const { return offset_; }
    const uint8_t* Data() const { return data_.empty() ? nullptr : data_.data(); }
    uint8_t* MutableData() { return data_.empty() ? nullptr : data_.data(); }

    // Decode one XYZ element (applies scale/offset). Valid for the XYZ channel.
    bool ReadXYZ(size_t index, double out[3]) const;
    // Decode one RGB element as uint8 (0-255). Valid for the RGB channel.
    bool ReadRGB(size_t index, uint8_t out[3]) const;
    // Decode one RGB element as float (0.0-1.0). Valid for the RGB channel
    // stored in Float32 format (as produced by LasFileReader).
    bool ReadRGBFloat(size_t index, float out[3]) const;

private:
    ChannelId id_ = ChannelId::XYZ;
    PointFormat format_ = PointFormat::Float32;
    size_t count_ = 0;
    size_t elementSize_ = 0;
    size_t stride_ = 0;
    double scale_[3] = {1.0, 1.0, 1.0};
    double offset_[3] = {0.0, 0.0, 0.0};
    std::vector<uint8_t> data_;
};

// CreateChannel (FUN_18006e2f0): build an owned channel, allocating and copying
// the source buffer. Returns the channel by value.
PointAttributeChannel CreateChannel(ChannelId id, PointFormat format, size_t count,
                                     const void* data,
                                     const double scale[3] = nullptr,
                                     const double offset[3] = nullptr,
                                     size_t stride = 0);

} // namespace pointcloud
} // namespace workstation
