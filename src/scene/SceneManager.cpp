#include "workstation/scene/SceneManager.h"
#include <algorithm>
#include <cstdio>

namespace workstation {
namespace scene {

void SceneManager::Initialize() {
    cadRenderer_ = nullptr;
}

void SceneManager::Shutdown() {
    Clear();
}

void SceneManager::Clear() {
    objects_.clear();
    idToObjectIndex_.clear();
    nodes_.clear();
    layers_.clear();
    sceneBounds_ = {};
    nextNodeID_ = 1;
    nextLayerID_ = 1;
    ++revision_;
}

NodeID SceneManager::AddObject(std::unique_ptr<SceneObject> object, const std::string& name) {
    if (!object) return 0;

    NodeID nodeID = NextNodeID();
    std::string objName = name.empty() ? object->GetDisplayName() : name;
    if (objName.empty()) objName = "Object " + std::to_string(nodeID);

    SceneNode node(nodeID, objName);
    node.SetObjectType(static_cast<uint32_t>(object->GetType()));
    nodes_[nodeID] = node;

    object->SetNode(&nodes_[nodeID]);
    object->SetDisplayName(objName);

    size_t index = objects_.size();
    objects_.push_back(std::move(object));
    idToObjectIndex_[nodeID] = index;

    ++revision_;
    return nodeID;
}

void SceneManager::RemoveObject(NodeID id) {
    auto it = idToObjectIndex_.find(id);
    if (it == idToObjectIndex_.end()) return;

    size_t index = it->second;
    auto& obj = objects_[index];
    if (obj) obj->Unload();

    objects_.erase(objects_.begin() + index);
    nodes_.erase(id);
    idToObjectIndex_.erase(id);

    for (auto& [nid, idx] : idToObjectIndex_) {
        if (idx > index) --idx;
    }
    ++revision_;
}

void SceneManager::RemoveAllObjects() {
    for (auto& obj : objects_) {
        if (obj) obj->Unload();
    }
    objects_.clear();
    idToObjectIndex_.clear();
    nodes_.clear();
    sceneBounds_ = {};
    ++revision_;
}

SceneObject* SceneManager::FindObject(NodeID id) const {
    auto it = idToObjectIndex_.find(id);
    if (it == idToObjectIndex_.end()) return nullptr;
    return objects_[it->second].get();
}

SceneObject* SceneManager::FindObjectByName(const std::string& name) const {
    for (const auto& obj : objects_) {
        if (obj && obj->GetDisplayName() == name) return obj.get();
    }
    return nullptr;
}

SceneObject* SceneManager::FindObjectByPath(const std::string& path) const {
    for (const auto& obj : objects_) {
        if (obj && obj->GetFilePath() == path) return obj.get();
    }
    return nullptr;
}

std::vector<SceneObject*> SceneManager::GetVisibleObjects() const {
    std::vector<SceneObject*> result;
    for (const auto& obj : objects_) {
        if (!obj) continue;
        auto* node = obj->GetNode();
        if (node && node->IsEffectivelyVisible()) {
            result.push_back(obj.get());
        }
    }
    return result;
}

std::vector<SceneObject*> SceneManager::GetObjectsByType(ObjectType type) const {
    std::vector<SceneObject*> result;
    for (const auto& obj : objects_) {
        if (obj && obj->GetType() == type) result.push_back(obj.get());
    }
    return result;
}

void SceneManager::Update() {
    for (auto& obj : objects_) {
        if (obj && (obj->IsDirty() || obj->GetNode()->IsDirty())) {
            obj->Update();
            obj->ClearDirty();
            obj->GetNode()->ClearDirty();
        }
    }
    RebuildBounds();
}

void SceneManager::SubmitRenderCommands(renderer::RenderQueue& queue,
                                          void* linePipeline,
                                          void* pipelineLayout) {
    for (auto& obj : objects_) {
        if (!obj) continue;
        auto* node = obj->GetNode();
        if (!node || !node->IsEffectivelyVisible()) continue;
        if (!layers_.empty()) {
            auto layerIt = layers_.find(node->GetLayerID());
            if (layerIt != layers_.end() && !layerIt->second.visible) continue;
        }
        obj->SubmitRenderCommands(queue, linePipeline, pipelineLayout);
    }
}

uint32_t SceneManager::AddLayer(const std::string& name, uint32_t color) {
    uint32_t id = nextLayerID_++;
    SceneLayer layer;
    layer.name = name;
    layer.color = color;
    layers_[id] = layer;
    return id;
}

void SceneManager::RemoveLayer(uint32_t layerID) {
    layers_.erase(layerID);
}

void SceneManager::SetLayerVisible(uint32_t layerID, bool visible) {
    auto it = layers_.find(layerID);
    if (it != layers_.end()) it->second.visible = visible;
}

bool SceneManager::IsLayerVisible(uint32_t layerID) const {
    auto it = layers_.find(layerID);
    return (it != layers_.end()) ? it->second.visible : true;
}

SceneLayer* SceneManager::GetLayer(uint32_t layerID) {
    auto it = layers_.find(layerID);
    return (it != layers_.end()) ? &it->second : nullptr;
}

void SceneManager::AssignObjectToLayer(NodeID nodeID, uint32_t layerID) {
    auto nodeIt = nodes_.find(nodeID);
    if (nodeIt != nodes_.end()) {
        nodeIt->second.SetLayerID(layerID);
        auto layerIt = layers_.find(layerID);
        if (layerIt != layers_.end()) {
            layerIt->second.objectIDs.push_back(nodeID);
        }
    }
}

void SceneManager::SetSelected(NodeID id, bool selected) {
    auto it = nodes_.find(id);
    if (it != nodes_.end()) it->second.SetSelected(selected);
    auto* obj = FindObject(id);
    if (obj) obj->GetNode()->SetSelected(selected);
}

void SceneManager::ClearSelection() {
    for (auto& [id, node] : nodes_) {
        node.SetSelected(false);
    }
}

std::vector<NodeID> SceneManager::GetSelectedIDs() const {
    std::vector<NodeID> result;
    for (const auto& [id, node] : nodes_) {
        if (node.IsSelected()) result.push_back(id);
    }
    return result;
}

void SceneManager::ForEachObject(ObjectCallback callback) const {
    for (const auto& obj : objects_) {
        if (obj) callback(obj.get());
    }
}

void SceneManager::RebuildBounds() {
    sceneBounds_ = {};
    bool first = true;
    for (const auto& obj : objects_) {
        if (!obj) continue;
        auto b = obj->GetBounds();
        if (b.maxX <= b.minX && b.maxY <= b.minY && b.maxZ <= b.minZ) continue;
        if (first) {
            sceneBounds_ = b;
            first = false;
        } else {
            sceneBounds_.minX = std::min(sceneBounds_.minX, b.minX);
            sceneBounds_.minY = std::min(sceneBounds_.minY, b.minY);
            sceneBounds_.minZ = std::min(sceneBounds_.minZ, b.minZ);
            sceneBounds_.maxX = std::max(sceneBounds_.maxX, b.maxX);
            sceneBounds_.maxY = std::max(sceneBounds_.maxY, b.maxY);
            sceneBounds_.maxZ = std::max(sceneBounds_.maxZ, b.maxZ);
        }
    }
}

} // namespace scene
} // namespace workstation
