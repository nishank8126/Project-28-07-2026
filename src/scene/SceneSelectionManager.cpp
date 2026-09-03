#include "workstation/scene/SceneSelectionManager.h"
#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/DwgAttachment.h"
#include "workstation/cad/SntAttachment.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace workstation {
namespace scene {

SelectionResult SceneSelectionManager::Pick(int mouseX, int mouseY,
                                             int viewportWidth, int viewportHeight,
                                             const renderer::Camera& camera,
                                             SelectionMode mode) {
    if (!sceneManager_) return {};

    Ray ray = RayPicker::ScreenToWorldRay(mouseX, mouseY, viewportWidth, viewportHeight, camera);

    SelectionResult bestResult;
    bestResult.distance = std::numeric_limits<double>::max();

    sceneManager_->ForEachObject([&](SceneObject* obj) {
        if (!obj || !obj->GetNode()) return;
        if (!obj->GetNode()->IsVisible()) return;
        if (!filter_.IsObjectAllowed(obj->GetType())) return;

        SelectionResult result;
        switch (obj->GetType()) {
            case ObjectType::PointCloud:
                result = PickPointCloud(ray, obj);
                break;
            case ObjectType::CadAttachment:
                result = PickCadAttachment(ray, obj);
                break;
            default:
                break;
        }

        if (result.valid && result.distance < bestResult.distance) {
            bestResult = result;
        }
    });

    if (bestResult.valid) {
        switch (mode) {
            case SelectionMode::Single:
                selectedObjects_.clear();
                AddToSelection(bestResult);
                break;
            case SelectionMode::Add:
                AddToSelection(bestResult);
                break;
            case SelectionMode::Toggle:
                if (IsSelected(bestResult.objectID)) {
                    RemoveFromSelection(bestResult.objectID);
                } else {
                    AddToSelection(bestResult);
                }
                break;
            case SelectionMode::Remove:
                RemoveFromSelection(bestResult.objectID);
                break;
        }
        if (selectionCallback_) selectionCallback_(bestResult);
    } else if (mode == SelectionMode::Single) {
        selectedObjects_.clear();
    }

    return bestResult;
}

std::vector<SelectionResult> SceneSelectionManager::PickMultiple(
    int mouseX, int mouseY, int viewportWidth, int viewportHeight,
    const renderer::Camera& camera) {
    if (!sceneManager_) return {};

    Ray ray = RayPicker::ScreenToWorldRay(mouseX, mouseY, viewportWidth, viewportHeight, camera);
    std::vector<SelectionResult> results;
    double threshold = 0.5;

    sceneManager_->ForEachObject([&](SceneObject* obj) {
        if (!obj || !obj->GetNode()) return;
        if (!obj->GetNode()->IsVisible()) return;
        if (!filter_.IsObjectAllowed(obj->GetType())) return;

        SelectionResult result;
        switch (obj->GetType()) {
            case ObjectType::PointCloud:
                result = PickPointCloud(ray, obj);
                break;
            case ObjectType::CadAttachment:
                result = PickCadAttachment(ray, obj);
                break;
            default:
                break;
        }

        if (result.valid && result.distance < threshold) {
            results.push_back(result);
        }
    });

    std::sort(results.begin(), results.end(),
              [](const SelectionResult& a, const SelectionResult& b) {
                  return a.distance < b.distance;
              });

    return results;
}

std::vector<SelectionResult> SceneSelectionManager::PickInRegion(
    const std::vector<std::pair<int,int>>& screenPoints,
    int viewportWidth, int viewportHeight,
    const renderer::Camera& camera) {
    if (!sceneManager_ || screenPoints.size() < 3) return {};

    std::vector<SelectionResult> results;

    sceneManager_->ForEachObject([&](SceneObject* obj) {
        if (!obj || !obj->GetNode()) return;
        if (!obj->GetNode()->IsVisible()) return;
        if (!filter_.IsObjectAllowed(obj->GetType())) return;

        auto bounds = obj->GetNode()->GetWorldBounds();
        math::Point3d center = {
            (bounds.minX + bounds.maxX) * 0.5,
            (bounds.minY + bounds.maxY) * 0.5,
            (bounds.minZ + bounds.maxZ) * 0.5
        };

        auto& cam = const_cast<renderer::Camera&>(camera);
        auto projected = cam.GetViewProjectionMatrix().MultiplyAndRenormalize(center);
        double cx = projected.x;
        double cy = projected.y;

        int sx = static_cast<int>((cx + 1.0) * 0.5 * viewportWidth);
        int sy = static_cast<int>((1.0 - cy) * 0.5 * viewportHeight);

        bool inside = false;
        int n = static_cast<int>(screenPoints.size());
        for (int i = 0, j = n - 1; i < n; j = i++) {
            int yi = screenPoints[i].second, yj = screenPoints[j].second;
            int xi = screenPoints[i].first, xj = screenPoints[j].first;
            if (((yi > sy) != (yj > sy)) &&
                (sx < (xj - xi) * (sy - yi) / (double)(yj - yi) + xi)) {
                inside = !inside;
            }
        }

        if (inside) {
            SelectionResult result;
            result.valid = true;
            result.objectID = obj->GetNode()->GetID();
            result.objectType = obj->GetType();
            result.objectName = obj->GetDisplayName();
            results.push_back(result);
        }
    });

    selectedObjects_.clear();
    for (const auto& r : results) {
        selectedObjects_.push_back(r);
    }
    if (multiSelectionCallback_ && !results.empty()) multiSelectionCallback_(results);
    return results;
}

void SceneSelectionManager::SelectAll() {
    if (!sceneManager_) return;
    selectedObjects_.clear();
    sceneManager_->ForEachObject([this](SceneObject* obj) {
        if (!obj || !obj->GetNode()) return;
        if (!obj->GetNode()->IsVisible()) return;
        if (!filter_.IsObjectAllowed(obj->GetType())) return;

        SelectionResult result;
        result.valid = true;
        result.objectID = obj->GetNode()->GetID();
        result.objectType = obj->GetType();
        result.objectName = obj->GetDisplayName();
        selectedObjects_.push_back(result);
    });
}

void SceneSelectionManager::DeselectAll() {
    selectedObjects_.clear();
}

void SceneSelectionManager::InvertSelection() {
    if (!sceneManager_) return;
    std::vector<SelectionResult> inverted;
    sceneManager_->ForEachObject([this, &inverted](SceneObject* obj) {
        if (!obj || !obj->GetNode()) return;
        if (!obj->GetNode()->IsVisible()) return;
        if (!filter_.IsObjectAllowed(obj->GetType())) return;

        if (!IsSelected(obj->GetNode()->GetID())) {
            SelectionResult result;
            result.valid = true;
            result.objectID = obj->GetNode()->GetID();
            result.objectType = obj->GetType();
            result.objectName = obj->GetDisplayName();
            inverted.push_back(result);
        }
    });
    selectedObjects_ = std::move(inverted);
}

SelectionResult SceneSelectionManager::GetPrimarySelection() const {
    if (selectedObjects_.empty()) return {};
    return selectedObjects_.front();
}

std::vector<NodeID> SceneSelectionManager::GetSelectedObjectIDs() const {
    std::vector<NodeID> ids;
    ids.reserve(selectedObjects_.size());
    for (const auto& s : selectedObjects_) {
        ids.push_back(s.objectID);
    }
    return ids;
}

std::vector<uint64_t> SceneSelectionManager::GetSelectedEntityIDs() const {
    std::vector<uint64_t> ids;
    for (const auto& s : selectedObjects_) {
        if (s.entityID > 0) ids.push_back(s.entityID);
    }
    return ids;
}

SelectionResult SceneSelectionManager::PickPointCloud(const Ray& ray, SceneObject* obj) {
    SelectionResult result;
    auto* node = obj->GetNode();
    if (!node) return result;

    auto aabbHit = RayPicker::IntersectAABB(ray, node->GetWorldBounds());
    if (!aabbHit.hit) return result;

    result.valid = true;
    result.objectID = node->GetID();
    result.objectType = ObjectType::PointCloud;
    result.distance = aabbHit.distance;
    result.worldPosition = aabbHit.position;
    result.objectName = obj->GetDisplayName();
    return result;
}

SelectionResult SceneSelectionManager::PickCadAttachment(const Ray& ray, SceneObject* obj) {
    auto* cad = static_cast<CadSceneObject*>(obj);
    if (!cad) return {};

    SelectionResult best;
    best.distance = std::numeric_limits<double>::max();

    if (cad->GetDxfAttachment()) {
        auto r = PickCadDxf(ray, cad);
        if (r.valid && r.distance < best.distance) best = r;
    } else if (cad->GetDwgAttachment()) {
        auto r = PickCadDwg(ray, cad);
        if (r.valid && r.distance < best.distance) best = r;
    } else if (cad->GetSntAttachment()) {
        auto r = PickCadSnt(ray, cad);
        if (r.valid && r.distance < best.distance) best = r;
    }

    return best;
}

SelectionResult SceneSelectionManager::PickCadDxf(const Ray& ray, CadSceneObject* cad) {
    SelectionResult result;
    auto* att = cad->GetDxfAttachment();
    if (!att) return result;

    auto* lm = att->layerManager();
    const auto& doc = att->document();
    double bestDist = std::numeric_limits<double>::max();

    for (const auto& entity : doc.entities) {
        if (lm && !lm->isLayerVisible(entity.layer)) continue;

        if (entity.type == "LINE" && entity.points.size() >= 2) {
            math::Point3d p0 = {entity.points[0].x, entity.points[0].y, entity.points[0].z};
            math::Point3d p1 = {entity.points[1].x, entity.points[1].y, entity.points[1].z};
            auto hit = RayPicker::IntersectLineSegment(ray, p0, p1, pickTolerance_ * 0.001);
            if (hit.hit && hit.distance < bestDist) {
                bestDist = hit.distance;
                result.valid = true;
                result.objectID = cad->GetNode()->GetID();
                result.objectType = ObjectType::CadAttachment;
                result.distance = hit.distance;
                result.worldPosition = hit.position;
                result.layerName = entity.layer;
                result.objectName = cad->GetDisplayName();
            }
        } else if ((entity.type == "POLYLINE" || entity.type == "LWPOLYLINE") && entity.points.size() >= 2) {
            for (size_t i = 0; i + 1 < entity.points.size(); ++i) {
                math::Point3d p0 = {entity.points[i].x, entity.points[i].y, entity.points[i].z};
                math::Point3d p1 = {entity.points[i+1].x, entity.points[i+1].y, entity.points[i+1].z};
                auto hit = RayPicker::IntersectLineSegment(ray, p0, p1, pickTolerance_ * 0.001);
                if (hit.hit && hit.distance < bestDist) {
                    bestDist = hit.distance;
                    result.valid = true;
                    result.objectID = cad->GetNode()->GetID();
                    result.objectType = ObjectType::CadAttachment;
                    result.distance = hit.distance;
                    result.worldPosition = hit.position;
                    result.layerName = entity.layer;
                    result.objectName = cad->GetDisplayName();
                }
            }
        } else if (entity.type == "CIRCLE" && entity.radius > 0) {
            math::Point3d center = {entity.center.x, entity.center.y, entity.center.z};
            math::Point3d normal = {0, 0, 1};
            auto hit = RayPicker::IntersectCircle(ray, center, normal, entity.radius);
            if (hit.hit && hit.distance < bestDist) {
                bestDist = hit.distance;
                result.valid = true;
                result.objectID = cad->GetNode()->GetID();
                result.objectType = ObjectType::CadAttachment;
                result.distance = hit.distance;
                result.worldPosition = hit.position;
                result.layerName = entity.layer;
                result.objectName = cad->GetDisplayName();
            }
        }
    }

    return result;
}

SelectionResult SceneSelectionManager::PickCadDwg(const Ray& ray, CadSceneObject* cad) {
    SelectionResult result;
    auto* att = cad->GetDwgAttachment();
    if (!att) return result;

    auto* lm = att->layerManager();
    double bestDist = std::numeric_limits<double>::max();

    att->buildGeometry();

    return result;
}

SelectionResult SceneSelectionManager::PickCadSnt(const Ray& ray, CadSceneObject* cad) {
    SelectionResult result;
    auto* att = cad->GetSntAttachment();
    if (!att) return result;

    auto* lm = att->layerManager();
    const auto& doc = att->document();
    double bestDist = std::numeric_limits<double>::max();

    for (const auto& entity : doc.entities) {
        if (lm && !lm->isLayerVisible(entity.layer)) continue;

        if (entity.vertices.size() >= 2) {
            for (size_t i = 0; i + 1 < entity.vertices.size(); ++i) {
                math::Point3d p0 = {entity.vertices[i][0], entity.vertices[i][1], entity.vertices[i][2]};
                math::Point3d p1 = {entity.vertices[i+1][0], entity.vertices[i+1][1], entity.vertices[i+1][2]};
                auto hit = RayPicker::IntersectLineSegment(ray, p0, p1, pickTolerance_ * 0.001);
                if (hit.hit && hit.distance < bestDist) {
                    bestDist = hit.distance;
                    result.valid = true;
                    result.objectID = cad->GetNode()->GetID();
                    result.objectType = ObjectType::CadAttachment;
                    result.distance = hit.distance;
                    result.worldPosition = hit.position;
                    result.layerName = entity.layer;
                    result.objectName = cad->GetDisplayName();
                }
            }
        } else if (entity.type == cad::SntEntity::Circle && entity.radius > 0) {
            math::Point3d center = {entity.center[0], entity.center[1], entity.center[2]};
            math::Point3d normal = {0, 0, 1};
            auto hit = RayPicker::IntersectCircle(ray, center, normal, entity.radius);
            if (hit.hit && hit.distance < bestDist) {
                bestDist = hit.distance;
                result.valid = true;
                result.objectID = cad->GetNode()->GetID();
                result.objectType = ObjectType::CadAttachment;
                result.distance = hit.distance;
                result.worldPosition = hit.position;
                result.layerName = entity.layer;
                result.objectName = cad->GetDisplayName();
            }
        }
    }

    return result;
}

SelectionResult SceneSelectionManager::PickCadLine(const Ray& ray, CadSceneObject* cad) {
    SelectionResult result;
    auto* att = cad->GetDxfAttachment();
    if (!att) return result;

    double bestDist = std::numeric_limits<double>::max();
    const auto& doc = att->document();
    for (const auto& entity : doc.entities) {
        if (entity.type == "LINE" && entity.points.size() >= 2) {
            math::Point3d p0 = {entity.points[0].x, entity.points[0].y, entity.points[0].z};
            math::Point3d p1 = {entity.points[1].x, entity.points[1].y, entity.points[1].z};
            auto hit = RayPicker::IntersectLineSegment(ray, p0, p1, pickTolerance_ * 0.001);
            if (hit.hit && hit.distance < bestDist) {
                bestDist = hit.distance;
                result.valid = true;
                result.objectID = cad->GetNode()->GetID();
                result.objectType = ObjectType::CadAttachment;
                result.distance = hit.distance;
                result.worldPosition = hit.position;
                result.layerName = entity.layer;
                result.objectName = cad->GetDisplayName();
            }
        }
    }
    return result;
}

SelectionResult SceneSelectionManager::PickCadPolyline(const Ray& ray, CadSceneObject* cad) {
    return PickCadLine(ray, cad);
}

SelectionResult SceneSelectionManager::PickCadCircle(const Ray& ray, CadSceneObject* cad) {
    SelectionResult result;
    auto* att = cad->GetDxfAttachment();
    if (!att) return result;

    double bestDist = std::numeric_limits<double>::max();
    const auto& doc = att->document();
    for (const auto& entity : doc.entities) {
        if (entity.type == "CIRCLE" && entity.radius > 0) {
            math::Point3d center = {entity.center.x, entity.center.y, entity.center.z};
            math::Point3d normal = {0, 0, 1};
            auto hit = RayPicker::IntersectCircle(ray, center, normal, entity.radius);
            if (hit.hit && hit.distance < bestDist) {
                bestDist = hit.distance;
                result.valid = true;
                result.objectID = cad->GetNode()->GetID();
                result.objectType = ObjectType::CadAttachment;
                result.distance = hit.distance;
                result.worldPosition = hit.position;
                result.layerName = entity.layer;
                result.objectName = cad->GetDisplayName();
            }
        }
    }
    return result;
}

SelectionResult SceneSelectionManager::PickCadSurface(const Ray& ray, CadSceneObject* cad) {
    return {};
}

void SceneSelectionManager::AddToSelection(const SelectionResult& result) {
    if (!IsSelected(result.objectID)) {
        selectedObjects_.push_back(result);
    }
}

void SceneSelectionManager::RemoveFromSelection(NodeID objectID) {
    selectedObjects_.erase(
        std::remove_if(selectedObjects_.begin(), selectedObjects_.end(),
                        [objectID](const SelectionResult& r) { return r.objectID == objectID; }),
        selectedObjects_.end());
}

bool SceneSelectionManager::IsSelected(NodeID objectID) const {
    return std::any_of(selectedObjects_.begin(), selectedObjects_.end(),
                        [objectID](const SelectionResult& r) { return r.objectID == objectID; });
}

} // namespace scene
} // namespace workstation
