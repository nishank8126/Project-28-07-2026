#pragma once
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointAttributeMask.h"
#include <vector>

namespace workstation { namespace pointcloud {

// Point storage abstraction: owns the per-cloud attribute channels and exposes
// the decoded XYZ / RGB readers (FUN_1800820b0 / FUN_1800821f0). Independent
// C++ design; no proprietary container layout.
class PointStorage {
public:
    // Add (or replace) a channel by id.
    bool AddChannel(PointAttributeChannel ch) {
        for (auto& c : channels_)
            if (c.Id() == ch.Id()) { c = std::move(ch); return true; }
        channels_.push_back(std::move(ch));
        return true;
    }

    const PointAttributeChannel* GetChannel(ChannelId id) const {
        for (const auto& c : channels_)
            if (c.Id() == id) return &c;
        return nullptr;
    }

    // High-level decode using the stored XYZ / RGB channel.
    bool ReadXYZ(size_t index, double out[3]) const {
        const auto* c = GetChannel(ChannelId::XYZ);
        return c ? c->ReadXYZ(index, out) : false;
    }
    bool ReadRGB(size_t index, uint8_t out[3]) const {
        const auto* c = GetChannel(ChannelId::RGB);
        return c ? c->ReadRGB(index, out) : false;
    }

    // Aggregate stats across all channels.
    size_t PointCount() const {
        const auto* c = GetChannel(ChannelId::XYZ);
        return c ? c->Count() : 0;
    }
    PointAttributeMask Attributes() const {
        PointAttributeMask m;
        for (const auto& c : channels_) {
            switch (c.Id()) {
                case ChannelId::XYZ:            m.Set(PointAttribute::XYZ); break;
                case ChannelId::RGB:            m.Set(PointAttribute::RGB); break;
                case ChannelId::Intensity:      m.Set(PointAttribute::Intensity); break;
                case ChannelId::Classification: m.Set(PointAttribute::Classification); break;
                case ChannelId::Normals:        m.Set(PointAttribute::Normals); break;
            }
        }
        return m;
    }
    size_t MemoryBytes() const {
        size_t s = 0;
        for (const auto& c : channels_) s += c.SizeBytes();
        return s;
    }
    size_t ChannelCount() const { return channels_.size(); }

private:
    std::vector<PointAttributeChannel> channels_;
};

} // namespace pointcloud
} // namespace workstation
