#pragma once
#include "workstation/scene/SceneNode.h"
#include "workstation/scene/SceneObject.h"
#include "workstation/scene/PointCloudSceneObject.h"
#include "workstation/scene/CadSceneObject.h"
#include "workstation/renderer/CadRenderer.h"
#include "workstation/renderer/OverlayRenderer.h"
#include "workstation/renderer/SelectionRenderer.h"
#include "workstation/spatial/BoundingBox.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>

namespace workstation {
namespace renderer { class RenderQueue; }

namespace scene {

struct SceneLayer {
    std::string name;
    bool visible = true;
    float opacity = 1.0f;
    uint32_t color = 0xFFFFFFFF;
    std::vector<NodeID> objectIDs;
};

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void Initialize();
    void Shutdown();
    void Clear();

    NodeID AddObject(std::unique_ptr<SceneObject> object, const std::string& name = "");
    void RemoveObject(NodeID id);
    void RemoveAllObjects();

    SceneObject* FindObject(NodeID id) const;
    SceneObject* FindObjectByName(const std::string& name) const;
    SceneObject* FindObjectByPath(const std::string& path) const;

    const std::vector<std::unique_ptr<SceneObject>>& GetObjects() const { return objects_; }
    std::vector<SceneObject*> GetVisibleObjects() const;
    std::vector<SceneObject*> GetObjectsByType(ObjectType type) const;

    uint32_t GetObjectCount() const { return static_cast<uint32_t>(objects_.size()); }

    void Update();
    void SubmitRenderCommands(renderer::RenderQueue& queue,
                               void* linePipeline,
                               void* pipelineLayout);

    void SetCadRenderer(renderer::CadRenderer* renderer) { cadRenderer_ = renderer; }
    renderer::CadRenderer* GetCadRenderer() const { return cadRenderer_; }

    uint32_t AddLayer(const std::string& name, uint32_t color = 0xFFFFFFFF);
    void RemoveLayer(uint32_t layerID);
    void SetLayerVisible(uint32_t layerID, bool visible);
    bool IsLayerVisible(uint32_t layerID) const;
    SceneLayer* GetLayer(uint32_t layerID);
    const std::unordered_map<uint32_t, SceneLayer>& GetLayers() const { return layers_; }

    void AssignObjectToLayer(NodeID nodeID, uint32_t layerID);

    spatial::BoundingBox GetSceneBounds() const { return sceneBounds_; }

    void SetSelected(NodeID id, bool selected);
    void ClearSelection();
    std::vector<NodeID> GetSelectedIDs() const;

    using ObjectCallback = std::function<void(SceneObject*)>;
    void ForEachObject(ObjectCallback callback) const;

    uint64_t GetRevision() const { return revision_; }

    renderer::OverlayRenderer& GetOverlayRenderer() { return overlayRenderer_; }
    renderer::SelectionRenderer& GetSelectionRenderer() { return selectionRenderer_; }

private:
    void RebuildBounds();
    NodeID NextNodeID() { return nextNodeID_++; }

    std::vector<std::unique_ptr<SceneObject>> objects_;
    std::unordered_map<NodeID, size_t> idToObjectIndex_;
    std::unordered_map<uint64_t, SceneNode> nodes_;
    std::unordered_map<uint32_t, SceneLayer> layers_;

    renderer::CadRenderer* cadRenderer_ = nullptr;
    spatial::BoundingBox sceneBounds_ = {};
    uint64_t revision_ = 0;
    uint64_t nextNodeID_ = 1;
    uint32_t nextLayerID_ = 1;

    renderer::OverlayRenderer overlayRenderer_;
    renderer::SelectionRenderer selectionRenderer_;
};

} // namespace scene
} // namespace workstation
