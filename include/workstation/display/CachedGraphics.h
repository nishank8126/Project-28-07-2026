#pragma once
#include "workstation/core/IdTypes.h"
#include <cstdint>
#include <memory>

namespace workstation {

// Renderer-independent description of a retained graphics representation.
// It MUST NOT contain any Direct3D / GPU objects yet (RE rule S:
// Core/Display must not depend on Direct3D). The renderer will later
// consume this handle to bind GPU resources it manages separately.
class CachedGraphics {
    uint64_t  m_graphicsId;
    ElementId m_sourceElement;
    uint64_t  m_sourceGeometryRevision;
    size_t    m_primitiveCount;

public:
    CachedGraphics(uint64_t graphicsId, ElementId source,
                   uint64_t sourceRevision, size_t primitiveCount);

    uint64_t  GraphicsId() const { return m_graphicsId; }
    ElementId SourceElement() const { return m_sourceElement; }
    uint64_t  SourceGeometryRevision() const { return m_sourceGeometryRevision; }
    size_t    PrimitiveCount() const { return m_primitiveCount; }
};

using CachedGraphicsHandle = std::shared_ptr<CachedGraphics>;

// Factory used by tests and the demo. Real code will obtain graphics from
// the graphics producer/stroker (Piece 2).
CachedGraphicsHandle CreateCachedGraphics(ElementId source,
                                          uint64_t sourceRevision,
                                          size_t primitiveCount = 0);

} // namespace workstation
