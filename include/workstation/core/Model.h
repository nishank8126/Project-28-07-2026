#pragma once
#include "workstation/core/IdTypes.h"
#include "workstation/core/Element.h"
#include <memory>
#include <vector>

namespace workstation {

// A Model owns the CAD elements of one logical partition of a Document.
// Ownership is exclusive: the Model owns the Element instances it creates.
class Model {
    ModelId m_id;
    std::string m_name;
    std::vector<std::unique_ptr<Element>> m_elements;

public:
    Model(ModelId id, std::string name = "");

    ModelId Id() const { return m_id; }
    const std::string& Name() const { return m_name; }

    Element* CreateElement(std::string label = "");
    Element* Find(ElementId id) const;
    size_t ElementCount() const { return m_elements.size(); }
};

} // namespace workstation
