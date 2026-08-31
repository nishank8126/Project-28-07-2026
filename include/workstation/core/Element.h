#pragma once
#include "workstation/core/IdTypes.h"
#include <cstdint>
#include <string>

namespace workstation {

// An Element carries the CAD meaning of a single modeled object.
// For Piece 1 it only holds enough state to prove the architecture:
//   - a stable identity (ElementId)
//   - a geometry-revision counter used for cache invalidation
//   - an optional debug label
//
// RE UNKNOWN:
//   The exact proprietary geometry-invalidation policy has not yet been
//   recovered. GeometryRevision()/TouchGeometry() are OUR independent
//   design for future element-level invalidation and may be refined when
//   reverse engineering proves the real mechanism.
class Element {
    ElementId m_id;
    uint64_t  m_geometryRevision;
    std::string m_label;
public:
    Element(ElementId id, std::string label = "");

    ElementId Id() const { return m_id; }
    uint64_t  GeometryRevision() const { return m_geometryRevision; }

    // Advance the geometry revision. Any CachedGraphics that recorded
    // m_geometryRevision will be treated as stale by the lookup path.
    void TouchGeometry();

    const std::string& Label() const { return m_label; }
};

} // namespace workstation
