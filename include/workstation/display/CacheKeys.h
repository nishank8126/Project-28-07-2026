#pragma once
#include <cstdint>
#include <optional>

namespace workstation {

// Immutable value key representing the effect a display style has on the
// generated geometry of an element. Initially a comparable id; later an
// IDisplayStyleGeometryKeyProvider (RE M) may build richer keys.
//
// RE M: a display style should not automatically be one giant integer that
// invalidates all graphics. The handler decides whether the geometry
// actually differs. For Piece 1 this is an opaque comparable value.
class DisplayStyleCacheKey {
    uint64_t m_value;
public:
    explicit DisplayStyleCacheKey(uint64_t v) : m_value(v) {}
    uint64_t Value() const { return m_value; }
    bool operator==(const DisplayStyleCacheKey& o) const { return m_value == o.m_value; }
    bool operator!=(const DisplayStyleCacheKey& o) const { return m_value != o.m_value; }
};

// Immutable value key representing display-filter (level/visibility) state
// that affects retained graphics. Opaque comparable value for Piece 1.
class DisplayFilterCacheKey {
    uint64_t m_value;
public:
    explicit DisplayFilterCacheKey(uint64_t v) : m_value(v) {}
    uint64_t Value() const { return m_value; }
    bool operator==(const DisplayFilterCacheKey& o) const { return m_value == o.m_value; }
    bool operator!=(const DisplayFilterCacheKey& o) const { return m_value != o.m_value; }
};

// The NON-SIZE portion of a cache key (RE E). View metric validity range
// is stored separately in GraphicsVariant so that small zoom changes inside
// the range remain cache hits without retessellation.
struct GraphicsUnsizedKey {
    std::optional<DisplayStyleCacheKey>  styleKey;
    std::optional<DisplayFilterCacheKey> filterKey;
    uint32_t transformKey = 0;  // RE N: our own transform-revision hash for now
    uint32_t variant = 0;       // small variant index for fast direct path

    // Simple path = no special state. Such keys go to the cheap direct slot.
    bool IsSimple() const {
        return !styleKey.has_value() && !filterKey.has_value() && transformKey == 0;
    }
};

} // namespace workstation
