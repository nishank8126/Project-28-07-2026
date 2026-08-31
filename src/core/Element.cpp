#include "workstation/core/Element.h"

namespace workstation {

Element::Element(ElementId id, std::string label)
    : m_id(id), m_geometryRevision(1), m_label(std::move(label)) {}

void Element::TouchGeometry() {
    ++m_geometryRevision;
}

} // namespace workstation
