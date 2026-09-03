#pragma once
#include "workstation/scene/SceneObject.h"
#include <cstdint>
#include <vector>

namespace workstation {
namespace scene {

struct SelectionFilter {
    bool selectPointClouds = true;
    bool selectCadAttachments = true;
    bool selectMeshes = true;
    bool selectAnnotations = true;
    bool selectMeasurements = true;
    bool selectSections = true;

    std::vector<uint32_t> allowedLayers;
    std::vector<uint32_t> excludedLayers;

    bool IsObjectAllowed(ObjectType type) const {
        switch (type) {
            case ObjectType::PointCloud: return selectPointClouds;
            case ObjectType::CadAttachment: return selectCadAttachments;
            case ObjectType::Mesh: return selectMeshes;
            case ObjectType::Annotation: return selectAnnotations;
            case ObjectType::Measurement: return selectMeasurements;
            case ObjectType::Section: return selectSections;
            default: return true;
        }
    }

    bool IsLayerAllowed(uint32_t layerID) const {
        if (!excludedLayers.empty()) {
            for (uint32_t id : excludedLayers) {
                if (id == layerID) return false;
            }
        }
        if (!allowedLayers.empty()) {
            for (uint32_t id : allowedLayers) {
                if (id == layerID) return true;
            }
            return false;
        }
        return true;
    }

    static SelectionFilter All() {
        SelectionFilter f;
        f.selectPointClouds = true;
        f.selectCadAttachments = true;
        f.selectMeshes = true;
        f.selectAnnotations = true;
        f.selectMeasurements = true;
        f.selectSections = true;
        return f;
    }

    static SelectionFilter CadOnly() {
        SelectionFilter f;
        f.selectPointClouds = false;
        f.selectCadAttachments = true;
        f.selectMeshes = false;
        f.selectAnnotations = false;
        f.selectMeasurements = false;
        f.selectSections = false;
        return f;
    }

    static SelectionFilter PointsOnly() {
        SelectionFilter f;
        f.selectPointClouds = true;
        f.selectCadAttachments = false;
        f.selectMeshes = false;
        f.selectAnnotations = false;
        f.selectMeasurements = false;
        f.selectSections = false;
        return f;
    }
};

} // namespace scene
} // namespace workstation
