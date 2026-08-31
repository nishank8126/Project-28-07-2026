#include "workstation/display/ElementRef.h"

namespace workstation {

ElementRef::ElementRef(ElementId id, Element* element)
    : m_elementId(id), m_element(element) {}

void ElementRef::SetDirectGraphics(uint32_t variant, CachedGraphicsHandle g) {
    if (variant >= kMaxDirectVariants) return;
    if (m_direct.size() <= variant) m_direct.resize(variant + 1);
    m_direct[variant] = g;
}

CachedGraphicsHandle ElementRef::GetDirectGraphics(uint32_t variant) const {
    if (variant >= m_direct.size()) return nullptr;
    auto g = m_direct[variant];
    if (!g) return nullptr;
    // Geometry-revision based staleness check (RE: our initial policy).
    if (m_element && g->SourceGeometryRevision() != m_element->GeometryRevision())
        return nullptr;
    return g;
}

void ElementRef::InsertSpecialized(GraphicsVariant v) {
    if (!m_specialized)
        m_specialized = std::make_unique<ElementGraphicsVariantSet>();
    m_specialized->Insert(std::move(v));
}

CachedGraphicsHandle ElementRef::FindSpecialized(const GraphicsUnsizedKey& key,
                                                double metric) const {
    if (!m_specialized) return nullptr;
    uint64_t rev = m_element ? m_element->GeometryRevision() : 0;
    return m_specialized->Find(key, metric, rev);
}

void ElementRef::InvalidateGraphics() {
    for (auto& s : m_direct) s.reset();
    if (m_specialized) m_specialized->Clear();
}

} // namespace workstation
