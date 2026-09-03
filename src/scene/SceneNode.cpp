#include "workstation/scene/SceneNode.h"
#include <cmath>
#include <algorithm>

namespace workstation {
namespace scene {

SceneNode::SceneNode(NodeID id, const std::string& name)
    : id_(id), name_(name) {}

void SceneNode::AddChild(SceneNode* child) {
    if (!child || child == this) return;
    child->SetParent(this);
    children_.push_back(child);
}

void SceneNode::RemoveChild(SceneNode* child) {
    children_.erase(
        std::remove(children_.begin(), children_.end(), child),
        children_.end());
    child->SetParent(nullptr);
}

SceneNode* SceneNode::FindChild(NodeID id) const {
    for (auto* child : children_) {
        if (child->GetID() == id) return child;
        auto* found = child->FindChild(id);
        if (found) return found;
    }
    return nullptr;
}

void SceneNode::UpdateWorldBounds() {
    worldBounds_ = bounds_;
    if (parent_) {
        const auto& pb = parent_->GetWorldBounds();
        worldBounds_.minX += pb.minX;
        worldBounds_.minY += pb.minY;
        worldBounds_.minZ += pb.minZ;
        worldBounds_.maxX += pb.maxX;
        worldBounds_.maxY += pb.maxY;
        worldBounds_.maxZ += pb.maxZ;
    }
}

bool SceneNode::IsEffectivelyVisible() const {
    if (!visible_) return false;
    if (parent_) return parent_->IsEffectivelyVisible();
    return true;
}

} // namespace scene
} // namespace workstation
