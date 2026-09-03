#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/scene/SceneObject.h"
#include <string>
#include <cstdint>

namespace workstation {
namespace scene {

using NodeID = uint64_t;

struct SelectionResult {
    bool valid = false;
    NodeID objectID = 0;
    ObjectType objectType = ObjectType::Unknown;
    uint64_t entityID = 0;
    math::Point3d worldPosition = {0, 0, 0};
    double distance = 0.0;
    std::string layerName;
    std::string objectName;
    uint32_t primitiveIndex = 0;

    void Clear() {
        valid = false;
        objectID = 0;
        objectType = ObjectType::Unknown;
        entityID = 0;
        worldPosition = {0, 0, 0};
        distance = 0.0;
        layerName.clear();
        objectName.clear();
        primitiveIndex = 0;
    }
};

struct MultiSelectionResult {
    std::vector<SelectionResult> results;
    uint32_t totalEntities = 0;
    bool HasSelection() const { return !results.empty(); }
    void Clear() { results.clear(); totalEntities = 0; }
};

} // namespace scene
} // namespace workstation
