#pragma once
#include "workstation/spatial/BoundingBox.h"
#include <string>
#include <vector>
#include <cstdint>

namespace workstation {
namespace scene {

using NodeID = uint64_t;

struct Vec3d {
    double x = 0.0, y = 0.0, z = 0.0;
};

struct Mat4d {
    double m[16] = {};
    static Mat4d Identity() {
        Mat4d r = {};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
        return r;
    }
};

struct Transform {
    Mat4d matrix = Mat4d::Identity();
    Vec3d position = {0.0, 0.0, 0.0};
    Vec3d scale = {1.0, 1.0, 1.0};
    Vec3d rotation = {0.0, 0.0, 0.0};
};

class SceneNode {
public:
    SceneNode() = default;
    explicit SceneNode(NodeID id, const std::string& name = "");
    ~SceneNode() = default;

    NodeID GetID() const { return id_; }
    void SetID(NodeID id) { id_ = id; }

    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    bool IsVisible() const { return visible_; }
    void SetVisible(bool v) { visible_ = v; }

    bool IsSelected() const { return selected_; }
    void SetSelected(bool s) { selected_ = s; }

    void SetTransform(const Transform& t) { transform_ = t; dirty_ = true; }
    const Transform& GetTransform() const { return transform_; }
    Transform& GetTransform() { return transform_; }

    void SetBounds(const spatial::BoundingBox& b) { bounds_ = b; }
    const spatial::BoundingBox& GetBounds() const { return bounds_; }

    void SetWorldBounds(const spatial::BoundingBox& b) { worldBounds_ = b; }
    const spatial::BoundingBox& GetWorldBounds() const { return worldBounds_; }

    SceneNode* GetParent() const { return parent_; }
    void SetParent(SceneNode* p) { parent_ = p; }

    const std::vector<SceneNode*>& GetChildren() const { return children_; }
    void AddChild(SceneNode* child);
    void RemoveChild(SceneNode* child);
    SceneNode* FindChild(NodeID id) const;

    void SetLayerID(uint32_t id) { layerID_ = id; }
    uint32_t GetLayerID() const { return layerID_; }

    void SetObjectType(uint32_t type) { objectType_ = type; }
    uint32_t GetObjectType() const { return objectType_; }

    bool IsDirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }
    void MarkDirty() { dirty_ = true; }

    void UpdateWorldBounds();
    bool IsEffectivelyVisible() const;

private:
    NodeID id_ = 0;
    std::string name_;
    bool visible_ = true;
    bool selected_ = false;
    bool dirty_ = true;

    Transform transform_;
    spatial::BoundingBox bounds_;
    spatial::BoundingBox worldBounds_;

    SceneNode* parent_ = nullptr;
    std::vector<SceneNode*> children_;

    uint32_t layerID_ = 0;
    uint32_t objectType_ = 0;
};

} // namespace scene
} // namespace workstation
