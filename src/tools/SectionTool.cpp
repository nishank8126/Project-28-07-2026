#include "workstation/tools/SectionTool.h"
#include "imgui.h"

#include <cmath>
#include <algorithm>

namespace workstation {
namespace tools {

void SectionTool::SetSectionLine(const math::Point3d& start, const math::Point3d& end) {
    line_.start = start;
    line_.end = end;
    hasSection_ = true;
    points_.clear();
    result_ = SectionResult{};
}

void SectionTool::AddPoint(const math::Point3d& point, uint64_t nodeId) {
    if (!hasSection_) return;

    if (IsPointWithinWidth(point)) {
        SectionPoint sp;
        sp.position = point;
        sp.station = ProjectPointToStation(point);
        sp.elevation = point.z;
        sp.nodeId = nodeId;
        points_.push_back(sp);
    }
}

void SectionTool::ClearPoints() {
    points_.clear();
    result_ = SectionResult{};
}

void SectionTool::ComputeResult() {
    result_.points = points_;

    if (points_.empty()) {
        result_.length = 0.0;
        return;
    }

    double dx = line_.end.x - line_.start.x;
    double dy = line_.end.y - line_.start.y;
    result_.length = std::sqrt(dx * dx + dy * dy);

    double minStation = points_[0].station;
    double maxStation = points_[0].station;
    double minElev = points_[0].elevation;
    double maxElev = points_[0].elevation;

    for (const auto& pt : points_) {
        if (pt.station < minStation) minStation = pt.station;
        if (pt.station > maxStation) maxStation = pt.station;
        if (pt.elevation < minElev) minElev = pt.elevation;
        if (pt.elevation > maxElev) maxElev = pt.elevation;
    }

    result_.minWidth = minStation;
    result_.maxWidth = maxStation;
    result_.minHeight = minElev;
    result_.maxHeight = maxElev;
}

double SectionTool::ProjectPointToStation(const math::Point3d& p) const {
    double dx = line_.end.x - line_.start.x;
    double dy = line_.end.y - line_.start.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-10) return 0.0;

    double t = ((p.x - line_.start.x) * dx + (p.y - line_.start.y) * dy) / (len * len);
    return t * len;
}

double SectionTool::ProjectPointToElevation(const math::Point3d& p) const {
    return p.z;
}

bool SectionTool::IsPointWithinWidth(const math::Point3d& p) const {
    double dx = line_.end.x - line_.start.x;
    double dy = line_.end.y - line_.start.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-10) return false;

    double nx = -dy / len;
    double ny = dx / len;

    double dist = std::abs((p.x - line_.start.x) * nx + (p.y - line_.start.y) * ny);
    return dist <= line_.width * 0.5;
}

void SectionTool::RenderUI() {
    ImGui::Text("Section Tool");
    ImGui::Separator();

    float width = static_cast<float>(width_);
    if (ImGui::SliderFloat("Section Width", &width, 0.1f, 100.0f)) {
        width_ = width;
        line_.width = width_;
    }

    ImGui::Separator();
    ImGui::Text("Has Section: %s", hasSection_ ? "Yes" : "No");

    if (hasSection_) {
        ImGui::Text("Line Length: %.2f m", result_.length);
        ImGui::Text("Points: %zu", points_.size());

        if (!points_.empty()) {
            ImGui::Text("Station Range: [%.2f, %.2f]", result_.minWidth, result_.maxWidth);
            ImGui::Text("Elevation Range: [%.2f, %.2f]", result_.minHeight, result_.maxHeight);
        }
    }

    if (ImGui::Button("Clear Points")) {
        ClearPoints();
    }
}

} // namespace tools
} // namespace workstation
