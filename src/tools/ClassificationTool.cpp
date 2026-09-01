#include "workstation/tools/ClassificationTool.h"
#include "imgui.h"

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

void ClassificationTool::AddNodeToEdit(uint64_t nodeId, uint32_t oldClassification) {
    pendingEdits_.push_back({nodeId, oldClassification, newClass_});
}

void ClassificationTool::ClearEdits() {
    pendingEdits_.clear();
}

void ClassificationTool::ApplyEdits() {
    appliedEdits_.insert(appliedEdits_.end(), pendingEdits_.begin(), pendingEdits_.end());
    pendingEdits_.clear();
}

void ClassificationTool::UndoEdits() {
    pendingEdits_.clear();
}

void ClassificationTool::RenderUI() {
    ImGui::Text("Classification Tool");
    ImGui::Separator();

    ImGui::Text("Target Classification:");
    int target = static_cast<int>(target_);
    ImGui::SliderInt("##target", &target, 0, 255);
    target_ = static_cast<uint32_t>(target);

    ImGui::Text("New Classification:");
    int newClass = static_cast<int>(newClass_);
    ImGui::SliderInt("##newClass", &newClass, 0, 255);
    newClass_ = static_cast<uint32_t>(newClass);

    ImGui::Separator();
    ImGui::Text("Classifications:");
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

    ImGui::Separator();
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

} // namespace tools
} // namespace workstation
