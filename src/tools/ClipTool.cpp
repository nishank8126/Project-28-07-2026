#include "workstation/tools/ClipTool.h"
#include "imgui.h"

namespace workstation {
namespace tools {

void ClipTool::EnablePlane(ClipPlaneAxis axis, double position) {
    plane_.axis = axis;
    plane_.normal = ComputePlaneNormal(axis);
    plane_.point = math::Point3d{0.0, 0.0, 0.0};

    switch (axis) {
        case ClipPlaneAxis::X: plane_.point.x = position; break;
        case ClipPlaneAxis::Y: plane_.point.y = position; break;
        case ClipPlaneAxis::Z: plane_.point.z = position; break;
        case ClipPlaneAxis::Custom: break;
    }

    plane_.enabled = true;
}

void ClipTool::DisablePlane() {
    plane_.enabled = false;
}

void ClipTool::SetPlanePosition(double position) {
    switch (plane_.axis) {
        case ClipPlaneAxis::X: plane_.point.x = position; break;
        case ClipPlaneAxis::Y: plane_.point.y = position; break;
        case ClipPlaneAxis::Z: plane_.point.z = position; break;
        case ClipPlaneAxis::Custom: break;
    }
}

double ClipTool::GetPlanePosition() const {
    switch (plane_.axis) {
        case ClipPlaneAxis::X: return plane_.point.x;
        case ClipPlaneAxis::Y: return plane_.point.y;
        case ClipPlaneAxis::Z: return plane_.point.z;
        case ClipPlaneAxis::Custom: return 0.0;
    }
    return 0.0;
}

void ClipTool::EnableBox(const spatial::BoundingBox& box) {
    box_.bounds = box;
    box_.enabled = true;
}

void ClipTool::DisableBox() {
    box_.enabled = false;
}

void ClipTool::SetBoxBounds(const spatial::BoundingBox& box) {
    box_.bounds = box;
}

bool ClipTool::IsPointClipped(const math::Point3d& point) const {
    if (plane_.enabled) {
        double dist = plane_.normal.x * (point.x - plane_.point.x) +
                      plane_.normal.y * (point.y - plane_.point.y) +
                      plane_.normal.z * (point.z - plane_.point.z);
        bool inside = dist >= 0.0;
        if (flip_) inside = !inside;
        if (!inside) return true;
    }

    if (box_.enabled) {
        const auto& b = box_.bounds;
        if (point.x < b.minX || point.x > b.maxX ||
            point.y < b.minY || point.y > b.maxY ||
            point.z < b.minZ || point.z > b.maxZ) {
            return true;
        }
    }

    return false;
}

bool ClipTool::IsNodeClipped(const math::Point3d& nodeCenter, double nodeRadius) const {
    if (plane_.enabled) {
        double dist = plane_.normal.x * (nodeCenter.x - plane_.point.x) +
                      plane_.normal.y * (nodeCenter.y - plane_.point.y) +
                      plane_.normal.z * (nodeCenter.z - plane_.point.z);
        bool inside = dist >= -nodeRadius;
        if (flip_) inside = !inside;
        if (!inside) return true;
    }

    if (box_.enabled) {
        const auto& b = box_.bounds;
        if (nodeCenter.x + nodeRadius < b.minX || nodeCenter.x - nodeRadius > b.maxX ||
            nodeCenter.y + nodeRadius < b.minY || nodeCenter.y - nodeRadius > b.maxY ||
            nodeCenter.z + nodeRadius < b.minZ || nodeCenter.z - nodeRadius > b.maxZ) {
            return true;
        }
    }

    return false;
}

math::Point3d ClipTool::ComputePlaneNormal(ClipPlaneAxis axis) const {
    switch (axis) {
        case ClipPlaneAxis::X: return math::Point3d{1.0, 0.0, 0.0};
        case ClipPlaneAxis::Y: return math::Point3d{0.0, 1.0, 0.0};
        case ClipPlaneAxis::Z: return math::Point3d{0.0, 0.0, 1.0};
        case ClipPlaneAxis::Custom: return math::Point3d{0.0, 0.0, 1.0};
    }
    return math::Point3d{0.0, 0.0, 1.0};
}

void ClipTool::RenderUI() {
    ImGui::Text("Clip Tool");
    ImGui::Separator();

    bool planeEnabled = plane_.enabled;
    if (ImGui::Checkbox("Enable Plane Clip", &planeEnabled)) {
        if (planeEnabled) {
            EnablePlane(ClipPlaneAxis::Z, 0.0);
        } else {
            DisablePlane();
        }
    }

    if (plane_.enabled) {
        const char* axes[] = {"X", "Y", "Z"};
        int currentAxis = static_cast<int>(plane_.axis);
        if (ImGui::Combo("Plane Axis", &currentAxis, axes, 3)) {
            plane_.axis = static_cast<ClipPlaneAxis>(currentAxis);
            plane_.normal = ComputePlaneNormal(plane_.axis);
        }

        float pos = static_cast<float>(GetPlanePosition());
        if (ImGui::SliderFloat("Position", &pos, -1000.0f, 1000.0f)) {
            SetPlanePosition(pos);
        }

        ImGui::Checkbox("Flip Direction", &flip_);
    }

    ImGui::Separator();

    bool boxEnabled = box_.enabled;
    if (ImGui::Checkbox("Enable Box Clip", &boxEnabled)) {
        if (boxEnabled) {
            spatial::BoundingBox box{-500, -500, -500, 500, 500, 500};
            EnableBox(box);
        } else {
            DisableBox();
        }
    }

    if (box_.enabled) {
        ImGui::Text("Box Bounds:");
        ImGui::Text("  X: [%.1f, %.1f]", box_.bounds.minX, box_.bounds.maxX);
        ImGui::Text("  Y: [%.1f, %.1f]", box_.bounds.minY, box_.bounds.maxY);
        ImGui::Text("  Z: [%.1f, %.1f]", box_.bounds.minZ, box_.bounds.maxZ);
    }
}

} // namespace tools
} // namespace workstation
