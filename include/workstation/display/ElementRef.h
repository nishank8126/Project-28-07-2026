#pragma once
#include "workstation/core/IdTypes.h"
#include "workstation/core/Element.h"
#include "workstation/display/CachedGraphics.h"
#include "workstation/display/CacheKeys.h"
#include "workstation/display/GraphicsVariant.h"
#include <memory>
#include <vector>

namespace workstation {

// Reference to an Element used by the display/rendering side. It owns the
// element's retained-graphics cache: a small set of cheap direct slots for
// common variants (RE C) plus an optional specialized variant set (RE D).
//
// RE UNKNOWN:
//   The proprietary node layout and intrusive allocator are not reproduced.
//   Ownership here is std::vector / std::unique_ptr so an arena/slab can be
//   added later (RE I) without changing the public architecture.
class ElementRef {
public:
    // Number of cheap direct common-variant slots. An implementation
    // decision (RE C: "Exact N is OUR implementation decision").
    static constexpr size_t kMaxDirectVariants = 64;

private:
    ElementId m_elementId;
    Element*  m_element;   // non-owning; Model owns the Element
    std::vector<CachedGraphicsHandle> m_direct;
    std::unique_ptr<ElementGraphicsVariantSet> m_specialized;

public:
    ElementRef(ElementId id, Element* element);

    ElementId Id() const { return m_elementId; }
    Element*  GetElement() const { return m_element; }

    // --- Fast direct path (RE C) ---
    void SetDirectGraphics(uint32_t variant, CachedGraphicsHandle g);
    CachedGraphicsHandle GetDirectGraphics(uint32_t variant) const;

    // --- Specialized path (RE D/H) ---
    void InsertSpecialized(GraphicsVariant v);
    CachedGraphicsHandle FindSpecialized(const GraphicsUnsizedKey& key,
                                        double metric) const;
    ElementGraphicsVariantSet* Specialized() const { return m_specialized.get(); }

    // --- Invalidation (RE: our initial policy) ---
    // Clears both direct and specialized retained graphics. Geometry-revision
    // based staleness is also enforced at lookup time (see GetDirectGraphics
    // and ElementGraphicsVariantSet::Find).
    void InvalidateGraphics();
};

} // namespace workstation
