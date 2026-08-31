#pragma once
#include <cstdint>
#include <functional>

namespace workstation {

// Strongly typed identifiers used throughout the public API instead of
// raw integers. This prevents accidental mixing of id kinds and documents
// intent at the type level.
template <typename Tag, typename Int = uint64_t>
class StrongId {
    Int m_value;
public:
    explicit StrongId(Int v = 0) : m_value(v) {}
    Int Value() const { return m_value; }
    bool operator==(const StrongId& o) const { return m_value == o.m_value; }
    bool operator!=(const StrongId& o) const { return m_value != o.m_value; }
    bool operator< (const StrongId& o) const { return m_value <  o.m_value; }
};

using DocumentId = StrongId<struct DocumentTag>;
using ModelId    = StrongId<struct ModelTag>;
using ElementId  = StrongId<struct ElementTag>;

} // namespace workstation

namespace std {
template <typename T, typename I>
struct hash<workstation::StrongId<T, I>> {
    size_t operator()(const workstation::StrongId<T, I>& id) const noexcept {
        return hash<I>()(id.Value());
    }
};
}
