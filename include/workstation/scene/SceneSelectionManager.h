#pragma once
#include "workstation/scene/SelectionResult.h"
#include "workstation/scene/SelectionFilter.h"
#include "workstation/scene/RayPicker.h"
#include "workstation/scene/SceneManager.h"
#include "workstation/scene/SceneObject.h"
#include "workstation/scene/CadSceneObject.h"
#include "workstation/renderer/Camera.h"

#include <vector>
#include <functional>
#include <cstdint>

namespace workstation {
namespace scene {

enum class SelectionMode {
    Single,
    Add,
    Toggle,
    Remove
};

class SceneSelectionManager {
public:
    SceneSelectionManager() = default;
    ~SceneSelectionManager() = default;

    void SetSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }
    SceneManager* GetSceneManager() const { return sceneManager_; }

    void SetFilter(const SelectionFilter& filter) { filter_ = filter; }
    const SelectionFilter& GetFilter() const { return filter_; }

    void SetPickTolerance(double pixels) { pickTolerance_ = pixels; }
    double GetPickTolerance() const { return pickTolerance_; }

    SelectionResult Pick(int mouseX, int mouseY,
                         int viewportWidth, int viewportHeight,
                         const renderer::Camera& camera,
                         SelectionMode mode = SelectionMode::Single);

    std::vector<SelectionResult> PickMultiple(int mouseX, int mouseY,
                                               int viewportWidth, int viewportHeight,
                                               const renderer::Camera& camera);

    std::vector<SelectionResult> PickInRegion(const std::vector<std::pair<int,int>>& screenPoints,
                                               int viewportWidth, int viewportHeight,
                                               const renderer::Camera& camera);

    void SelectAll();
    void DeselectAll();
    void InvertSelection();

    bool HasSelection() const { return !selectedObjects_.empty(); }
    uint32_t GetSelectionCount() const { return static_cast<uint32_t>(selectedObjects_.size()); }

    const std::vector<SelectionResult>& GetSelections() const { return selectedObjects_; }
    SelectionResult GetPrimarySelection() const;

    std::vector<NodeID> GetSelectedObjectIDs() const;
    std::vector<uint64_t> GetSelectedEntityIDs() const;

    using SelectionCallback = std::function<void(const SelectionResult&)>;
    using MultiSelectionCallback = std::function<void(const std::vector<SelectionResult>&)>;

    void SetSelectionCallback(SelectionCallback cb) { selectionCallback_ = std::move(cb); }
    void SetMultiSelectionCallback(MultiSelectionCallback cb) { multiSelectionCallback_ = std::move(cb); }

private:
    SelectionResult PickPointCloud(const Ray& ray, SceneObject* obj);
    SelectionResult PickCadAttachment(const Ray& ray, SceneObject* obj);
    SelectionResult PickCadLine(const Ray& ray, CadSceneObject* cad);
    SelectionResult PickCadPolyline(const Ray& ray, CadSceneObject* cad);
    SelectionResult PickCadCircle(const Ray& ray, CadSceneObject* cad);
    SelectionResult PickCadSurface(const Ray& ray, CadSceneObject* cad);

    SelectionResult PickCadDxf(const Ray& ray, CadSceneObject* cad);
    SelectionResult PickCadDwg(const Ray& ray, CadSceneObject* cad);
    SelectionResult PickCadSnt(const Ray& ray, CadSceneObject* cad);

    void AddToSelection(const SelectionResult& result);
    void RemoveFromSelection(NodeID objectID);
    bool IsSelected(NodeID objectID) const;

    SceneManager* sceneManager_ = nullptr;
    SelectionFilter filter_;
    double pickTolerance_ = 3.0;
    std::vector<SelectionResult> selectedObjects_;
    SelectionCallback selectionCallback_;
    MultiSelectionCallback multiSelectionCallback_;
};

} // namespace scene
} // namespace workstation
