#pragma once
#include "workstation/core/IdTypes.h"
#include "workstation/core/Model.h"
#include <memory>
#include <vector>

namespace workstation {

// A Document owns one or more Models. It is the top-level container of a
// CAD file's contents for Piece 1.
class Document {
    DocumentId m_id;
    std::vector<std::unique_ptr<Model>> m_models;

public:
    explicit Document(DocumentId id);

    DocumentId Id() const { return m_id; }

    Model* CreateModel(std::string name = "");
    Model* Find(ModelId id) const;
    size_t ModelCount() const { return m_models.size(); }
};

} // namespace workstation
