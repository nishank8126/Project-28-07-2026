#include "workstation/tools/SelectionTool.h"
#include "imgui.h"

#include <cmath>

namespace workstation {
namespace tools {

void SelectionTool::BeginSelection(const math::Point3d& origin) {
    start_ = origin;
    current_ = origin;
    selecting_ = true;
    result_.selectedNodeIds.clear();
    result_.selectedPositions.clear();
    result_.totalSelectedPoints = 0;
}

void SelectionTool::UpdateSelection(const math::Point3d& current) {
    if (!selecting_) return;
    current_ = current;
}

void SelectionTool::EndSelection(const math::Point3d& end) {
    if (!selecting_) return;
    current_ = end;
    selecting_ = false;

    result_.selectionBounds = ComputeAABB();

    if (callback_.onSelectionChanged) {
        callback_.onSelectionChanged(result_);
    }
}

void SelectionTool::CancelSelection() {
    selecting_ = false;
    ClearSelection();
}

void SelectionTool::AddPolygonPoint(const math::Point3d& point) {
    if (mode_ == SelectionMode::Polygon) {
        polygonPoints_.push_back(point);
    }
}

void SelectionTool::FinishPolygon() {
    if (polygonPoints_.size() >= 3) {
        result_.selectionBounds = ComputeAABB();
        if (callback_.onSelectionChanged) {
            callback_.onSelectionChanged(result_);
        }
    }
    polygonPoints_.clear();
}

void SelectionTool::ClearSelection() {
    result_.selectedNodeIds.clear();
    result_.selectedPositions.clear();
    result_.totalSelectedPoints = 0;
    result_.selectionBounds = spatial::BoundingBox{};
    polygonPoints_.clear();

    if (callback_.onSelectionCleared) {
        callback_.onSelectionCleared();
    }
}

spatial::BoundingBox SelectionTool::ComputeAABB() const {
    spatial::BoundingBox box;

    if (mode_ == SelectionMode::Polygon && !polygonPoints_.empty()) {
        box.minX = box.maxX = polygonPoints_[0].x;
        box.minY = box.maxY = polygonPoints_[0].y;
        box.minZ = box.maxZ = polygonPoints_[0].z;

        for (const auto& p : polygonPoints_) {
            if (p.x < box.minX) box.minX = p.x;
            if (p.x > box.maxX) box.maxX = p.x;
            if (p.y < box.minY) box.minY = p.y;
            if (p.y > box.maxY) box.maxY = p.y;
            if (p.z < box.minZ) box.minZ = p.z;
            if (p.z > box.maxZ) box.maxZ = p.z;
        }
    } else {
        box.minX = std::min(start_.x, current_.x);
        box.maxX = std::max(start_.x, current_.x);
        box.minY = std::min(start_.y, current_.y);
        box.maxY = std::max(start_.y, current_.y);
        box.minZ = std::min(start_.z, current_.z);
        box.maxZ = std::max(start_.z, current_.z);
    }

    return box;
}

bool SelectionTool::IsPointInPolygon(const math::Point3d& p) const {
    if (polygonPoints_.size() < 3) return false;

    int n = static_cast<int>(polygonPoints_.size());
    bool inside = false;

    for (int i = 0, j = n - 1; i < n; j = i++) {
        const auto& pi = polygonPoints_[i];
        const auto& pj = polygonPoints_[j];

        if (((pi.y > p.y) != (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }

    return inside;
}

bool SelectionTool::IsPointInBox(const math::Point3d& p, const spatial::BoundingBox& box) const {
    return p.x >= box.minX && p.x <= box.maxX &&
           p.y >= box.minY && p.y <= box.maxY &&
           p.z >= box.minZ && p.z <= box.maxZ;
}

void SelectionTool::RenderUI() {
    ImGui::Text("Selection Tool");
    ImGui::Separator();

    const char* modes[] = {"Point", "Rectangle", "Polygon", "Box", "Radius"};
    int currentMode = static_cast<int>(mode_);
    if (ImGui::Combo("Mode", &currentMode, modes, 5)) {
        mode_ = static_cast<SelectionMode>(currentMode);
    }

    if (mode_ == SelectionMode::Radius) {
        float r = static_cast<float>(radius_);
        if (ImGui::SliderFloat("Radius", &r, 0.1f, 1000.0f)) {
            radius_ = r;
        }
    }

    ImGui::Separator();
    ImGui::Text("Selected Points: %llu", result_.totalSelectedPoints);
    ImGui::Text("Selected Nodes: %zu", result_.selectedNodeIds.size());

    if (ImGui::Button("Clear Selection")) {
        ClearSelection();
    }
}

} // namespace tools
} // namespace workstation
