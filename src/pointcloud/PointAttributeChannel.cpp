#include "workstation/pointcloud/PointAttributeChannel.h"
#include <cstring>

namespace workstation { namespace pointcloud {

bool PointAttributeChannel::Create(ChannelId id, PointFormat format, size_t count,
                                   const void* data,
                                   const double scale[3], const double offset[3],
                                   size_t stride) {
    id_ = id;
    format_ = format;
    count_ = count;

    size_t comps = (id == ChannelId::Intensity || id == ChannelId::Classification) ? 1 : 3;
    size_t fmtSize = (format == PointFormat::Int16) ? 2
                    : (format == PointFormat::Float32) ? 4 : 1;
    elementSize_ = comps * fmtSize;
    stride_ = stride ? stride : elementSize_;

    if (scale) {
        scale_[0] = scale[0]; scale_[1] = scale[1]; scale_[2] = scale[2];
    } else {
        scale_[0] = 1.0; scale_[1] = 1.0; scale_[2] = 1.0;   // identity default
    }
    if (offset) {
        offset_[0] = offset[0]; offset_[1] = offset[1]; offset_[2] = offset[2];
    } else {
        offset_[0] = 0.0; offset_[1] = 0.0; offset_[2] = 0.0;
    }

    size_t bytes = count_ * stride_;
    data_.resize(bytes);
    if (data && bytes > 0) {
        // DESIGN DECISION: source is assumed tightly packed (count * elementSize).
        size_t copy = count_ * elementSize_;
        if (copy > bytes) copy = bytes;
        std::memcpy(data_.data(), data, copy);
    }
    return true;
}

bool PointAttributeChannel::ReadXYZ(size_t index, double out[3]) const {
    if (id_ != ChannelId::XYZ || index >= count_ || data_.empty()) return false;
    const uint8_t* base = data_.data() + index * stride_;
    if (format_ == PointFormat::Int16) {
        for (int i = 0; i < 3; ++i) {
            int16_t v = 0;
            std::memcpy(&v, base + i * 2, 2);
            out[i] = static_cast<double>(v) * scale_[i] + offset_[i];
        }
    } else if (format_ == PointFormat::Float32) {
        for (int i = 0; i < 3; ++i) {
            float v = 0;
            std::memcpy(&v, base + i * 4, 4);
            out[i] = static_cast<double>(v) * scale_[i] + offset_[i];
        }
    } else {
        return false;
    }
    return true;
}

bool PointAttributeChannel::ReadRGB(size_t index, uint8_t out[3]) const {
    if (id_ != ChannelId::RGB || index >= count_ || data_.empty()) return false;
    const uint8_t* base = data_.data() + index * stride_;
    out[0] = base[0]; out[1] = base[1]; out[2] = base[2];
    return true;
}

PointAttributeChannel CreateChannel(ChannelId id, PointFormat format, size_t count,
                                    const void* data,
                                    const double scale[3], const double offset[3],
                                    size_t stride) {
    PointAttributeChannel ch;
    ch.Create(id, format, count, data, scale, offset, stride);
    return ch;
}

} // namespace pointcloud
} // namespace workstation
