#include "workstation/tools/ClassificationTool.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "imgui.h"

#include <cstdio>
#include <utility>

namespace workstation {
namespace tools {

void ClassificationTool::Initialize() {
    classifications_[0] = {0, "Created/Never Classified", true, 0xFFC0C0C0};
    classifications_[1] = {1, "Unclassified", true, 0xFFC0C0C0};
    classifications_[2] = {2, "Ground", true, 0xFF00FF00};
    classifications_[3] = {3, "Low Vegetation", true, 0xFF008000};
    classifications_[4] = {4, "Medium Vegetation", true, 0xFF004000};
    classifications_[5] = {5, "High Vegetation", true, 0xFF002000};
    classifications_[6] = {6, "Building", true, 0xFFFF0000};
    classifications_[7] = {7, "Low Point (Noise)", true, 0xFFFFFF00};
    classifications_[8] = {8, "Reserved/Model Key-Point", true, 0xFF00FFFF};
    classifications_[9] = {9, "Water", true, 0xFF0000FF};
    classifications_[10] = {10, "Rail", true, 0xFFFF00FF};
    classifications_[11] = {11, "Road Surface", true, 0xFF808080};
    classifications_[12] = {12, "Reserved/Overlap", true, 0xFF404040};
    classifications_[13] = {13, "Wire - Guard", true, 0xFFFFFF00};
    classifications_[14] = {14, "Wire - Conductor", true, 0xFFFFFF00};
    classifications_[15] = {15, "Transmission Tower", true, 0xFF800080};
    classifications_[16] = {16, "Wire-Structure Connector", true, 0xFFFFFF00};
    classifications_[17] = {18, "Bridge Deck", true, 0xFF808080};
    classifications_[18] = {19, "High Noise", true, 0xFFFFFF00};
}

void ClassificationTool::NotifyChanged() {
    if (onClassificationChanged_) onClassificationChanged_();
}

void ClassificationTool::AddNodeToEdit(uint64_t nodeId, uint32_t oldClassification) {
    pendingEdits_.push_back({nodeId, oldClassification, newClass_});
}

void ClassificationTool::ClearEdits() {
    pendingEdits_.clear();
}

void ClassificationTool::ApplyEdits() {
    if (targetCloud_ && targetCloud_->Root()) {
        auto& channels = targetCloud_->Root()->channels();
        for (const auto& e : pendingEdits_) {
            channels.WriteClassification(static_cast<size_t>(e.nodeId),
                                          static_cast<uint8_t>(e.newClassification));
        }
        NotifyChanged();
    }
    appliedEdits_.insert(appliedEdits_.end(), pendingEdits_.begin(), pendingEdits_.end());
    pendingEdits_.clear();
}

void ClassificationTool::UndoEdits() {
    pendingEdits_.clear();
}

void ClassificationTool::RunIsolatedPoints() {
    if (!targetCloud_) { lastRunStatus_ = "No point cloud loaded"; return; }
    auto result = ClassifyIsolatedPoints(*targetCloud_, isolatedParams_);
    char buf[128];
    snprintf(buf, sizeof(buf), "Isolated Points: %zu points reclassified", result.PointsChanged());
    lastRunStatus_ = buf;
    if (result.PointsChanged() > 0) {
        appliedAlgoResults_.push_back({"Isolated Points", std::move(result)});
        NotifyChanged();
    }
}

void ClassificationTool::RunLowPoints() {
    if (!targetCloud_) { lastRunStatus_ = "No point cloud loaded"; return; }
    auto result = ClassifyLowPoints(*targetCloud_, lowParams_);
    char buf[128];
    snprintf(buf, sizeof(buf), "Low Points: %zu points reclassified", result.PointsChanged());
    lastRunStatus_ = buf;
    if (result.PointsChanged() > 0) {
        appliedAlgoResults_.push_back({"Low Points", std::move(result)});
        NotifyChanged();
    }
}

void ClassificationTool::RunGroundPTD() {
    if (!targetCloud_) { lastRunStatus_ = "No point cloud loaded"; return; }
    auto result = ClassifyGroundPTD(*targetCloud_, groundParams_);
    char buf[128];
    snprintf(buf, sizeof(buf), "Ground (PTD): %zu points reclassified", result.PointsChanged());
    lastRunStatus_ = buf;
    if (result.PointsChanged() > 0) {
        appliedAlgoResults_.push_back({"Ground (PTD)", std::move(result)});
        NotifyChanged();
    }
}

void ClassificationTool::UndoLastAlgorithm() {
    if (appliedAlgoResults_.empty() || !targetCloud_) return;
    UndoClassifyResult(*targetCloud_, appliedAlgoResults_.back().result);
    lastRunStatus_ = "Undid: " + appliedAlgoResults_.back().label;
    appliedAlgoResults_.pop_back();
    NotifyChanged();
}

void ClassificationTool::RenderUI() {
    ImGui::Text("Classification Tool");
    ImGui::Separator();

    if (!targetCloud_) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "No point cloud loaded.");
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Automated Classification", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Reads/writes the standard ASPRS classification code per point.");

