#include "workstation/display/CachedGraphics.h"
#include <atomic>

namespace workstation {

static std::atomic<uint64_t> g_graphicsCounter{1};

CachedGraphics::CachedGraphics(uint64_t graphicsId, ElementId source,
                               uint64_t sourceRevision, size_t primitiveCount)
    : m_graphicsId(graphicsId),
      m_sourceElement(source),
      m_sourceGeometryRevision(sourceRevision),
      m_primitiveCount(primitiveCount) {}

CachedGraphicsHandle CreateCachedGraphics(ElementId source,
                                          uint64_t sourceRevision,
                                          size_t primitiveCount) {
    uint64_t id = g_graphicsCounter.fetch_add(1);
    return std::make_shared<CachedGraphics>(id, source, sourceRevision,
                                            primitiveCount);
}

} // namespace workstation
