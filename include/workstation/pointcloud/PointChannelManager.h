#pragma once
#include <cstddef>
#include <cstdint>

namespace workstation { namespace pointcloud {

// Channel identifiers discovered for the point attribute channel system.
enum class ChannelId : uint32_t {
    XYZ = 0,
    RGB = 1,
    Intensity = 2,
    Classification = 3,
    Normals = 4
};

// Storage format for a channel's elements (clean-room; no proprietary enum name).
// FORMAT 1 = int16, FORMAT 2 = float32 (per XYZ decoding RE).
enum class PointFormat : uint8_t {
    Float32 = 0,
    Int16   = 1,
    UInt8   = 2
};

// Internal format type codes (FUN_18006e2f0). The element-size table below is
// CONFIRMED; their mapping to a high-level ChannelId is NOT reversed.
enum class PointChannelType : uint32_t {
    Type1  = 1,
    Type2  = 2,
    Type4  = 4,
    Type5  = 5,
    Type6  = 6,
    Type7  = 7,
    Type8  = 8,
    Type9  = 9,
    Type10 = 10
};

// CONFIRMED element-size table (FUN_18006e2f0).
inline size_t GetElementSize(PointChannelType t) {
    switch (t) {
        case PointChannelType::Type1:  return 4;
        case PointChannelType::Type2:  return 8;
        case PointChannelType::Type4:  return 1;
        case PointChannelType::Type5:  return 1;
        case PointChannelType::Type6:  return 2;
        case PointChannelType::Type7:  return 2;
        case PointChannelType::Type8:  return 4;
        case PointChannelType::Type9:  return 4;
        case PointChannelType::Type10: return 8;
    }
    return 0;
}

// Lightweight channel descriptor used by the Piece 1 query layer. The point
// cloud storage layer (Piece 2) uses the independent PointAttributeChannel.
struct PointChannel {
    ChannelId id = ChannelId::XYZ;
    void* data = nullptr;
    size_t elementSize = 0;
    uint32_t metadata = 0;
};

// Confirmed: the system supports up to 32 channels. No proprietary reader.
class PointChannelManager {
public:
    static constexpr size_t MAX_CHANNELS = 32;

    bool Add(const PointChannel& ch) {
        if (count_ >= MAX_CHANNELS) return false;
        channels_[count_++] = ch;
        return true;
    }

    void AddDefaultChannels() {
        Add(PointChannel{ChannelId::XYZ, nullptr, 12, 0});
        Add(PointChannel{ChannelId::RGB, nullptr, 12, 0});
        Add(PointChannel{ChannelId::Intensity, nullptr, 4, 0});
        Add(PointChannel{ChannelId::Classification, nullptr, 1, 0});
        Add(PointChannel{ChannelId::Normals, nullptr, 12, 0});
    }

    const PointChannel* Get(ChannelId id) const {
        for (size_t i = 0; i < count_; ++i)
            if (channels_[i].id == id) return &channels_[i];
        return nullptr;
    }

    size_t Count() const { return count_; }
    const PointChannel* begin() const { return channels_; }
    const PointChannel* end() const { return channels_ + count_; }
    void Clear() { count_ = 0; }

private:
    PointChannel channels_[MAX_CHANNELS];
    size_t count_ = 0;
};

} // namespace pointcloud
} // namespace workstation
