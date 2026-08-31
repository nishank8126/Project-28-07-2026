#pragma once
#include <cstdint>

namespace workstation { namespace pointcloud {

// Point attribute flags (clean-room replica of the backend attribute set).
enum class PointAttribute : uint32_t {
    None           = 0,
    XYZ            = 1u << 0,
    RGB            = 1u << 1,
    Intensity      = 1u << 2,
    Classification = 1u << 3,
    Normals        = 1u << 4
};

inline PointAttribute operator|(PointAttribute a, PointAttribute b) {
    return static_cast<PointAttribute>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline PointAttribute operator&(PointAttribute a, PointAttribute b) {
    return static_cast<PointAttribute>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline PointAttribute operator~(PointAttribute a) {
    return static_cast<PointAttribute>(~static_cast<uint32_t>(a));
}

// Bitmask wrapper over the attribute flags.
class PointAttributeMask {
public:
    PointAttributeMask() = default;
    PointAttributeMask(PointAttribute a) : mask_(static_cast<uint32_t>(a)) {}

    bool Has(PointAttribute a) const {
        return (mask_ & static_cast<uint32_t>(a)) != 0;
    }
    void Set(PointAttribute a) { mask_ |= static_cast<uint32_t>(a); }
    uint32_t Value() const { return mask_; }

    PointAttributeMask& operator|=(const PointAttributeMask& o) { mask_ |= o.mask_; return *this; }
    PointAttributeMask operator|(const PointAttributeMask& o) const {
        PointAttributeMask r; r.mask_ = mask_ | o.mask_; return r;
    }

private:
    uint32_t mask_ = 0;
};

// ptPointAttributes(): clean-room replica of the backend attribute query.
// Returns the full attribute set exposed by the channel system.
inline PointAttributeMask ptPointAttributes() {
    return PointAttributeMask(PointAttribute::XYZ | PointAttribute::RGB |
                              PointAttribute::Intensity | PointAttribute::Classification |
                              PointAttribute::Normals);
}

} // namespace pointcloud
} // namespace workstation
