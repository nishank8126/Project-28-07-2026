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

    drawPoint_ = std::make_unique<PointDrawTool>();
    drawLine_ = std::make_unique<LineDrawTool>();
    drawPolyline_ = std::make_unique<PolylineDrawTool>();
    drawPolygon_ = std::make_unique<PolygonDrawTool>();
    drawRect_ = std::make_unique<RectangleDrawTool>();
    drawCircle_ = std::make_unique<CircleDrawTool>();
    drawArc_ = std::make_unique<ArcDrawTool>();
    drawText_ = std::make_unique<TextDrawTool>();
    drawDim_ = std::make_unique<DimensionDrawTool>();
}

void ToolManager::Shutdown() {
    selectionTool_.reset();
    measurementTool_.reset();
    clipTool_.reset();
    sectionTool_.reset();
    crossSectionTool_.reset();
    classificationTool_.reset();
    drawPoint_.reset(); drawLine_.reset(); drawPolyline_.reset();
    drawPolygon_.reset(); drawRect_.reset(); drawCircle_.reset();
    drawArc_.reset(); drawText_.reset(); drawDim_.reset();
}

void ToolManager::SetActiveTool(ToolType type) {
    if (activeTool_ == type) {
        activeTool_ = ToolType::None;
    } else {
        activeTool_ = type;
    }
}

CadDrawTool* ToolManager::GetDrawTool() {
    switch (activeTool_) {
        case ToolType::DrawPoint:      return drawPoint_.get();
        case ToolType::DrawLine:       return drawLine_.get();
        case ToolType::DrawPolyline:   return drawPolyline_.get();
        case ToolType::DrawPolygon:    return drawPolygon_.get();
        case ToolType::DrawRectangle:  return drawRect_.get();
        case ToolType::DrawCircle:     return drawCircle_.get();
        case ToolType::DrawArc:        return drawArc_.get();
        case ToolType::DrawText:       return drawText_.get();
        case ToolType::DrawDimension:  return drawDim_.get();
        default: return nullptr;
    }
}

const char* ToolManager::GetToolName(ToolType type) const {
    switch (type) {
        case ToolType::Selection:       return "Selection";
        case ToolType::Measurement:     return "Measurement";
        case ToolType::Clip:            return "Clip";
        case ToolType::Section:         return "Section";
        case ToolType::CrossSection:    return "Cross Section";
        case ToolType::Classification:  return "Classification";
        case ToolType::DrawPoint:       return "Draw Point";
        case ToolType::DrawLine:        return "Draw Line";
        case ToolType::DrawPolyline:    return "Draw Polyline";
        case ToolType::DrawPolygon:     return "Draw Polygon";
        case ToolType::DrawRectangle:   return "Draw Rectangle";
        case ToolType::DrawCircle:      return "Draw Circle";
        case ToolType::DrawArc:         return "Draw Arc";
        case ToolType::DrawText:        return "Draw Text";
        case ToolType::DrawDimension:   return "Draw Dimension";
        case ToolType::None:            return "None";
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
    ImGui::Text("CAD Drawing");
    if (ImGui::Button("Point", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawPoint);
    if (ImGui::Button("Line", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawLine);
    if (ImGui::Button("Polyline", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawPolyline);
    if (ImGui::Button("Polygon", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawPolygon);
    if (ImGui::Button("Rectangle", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawRectangle);
    if (ImGui::Button("Circle", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawCircle);
    if (ImGui::Button("Arc", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawArc);
    if (ImGui::Button("Text", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawText);
    if (ImGui::Button("Dimension", ImVec2(-1, 0))) SetActiveTool(ToolType::DrawDimension);

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
        default: break;
    }

    if (!drawnGeometries_.empty()) {
        ImGui::Separator();
        ImGui::Text("Drawn: %zu geometries", drawnGeometries_.size());
        if (ImGui::Button("Clear All")) drawnGeometries_.clear();
    }

    ImGui::End();
}

} // namespace tools
} // namespace workstation
