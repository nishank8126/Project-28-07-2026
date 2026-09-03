#include "workstation/tools/MeasurementTool.h"
#include "imgui.h"

#include <cmath>
#include <sstream>
#include <iomanip>

namespace workstation {
namespace tools {

void MeasurementTool::AddPoint(const math::Point3d& point, uint64_t nodeId) {
    points_.push_back({point, nodeId});
    ComputeResult();
}

void MeasurementTool::UndoLastPoint() {
    if (!points_.empty()) {
        points_.pop_back();
        ComputeResult();
    }
}

void MeasurementTool::ClearPoints() {
    points_.clear();
    result_ = MeasurementResult{};
    result_.mode = mode_;
}

void MeasurementTool::ComputeResult() {
    result_.mode = mode_;
    result_.points = points_;

    if (points_.empty()) {
        result_.distance = 0.0;
        result_.area = 0.0;
        result_.heightDiff = 0.0;
        return;
    }

    switch (mode_) {
        case MeasurementMode::Distance: {
            result_.distance = 0.0;
            for (size_t i = 1; i < points_.size(); ++i) {
                result_.distance += ComputeDistance(points_[i - 1].position, points_[i].position);
            }
            break;
        }
        case MeasurementMode::Area: {
            result_.area = 0.0;
            if (points_.size() >= 3) {
                for (size_t i = 1; i + 1 < points_.size(); ++i) {
                    result_.area += ComputeTriangleArea(
                        points_[0].position, points_[i].position, points_[i + 1].position);
                }
            }
            break;
        }
        case MeasurementMode::Height: {
            if (points_.size() >= 2) {
                double minZ = points_[0].position.z;
                double maxZ = points_[0].position.z;
                for (const auto& pt : points_) {
                    if (pt.position.z < minZ) minZ = pt.position.z;
                    if (pt.position.z > maxZ) maxZ = pt.position.z;
                }
                result_.heightDiff = maxZ - minZ;
            }
            break;
        }
        case MeasurementMode::Volume: {
            // Compute volume using the prism method: for a closed polygon
            // footprint (xy), sum the signed volumes of vertical prisms.
            // Requires at least 3 points to form the base polygon.
            result_.volume = 0.0;
            if (points_.size() >= 3) {
                // Average elevation of all points as the reference base
                double baseZ = 0.0;
                for (const auto& pt : points_) baseZ += pt.position.z;
                baseZ /= static_cast<double>(points_.size());

                // Signed area and volume using the divergence theorem
                double signedArea = 0.0;
                double signedVolume = 0.0;
                size_t n = points_.size();
                for (size_t i = 0; i < n; ++i) {
                    size_t j = (i + 1) % n;
                    const auto& pi = points_[i].position;
                    const auto& pj = points_[j].position;
                    double cross = pi.x * pj.y - pj.x * pi.y;
                    signedArea += cross;
                    signedVolume += cross * (pi.z + pj.z) / 2.0;
                }
                signedArea *= 0.5;
                if (std::abs(signedArea) > 1e-12) {
                    // Volume relative to base elevation
                    result_.volume = std::abs(signedVolume) - std::abs(signedArea) * baseZ;
                    result_.volume = std::abs(result_.volume);
                }
            }
            break;
        }
        case MeasurementMode::None:
            break;
    }
}

double MeasurementTool::ComputeDistance(const math::Point3d& a, const math::Point3d& b) const {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double MeasurementTool::ComputeTriangleArea(const math::Point3d& a, const math::Point3d& b, const math::Point3d& c) const {
    double abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
    double acx = c.x - a.x, acy = c.y - a.y, acz = c.z - a.z;

    double cx = aby * acz - abz * acy;
    double cy = abz * acx - abx * acz;
    double cz = abx * acy - aby * acx;

    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

void MeasurementTool::RenderUI() {
    ImGui::Text("Measurement Tool");
    ImGui::Separator();

    const char* modes[] = {"Distance", "Area", "Height", "Volume"};
    int currentMode = static_cast<int>(mode_);
    if (ImGui::Combo("Mode", &currentMode, modes, 4)) {
        mode_ = static_cast<MeasurementMode>(currentMode);
        ClearPoints();
    }

    ImGui::Separator();
    ImGui::Text("Points: %zu", points_.size());

    if (ImGui::Button("Undo Point")) {
        UndoLastPoint();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearPoints();
    }

    ImGui::Separator();
    if (mode_ == MeasurementMode::Distance) {
        ImGui::Text("Distance: %.3f m", result_.distance);
    } else if (mode_ == MeasurementMode::Area) {
        ImGui::Text("Area: %.3f m\u00B2", result_.area);
    } else if (mode_ == MeasurementMode::Height) {
        ImGui::Text("Height Diff: %.3f m", result_.heightDiff);
    } else if (mode_ == MeasurementMode::Volume) {
        ImGui::Text("Volume: %.3f m\u00B3", result_.volume);
    }
}

} // namespace tools
} // namespace workstation
