#include "workstation/core/Document.h"

namespace workstation {

static uint64_t g_modelCounter = 0;

Document::Document(DocumentId id) : m_id(id) {}

Model* Document::CreateModel(std::string name) {
    ModelId id(++g_modelCounter);
    auto m = std::make_unique<Model>(id, std::move(name));
    Model* p = m.get();
    m_models.push_back(std::move(m));
    return p;
}

Model* Document::Find(ModelId id) const {
    for (const auto& m : m_models)
        if (m->Id() == id) return m.get();
    return nullptr;
}

} // namespace workstation
