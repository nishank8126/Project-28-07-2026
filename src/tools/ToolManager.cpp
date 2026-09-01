#include "workstation/tools/ToolManager.h"
#include "imgui.h"

namespace workstation {
namespace tools {

void ToolManager::Initialize() {
    selectionTool_ = std::make_unique<SelectionTool>();
    measurementTool_ = std::make_unique<MeasurementTool>();
    clipTool_ = std::make_unique<ClipTool>();
    sectionTool_ = std::make_unique<SectionTool>();
    crossSectionTool_ = std::make_unique<CrossSectionTool>();
    classificationTool_ = std::make_unique<ClassificationTool>();
    classificationTool_->Initialize();
}

void ToolManager::Shutdown() {
    selectionTool_.reset();
    measurementTool_.reset();
    clipTool_.reset();
    sectionTool_.reset();
    crossSectionTool_.reset();
    classificationTool_.reset();
}

void ToolManager::SetActiveTool(ToolType type) {
    if (activeTool_ == type) {
        activeTool_ = ToolType::None;
    } else {
        activeTool_ = type;
    }
}

const char* ToolManager::GetToolName(ToolType type) const {
    switch (type) {
        case ToolType::Selection:      return "Selection";
        case ToolType::Measurement:    return "Measurement";
        case ToolType::Clip:           return "Clip";
        case ToolType::Section:        return "Section";
        case ToolType::CrossSection:   return "Cross Section";
        case ToolType::Classification: return "Classification";
        case ToolType::None:           return "None";
    }
    return "Unknown";
}

void ToolManager::RenderUI(renderer::RenderContext& ctx) {
    ImGui::Begin("Tools");

    ImGui::Text("Active Tool: %s", GetToolName(activeTool_));
    ImGui::Separator();

    if (ImGui::Button("Selection", ImVec2(-1, 0))) SetActiveTool(ToolType::Selection);
    if (ImGui::Button("Measurement", ImVec2(-1, 0))) SetActiveTool(ToolType::Measurement);
    if (ImGui::Button("Clip", ImVec2(-1, 0))) SetActiveTool(ToolType::Clip);
    if (ImGui::Button("Section", ImVec2(-1, 0))) SetActiveTool(ToolType::Section);
    if (ImGui::Button("Cross Section", ImVec2(-1, 0))) SetActiveTool(ToolType::CrossSection);
    if (ImGui::Button("Classification", ImVec2(-1, 0))) SetActiveTool(ToolType::Classification);

    ImGui::Separator();

    switch (activeTool_) {
        case ToolType::Selection:
            selectionTool_->RenderUI();
            break;
        case ToolType::Measurement:
            measurementTool_->RenderUI();
            break;
        case ToolType::Clip:
            clipTool_->RenderUI();
            break;
        case ToolType::Section:
            sectionTool_->RenderUI();
            break;
        case ToolType::CrossSection:
            crossSectionTool_->RenderUI();
            break;
        case ToolType::Classification:
            classificationTool_->RenderUI();
            break;
        case ToolType::None:
            break;
    }

    ImGui::End();
}

} // namespace tools
} // namespace workstation
