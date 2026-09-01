#include "workstation/tools/CrossSectionTool.h"
#include "imgui.h"

#include <cmath>
#include <algorithm>

namespace workstation {
namespace tools {

void CrossSectionTool::SetAlignment(const math::Point3d& start, const math::Point3d& end) {
    alignStart_ = start;
    alignEnd_ = end;
}

void CrossSectionTool::AddPoint(const math::Point3d& point, uint64_t nodeId, uint32_t classification) {
    CrossSectionPoint csp;
    csp.position = point;
    csp.station = ComputeStation(point);
    csp.offset = ComputeOffset(point);
    csp.elevation = point.z;
    csp.classification = classification;
    csp.nodeId = nodeId;
    allPoints_.push_back(csp);
}

void CrossSectionTool::ClearPoints() {
    allPoints_.clear();
    result_ = CrossSectionResult{};
}

void CrossSectionTool::ComputeProfiles() {
    result_.profiles.clear();
    result_.type = type_;

    double dx = alignEnd_.x - alignStart_.x;
    double dy = alignEnd_.y - alignStart_.y;
    double totalLength = std::sqrt(dx * dx + dy * dy);

    if (totalLength < 1e-10) return;

    uint32_t numProfiles = static_cast<uint32_t>(std::floor(totalLength / spacing_)) + 1;
    result_.profileCount = numProfiles;

    for (uint32_t i = 0; i < numProfiles; ++i) {
        double station = i * spacing_;
        if (station > totalLength) break;

        double t = station / totalLength;
        math::Point3d profileCenter{
            alignStart_.x + t * dx,
            alignStart_.y + t * dy,
            alignStart_.z
        };

        CrossSectionProfile profile;
        profile.station = station;

        for (const auto& pt : allPoints_) {
            double distToCenter = std::sqrt(
                (pt.position.x - profileCenter.x) * (pt.position.x - profileCenter.x) +
                (pt.position.y - profileCenter.y) * (pt.position.y - profileCenter.y) +
                (pt.position.z - profileCenter.z) * (pt.position.z - profileCenter.z));

            if (std::abs(pt.offset) <= width_ * 0.5) {
                profile.points.push_back(pt);

                if (profile.points.size() == 1) {
                    profile.leftOffset = pt.offset;
                    profile.rightOffset = pt.offset;
                    profile.minHeight = pt.elevation;
                    profile.maxHeight = pt.elevation;
                } else {
                    if (pt.offset < profile.leftOffset) profile.leftOffset = pt.offset;
                    if (pt.offset > profile.rightOffset) profile.rightOffset = pt.offset;
                    if (pt.elevation < profile.minHeight) profile.minHeight = pt.elevation;
                    if (pt.elevation > profile.maxHeight) profile.maxHeight = pt.elevation;
                }
            }
        }

        profile.length = profile.rightOffset - profile.leftOffset;
        result_.profiles.push_back(profile);
    }
}

double CrossSectionTool::ComputeStation(const math::Point3d& p) const {
    double dx = alignEnd_.x - alignStart_.x;
    double dy = alignEnd_.y - alignStart_.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-10) return 0.0;

    double t = ((p.x - alignStart_.x) * dx + (p.y - alignStart_.y) * dy) / (len * len);
    return t * len;
}

double CrossSectionTool::ComputeOffset(const math::Point3d& p) const {
    double dx = alignEnd_.x - alignStart_.x;
    double dy = alignEnd_.y - alignStart_.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-10) return 0.0;

    double nx = -dy / len;
    double ny = dx / len;

    return (p.x - alignStart_.x) * nx + (p.y - alignStart_.y) * ny;
}

bool CrossSectionTool::IsPointWithinWidth(const math::Point3d& p) const {
    return std::abs(ComputeOffset(p)) <= width_ * 0.5;
}

void CrossSectionTool::RenderUI() {
    ImGui::Text("Cross Section Tool");
    ImGui::Separator();

    const char* types[] = {"Road", "Terrain", "Railway", "Custom"};
    int currentType = static_cast<int>(type_);
    if (ImGui::Combo("Type", &currentType, types, 4)) {
        type_ = static_cast<CrossSectionType>(currentType);
    }

    float spacing = static_cast<float>(spacing_);
    if (ImGui::SliderFloat("Profile Spacing", &spacing, 1.0f, 100.0f)) {
        spacing_ = spacing;
    }

    float width = static_cast<float>(width_);
    if (ImGui::SliderFloat("Profile Width", &width, 1.0f, 200.0f)) {
        width_ = width;
    }

    ImGui::Separator();
    ImGui::Text("Total Points: %zu", allPoints_.size());
    ImGui::Text("Profiles: %u", result_.profileCount);

    if (ImGui::Button("Compute Profiles")) {
        ComputeProfiles();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearPoints();
    }

    if (!result_.profiles.empty()) {
        ImGui::Separator();
        ImGui::Text("Profile Results:");
        for (size_t i = 0; i < result_.profiles.size() && i < 5; ++i) {
            const auto& p = result_.profiles[i];
            ImGui::Text("  Profile %zu: Station=%.1f, Points=%zu", i, p.station, p.points.size());
        }
        if (result_.profiles.size() > 5) {
            ImGui::Text("  ... and %zu more", result_.profiles.size() - 5);
        }
    }
}

} // namespace tools
} // namespace workstation