        ImGui::Separator();
        ImGui::Text("Isolated Points");
        float isoRadius = static_cast<float>(isolatedParams_.radius);
        if (ImGui::SliderFloat("Radius##iso", &isoRadius, 0.1f, 50.0f)) isolatedParams_.radius = isoRadius;
        int isoMinN = isolatedParams_.minNeighbors;
        if (ImGui::SliderInt("Min Neighbors##iso", &isoMinN, 0, 20)) isolatedParams_.minNeighbors = isoMinN;
        if (ImGui::Button("Run Isolated Points")) RunIsolatedPoints();

        ImGui::Separator();
        ImGui::Text("Low Points");
        float lowRadius = static_cast<float>(lowParams_.radius);
        if (ImGui::SliderFloat("Radius##low", &lowRadius, 0.1f, 100.0f)) lowParams_.radius = lowRadius;
        float lowHeight = static_cast<float>(lowParams_.heightThreshold);
        if (ImGui::SliderFloat("Height Threshold##low", &lowHeight, 0.05f, 20.0f)) lowParams_.heightThreshold = lowHeight;
        if (ImGui::Button("Run Low Points")) RunLowPoints();

        ImGui::Separator();
        ImGui::Text("Ground (Progressive TIN Densification)");
        float gCell = static_cast<float>(groundParams_.gridCellSize);
        if (ImGui::SliderFloat("Grid Cell Size##gnd", &gCell, 0.5f, 100.0f)) groundParams_.gridCellSize = gCell;
        float gInitDist = static_cast<float>(groundParams_.initialDistance);
        if (ImGui::SliderFloat("Initial Distance##gnd", &gInitDist, 0.05f, 10.0f)) groundParams_.initialDistance = gInitDist;
        float gIterDist = static_cast<float>(groundParams_.iterationDistance);
        if (ImGui::SliderFloat("Iteration Distance##gnd", &gIterDist, 0.05f, 5.0f)) groundParams_.iterationDistance = gIterDist;
        float gAngle = static_cast<float>(groundParams_.angleThresholdDeg);
        if (ImGui::SliderFloat("Angle Threshold (deg)##gnd", &gAngle, 1.0f, 45.0f)) groundParams_.angleThresholdDeg = gAngle;
        int gIters = groundParams_.maxIterations;
        if (ImGui::SliderInt("Max Iterations##gnd", &gIters, 1, 20)) groundParams_.maxIterations = gIters;
        if (ImGui::Button("Run Ground Classification")) RunGroundPTD();

        ImGui::Separator();
        if (!lastRunStatus_.empty()) {
            ImGui::TextWrapped("%s", lastRunStatus_.c_str());
        }
        ImGui::Text("Algorithm History: %zu", appliedAlgoResults_.size());
        if (!appliedAlgoResults_.empty()) {
            if (ImGui::Button("Undo Last Algorithm Pass")) UndoLastAlgorithm();
        }
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Manual Edit")) {
        ImGui::Text("Target Classification:");
        int target = static_cast<int>(target_);
        ImGui::SliderInt("##target", &target, 0, 255);
        target_ = static_cast<uint32_t>(target);

        ImGui::Text("New Classification:");
        int newClass = static_cast<int>(newClass_);
        ImGui::SliderInt("##newClass", &newClass, 0, 255);
        newClass_ = static_cast<uint32_t>(newClass);

        ImGui::Text("Pending Edits: %zu", pendingEdits_.size());
        ImGui::Text("Applied Edits: %zu", appliedEdits_.size());

        if (ImGui::Button("Apply Edits")) {
            ApplyEdits();
        }
        ImGui::SameLine();
        if (ImGui::Button("Undo")) {
            UndoEdits();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            ClearEdits();
        }
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Classes")) {
        if (ImGui::BeginChild("ClassList", ImVec2(0, 200))) {
            for (const auto& [code, info] : classifications_) {
                ImGui::PushID(code);

                ImVec4 color(((info.color >> 16) & 0xFF) / 255.0f,
                            ((info.color >> 8) & 0xFF) / 255.0f,
                            (info.color & 0xFF) / 255.0f,
                            ((info.color >> 24) & 0xFF) / 255.0f);
                ImGui::ColorButton("##color", color, ImGuiColorEditFlags_NoPicker, ImVec2(12, 12));
                ImGui::SameLine();

                bool visible = info.visible;
                if (ImGui::Checkbox("##vis", &visible)) {
                    classifications_[code].visible = visible;
                }
                ImGui::SameLine();

                ImGui::Text("[%u] %s", code, info.name.c_str());

                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
}

} // namespace tools
} // namespace workstation
