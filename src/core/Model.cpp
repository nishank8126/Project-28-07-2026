#include "workstation/core/Model.h"
#include <stdexcept>

namespace workstation {

static uint64_t g_elementCounter = 0;

Model::Model(ModelId id, std::string name)
    : m_id(id), m_name(std::move(name)) {}

Element* Model::CreateElement(std::string label) {
    ElementId id(++g_elementCounter);
    auto e = std::make_unique<Element>(id, std::move(label));
    Element* p = e.get();
    m_elements.push_back(std::move(e));
    return p;
}

Element* Model::Find(ElementId id) const {
    for (const auto& e : m_elements)
        if (e->Id() == id) return e.get();
    return nullptr;
}

} // namespace workstation
